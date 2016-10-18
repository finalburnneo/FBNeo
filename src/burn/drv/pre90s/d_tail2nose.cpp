// FB Alpha Tail to Nose driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2608.h"
#include "konamiic.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvSndROM;
static UINT8 *DrvISndROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZoomRAM;
static UINT8 *DrvZoomRAMExp;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;

static UINT32  *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 DrvJoy1[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[1];

static UINT8 *soundlatch;
static UINT8 *char_bank;
static UINT8 *pal_bank;
static UINT8 *video_enable;
static UINT8 *DrvZ80Bank;

static INT32 redraw_zoom_tiles;

static struct BurnInputInfo Tail2nosInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 11,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 14,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Tail2nos)

static struct BurnDIPInfo Tail2nosDIPList[]=
{
	{0x09, 0xff, 0xff, 0x00, NULL			},
	{0x0a, 0xff, 0xff, 0x50, NULL			},

	{0   , 0xfe, 0   ,   15, "Coin A"		},
	{0x09, 0x01, 0x0f, 0x09, "5 Coins 1 Credits"	},
	{0x09, 0x01, 0x0f, 0x08, "4 Coins 1 Credits"	},
	{0x09, 0x01, 0x0f, 0x07, "3 Coins 1 Credits"	},
	{0x09, 0x01, 0x0f, 0x06, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0x0f, 0x0b, "6 Coins/4 Credits"	},
	{0x09, 0x01, 0x0f, 0x0c, "4 Coins 3 Credits"	},
	{0x09, 0x01, 0x0f, 0x00, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0x0f, 0x0d, "5 Coins/6 Credits"	},
	{0x09, 0x01, 0x0f, 0x0e, "4 Coins 5 Credits"	},
	{0x09, 0x01, 0x0f, 0x0a, "2 Coins 3 Credits"	},
	{0x09, 0x01, 0x0f, 0x01, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0x0f, 0x02, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0x0f, 0x03, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0x0f, 0x04, "1 Coin  5 Credits"	},
	{0x09, 0x01, 0x0f, 0x05, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,   15, "Coin B"		},
	{0x09, 0x01, 0xf0, 0x90, "5 Coins 1 Credits"	},
	{0x09, 0x01, 0xf0, 0x80, "4 Coins 1 Credits"	},
	{0x09, 0x01, 0xf0, 0x70, "3 Coins 1 Credits"	},
	{0x09, 0x01, 0xf0, 0x60, "2 Coins 1 Credits"	},
	{0x09, 0x01, 0xf0, 0xb0, "6 Coins/4 Credits"	},
	{0x09, 0x01, 0xf0, 0xc0, "4 Coins 3 Credits"	},
	{0x09, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"	},
	{0x09, 0x01, 0xf0, 0xd0, "5 Coins/6 Credits"	},
	{0x09, 0x01, 0xf0, 0xe0, "4 Coins 5 Credits"	},
	{0x09, 0x01, 0xf0, 0xa0, "2 Coins 3 Credits"	},
	{0x09, 0x01, 0xf0, 0x10, "1 Coin  2 Credits"	},
	{0x09, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"	},
	{0x09, 0x01, 0xf0, 0x30, "1 Coin  4 Credits"	},
	{0x09, 0x01, 0xf0, 0x40, "1 Coin  5 Credits"	},
	{0x09, 0x01, 0xf0, 0x50, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0a, 0x01, 0x03, 0x01, "Easy"			},
	{0x0a, 0x01, 0x03, 0x00, "Normal"		},
	{0x0a, 0x01, 0x03, 0x02, "Hard"			},
	{0x0a, 0x01, 0x03, 0x03, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0a, 0x01, 0x04, 0x04, "Off"			},
	{0x0a, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x08, 0x00, "Off"			},
	{0x0a, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    2, "Game Mode"		},
	{0x0a, 0x01, 0x10, 0x10, "Single"		},
	{0x0a, 0x01, 0x10, 0x00, "Multiple"		},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x0a, 0x01, 0x20, 0x00, "Off"			},
//	{0x0a, 0x01, 0x20, 0x20, "On"			},

	{0   , 0xfe, 0   ,    2, "Control Panel"	},
	{0x0a, 0x01, 0x40, 0x40, "Standard"		},
	{0x0a, 0x01, 0x40, 0x00, "Original"		},

	{0   , 0xfe, 0   ,    2, "Country"		},
	{0x0a, 0x01, 0x80, 0x00, "Domestic"		},
	{0x0a, 0x01, 0x80, 0x80, "Overseas"		},
};

STDDIPINFO(Tail2nos)

static inline void graphics_ram_expand_one(INT32 offset)
{
	offset &= 0x1fffe;

	DrvZoomRAMExp[offset * 2 + 3] = DrvZoomRAM[offset + 0] & 0x0f;
	DrvZoomRAMExp[offset * 2 + 2] = DrvZoomRAM[offset + 0] >> 4;
	DrvZoomRAMExp[offset * 2 + 1] = DrvZoomRAM[offset + 1] & 0x0f;
	DrvZoomRAMExp[offset * 2 + 0] = DrvZoomRAM[offset + 1] >> 4;

	redraw_zoom_tiles = 1;
}

static inline void palette_update(INT32 offset)
{
	INT32 p = *((UINT16*)(DrvPalRAM + offset));

	INT32 r = (p >> 10) & 0x1f;
	INT32 g = (p >>  5) & 0x1f;
	INT32 b = (p >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset/2] = BurnHighCol(r, g, b, 0);
}

static void __fastcall tail2nose_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffe0000) == 0x400000) {
		UINT16 *p = (UINT16*)DrvZoomRAM;
		if (p[(address & 0x1ffff)/2] != data) {
			p[(address & 0x1ffff)/2] = data;
			graphics_ram_expand_one(address);
		}
		return;
	}

	if ((address & 0xffff000) == 0x500000) {
		K051316Write(0, (address & 0xffe)/2, data & 0xff);
		return;
	}

	if ((address & 0xfffffe0) == 0x510000) {
		K051316WriteCtrl(0, (address & 0x1e)/2, data);
		return;
	}

	if ((address & 0xffff000) == 0xffe000) {
		*((UINT16*)(DrvPalRAM + (address & 0xffe))) = data;
		palette_update(address & 0xffe);
		return;
	}
}

static void __fastcall tail2nose_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffe0000) == 0x400000) {
		if (DrvZoomRAM[(address & 0x1ffff)^1] != data) {
			DrvZoomRAM[(address & 0x1ffff)^1] = data;
			graphics_ram_expand_one(address);
		}
		return;
	}

	if ((address & 0xffff000) == 0x500000) {
		K051316Write(0, (address & 0xffe)/2, data);
		return;
	}

	if ((address & 0xfffffe0) == 0x510000) {
		K051316WriteCtrl(0, (address & 0x1e)/2, data);
		return;
	}

	if ((address & 0xffff000) == 0xffe000) {
		DrvPalRAM[(address & 0xfff)^1] = data;
		palette_update(address & 0xffe);
		return;
	}

	switch (address)
	{
		case 0xfff001:
		{
			static const INT32 banks[6] = { 0, 1, -1, -1, 2, 2 };

			*char_bank = banks[data & 0x05];
			*video_enable =  data & 0x10;
			*pal_bank     = (data & 0x20) ? 7 : 3;
		}

		return;

		case 0xfff009:
			*soundlatch = data;
			ZetNmi();
		return;
	}
}

static UINT8 __fastcall tail2nose_main_read_byte(UINT32 address)
{
	if ((address & 0xffff000) == 0x500000) {
		return K051316Read(0, (address & 0xffe)/2);
	}

	switch (address)
	{
		case 0xfff000:
			return DrvInputs[0] >> 8;

		case 0xfff001:
			return DrvInputs[0];

		case 0xfff004:
			return DrvDips[0];

		case 0xfff005:
			return DrvDips[1];
	}

	return 0;
}

static UINT16 __fastcall tail2nose_main_read_word(UINT32 address)
{
	if ((address & 0xffff000) == 0x500000) {
		return K051316Read(0, (address & 0xffe)/2);
	}

	switch (address)
	{
		case 0xfff000:
			return DrvInputs[0];

		case 0xfff004:
			return (DrvDips[1] << 8) | (DrvDips[0] << 0);
	}

	return 0;
}

static void __fastcall tail2nose_sound_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			BurnYM2608Write(port & 0x03, data);
		return;
	}
}

static UINT8 __fastcall tail2nose_sound_in(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x07:
			return *soundlatch;
	}

	return 0;
}

static void tail2noseFMIRQHandler(INT32, INT32 status)
{
	ZetSetIRQLine(0, (status & 1) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE );
}

static INT32 tail2noseSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 5000000;
}

static double tail2noseGetTime()
{
	return (double)ZetTotalCycles() / 5000000;
}

static void tail2nos_zoom_callback(INT32 *code, INT32 *color, INT32 */*flags*/)
{
	*code |= ((*color & 0x03) << 8);
	*color = 0x20 + ((*color & 0x38) >> 3);
}

static void bankswitch(UINT32, UINT32 data)
{
	if (ZetGetActive() == -1) return; // bypass crash on init

	DrvZ80Bank[0] = data & 1;

	ZetMapArea(0x8000, 0xffff, 0, DrvZ80ROM + 0x10000 + (data & 1) * 0x8000);
	ZetMapArea(0x8000, 0xffff, 2, DrvZ80ROM + 0x10000 + (data & 1) * 0x8000);
}

static void DrvGfxExpand(UINT8 *s, INT32 len)
{
	for (INT32 i = len - 1; i >= 0; i--) {
		INT32 d = s[i];
		s[i * 2 + 0] = d & 0xf;
		s[i * 2 + 1] = d >> 4;
	}
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2608Reset();
	bankswitch(0,0);
	ZetClose();

	K051316Reset();

	redraw_zoom_tiles = 1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;
	DrvZ80ROM	= Next; Next += 0x020000;

	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x100000;

	DrvISndROM	= Next; Next += 0x002000;
	DrvSndROM	= Next; Next += 0x020000;

	DrvPalette	= (UINT32*)Next; Next += 0x0800 * sizeof(int);

	AllRam		= Next;

	soundlatch 	= Next; Next += 0x000001;
	char_bank 	= Next; Next += 0x000001;
	video_enable 	= Next; Next += 0x000001;
	pal_bank 	= Next; Next += 0x000001;
	DrvZ80Bank	= Next; Next += 0x000001;

	DrvSprRAM	= Next; Next += 0x001000;
	Drv68KRAM	= Next; Next += 0x004000;
	DrvPalRAM	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x001000;
	DrvZoomRAM	= Next; Next += 0x020000;
	DrvZoomRAMExp	= Next; Next += 0x040000;

	DrvZ80RAM	= Next; Next += 0x000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000001,	 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000000,	 1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020001,	 2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x020000,	 3, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x040000,	 4, 1)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0c0001,	 5, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x0c0000,	 6, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM + 0x000000,	 7, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x010000,	 8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,	 9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,	10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,	11, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,	12, 2)) return 1;

		if (BurnLoadRom(DrvSndROM,		13, 1)) return 1;

		if (BurnLoadRom(DrvISndROM,		0x80, 1)) return 1;

		for (INT32 i = 0; i < 0x80000; i+=4) {
			BurnByteswap(DrvGfxROM1 + i + 1, 2);
		}

		DrvGfxExpand(DrvGfxROM0, 0x100000);
		DrvGfxExpand(DrvGfxROM1, 0x080000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x040000,	0x200000, 0x27ffff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x0c0000,	0x2c0000, 0x2dffff, MAP_ROM);
	SekMapMemory(DrvZoomRAM,		0x400000, 0x41ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0xff8000, 0xffbfff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0xffc000, 0xffcfff, MAP_RAM);
	SekMapMemory(DrvVidRAM,			0xffd000, 0xffdfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0xffe000, 0xffefff, MAP_ROM);
	SekSetWriteWordHandler(0,		tail2nose_main_write_word);
	SekSetWriteByteHandler(0,		tail2nose_main_write_byte);
	SekSetReadWordHandler(0,		tail2nose_main_read_word);
	SekSetReadByteHandler(0,		tail2nose_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapArea(0x0000, 0x77ff, 0, DrvZ80ROM);
	ZetMapArea(0x0000, 0x77ff, 2, DrvZ80ROM);
	ZetMapArea(0x7800, 0x7fff, 0, DrvZ80RAM);
	ZetMapArea(0x7800, 0x7fff, 1, DrvZ80RAM);
	ZetMapArea(0x7800, 0x7fff, 2, DrvZ80RAM);
	ZetSetOutHandler(tail2nose_sound_out);
	ZetSetInHandler(tail2nose_sound_in);

	INT32 nSndROMLen = 0x20000;
	BurnYM2608Init(8000000, DrvSndROM, &nSndROMLen, DrvISndROM, &tail2noseFMIRQHandler, tail2noseSynchroniseStream, tail2noseGetTime, 0);
	AY8910SetPorts(0, NULL, NULL, NULL, bankswitch); // Really YM2608
	BurnTimerAttachZet(5000000);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_1, 0.25, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_YM2608_ROUTE_2, 1.00, BURN_SND_ROUTE_BOTH);
	BurnYM2608SetRoute(BURN_SND_YM2608_AY8910_ROUTE,   1.00, BURN_SND_ROUTE_BOTH);

	ZetClose();

	K051316Init(0, DrvZoomRAM, DrvZoomRAMExp, 0x3ff, tail2nos_zoom_callback, 4, 0);
	K051316SetOffset(0, -89, -24);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetOpen(0);
	BurnYM2608Exit();
	ZetClose();

	K051316Exit();

	SekExit();
	ZetExit();

	BurnFree (AllMem);

	return 0;
}

static void draw_layer()
{
	UINT16 *ram = (UINT16*)DrvVidRAM;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) << 3;
		INT32 sy = (offs >> 6) << 3;

		if (sx >= nScreenWidth) {
			offs |= 0x3f;
			continue;
		}

		if (sy >= nScreenHeight) break;

		INT32 code  = (ram[offs] & 0x1fff) + (*char_bank << 13);
		INT32 color = (ram[offs] >> 13) + (*pal_bank << 4);

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy - 8, color, 4, 0x0f, 0, DrvGfxROM0);
	}
}

static void draw_sprites()
{
	UINT16 *ram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x300 / 2; offs += 4)
	{
		INT32 sx = ram[offs + 1];
		if (sx >= 0x8000) sx -= 0x10000;

		INT32 sy = 0x10000 - ram[offs + 0];
		if (sy >= 0x8000) sy -= 0x10000;

		INT32 attr  = ram[offs + 2];
		INT32 code  = attr & 0x07ff;
		INT32 color =((attr & 0xe000) >> 13) + 0x28;
		INT32 flipx = attr & 0x1000;
		INT32 flipy = attr & 0x0800;

		if (flipy) {
			if (flipx) {
				RenderCustomTile_Mask_FlipXY_Clip(pTransDraw, 16, 32, 0, sx+4, sy-9, color, 4, 0x0f, 0, DrvGfxROM1 + (code * 0x200));
			} else {
				RenderCustomTile_Mask_FlipY_Clip(pTransDraw, 16, 32, 0, sx+4, sy-9, color, 4, 0x0f, 0, DrvGfxROM1 + (code * 0x200));
			}
		} else {
			if (flipx) {
				RenderCustomTile_Mask_FlipX_Clip(pTransDraw, 16, 32, 0, sx+4, sy-9, color, 4, 0x0f, 0, DrvGfxROM1 + (code * 0x200));
			} else {
				RenderCustomTile_Mask_Clip(pTransDraw, 16, 32, 0, sx+4, sy-9, color, 4, 0x0f, 0, DrvGfxROM1 + (code * 0x200));
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x1000; i+=2) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (*video_enable)
	{
		K051316WrapEnable(0, 1);

		if (redraw_zoom_tiles) {
			K051316RedrawTiles(0);
			redraw_zoom_tiles = 0;
		}

		K051316_zoom_draw(0, 0 | K051316_16BIT);
		draw_sprites();
		draw_layer();
	} else {
		BurnTransferClear();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 1 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}
	}

	SekNewFrame();
	ZetNewFrame();

	INT32 nSegment;
	INT32 nInterleave = 100;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 10000000 / 60, 5000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += SekRun(nSegment);

		BurnTimerUpdate(SekTotalCycles() / 2);
	}

	SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2608Update(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029730;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		SekScan(nAction);
		ZetScan(nAction);

		K051316Scan(nAction);
		BurnYM2608Scan(nAction, pnMin);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(0, DrvZ80Bank[0]);
		ZetClose();

		DrvRecalc = 1;

		for (INT32 i = 0; i < 0x20000; i+=2) {
			graphics_ram_expand_one(i);
		}
	}

	return 0;
}

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnRomInfo Ym2608RomDesc[] = {
#if !defined (ROM_VERIFY)
	{ "ym2608_adpcm_rom.bin",  0x002000, 0x23c9e0d8, BRF_ESS | BRF_PRG | BRF_BIOS },
#else
	{ "",  0x000000, 0x00000000, BRF_ESS | BRF_PRG | BRF_BIOS },
#endif
};


// Tail to Nose - Great Championship

static struct BurnRomInfo tail2nosRomDesc[] = {
	{ "v4",		0x10000, 0x1d4240c2, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "v7",		0x10000, 0x0fb70066, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "v3",		0x10000, 0xe2e0abad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "v6",		0x10000, 0x069817a7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a23",	0x80000, 0xd851cf04, 1 | BRF_PRG | BRF_ESS }, //  4 
	{ "v5",		0x10000, 0xa9fe15a1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "v8",		0x10000, 0x4fb6a43e, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "v2",		0x08000, 0x920d8920, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 Code
	{ "v1",		0x10000, 0xbf35c1a4, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "a24",	0x80000, 0xb1e9de43, 3 | BRF_GRA },           //  9 Foreground Tiles
	{ "o1s",	0x40000, 0xe27a8eb4, 3 | BRF_GRA },           // 10

	{ "oj1",	0x40000, 0x39c36b35, 4 | BRF_GRA },           // 11 Sprites
	{ "oj2",	0x40000, 0x77ccaea2, 4 | BRF_GRA },           // 12

	{ "osb",	0x20000, 0xd49ab2f5, 5 | BRF_GRA },           // 13 Samples
};

STDROMPICKEXT(tail2nos, tail2nos, Ym2608)
STD_ROM_FN(tail2nos)

struct BurnDriver BurnDrvTail2nos = {
	"tail2nos", NULL, "ym2608", NULL, "1989",
	"Tail to Nose - Great Championship\0", NULL, "V-System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, tail2nosRomInfo, tail2nosRomName, NULL, NULL, Tail2nosInputInfo, Tail2nosDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Super Formula (Japan)

static struct BurnRomInfo sformulaRomDesc[] = {
	{ "ic129.4",	0x10000, 0x672bf690, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "ic130.7",	0x10000, 0x73f0c91c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "v3",		0x10000, 0xe2e0abad, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "v6",		0x10000, 0x069817a7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "a23",	0x80000, 0xd851cf04, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "v5",		0x10000, 0xa9fe15a1, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "v8",		0x10000, 0x4fb6a43e, 1 | BRF_PRG | BRF_ESS }, //  6

	{ "v2",		0x08000, 0x920d8920, 2 | BRF_PRG | BRF_ESS }, //  7 Z80 Code
	{ "v1",		0x10000, 0xbf35c1a4, 2 | BRF_PRG | BRF_ESS }, //  8

	{ "a24",	0x80000, 0xb1e9de43, 3 | BRF_GRA },           //  9 Foreground Tiles
	{ "o1s",	0x40000, 0xe27a8eb4, 3 | BRF_GRA },           // 10

	{ "oj1",	0x40000, 0x39c36b35, 4 | BRF_GRA },           // 11 Sprites
	{ "oj2",	0x40000, 0x77ccaea2, 4 | BRF_GRA },           // 12

	{ "osb",	0x20000, 0xd49ab2f5, 5 | BRF_GRA },           // 13 Samples
};

STDROMPICKEXT(sformula, sformula, Ym2608)
STD_ROM_FN(sformula)

struct BurnDriver BurnDrvSformula = {
	"sformula", "tail2nos", "ym2608", NULL, "1989",
	"Super Formula (Japan)\0", NULL, "V-System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, sformulaRomInfo, sformulaRomName, NULL, NULL, Tail2nosInputInfo, Tail2nosDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};
