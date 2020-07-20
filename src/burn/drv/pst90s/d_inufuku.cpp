// FinalBurn Neo Quiz & Variety Sukusuku Inufuku and 3 On 3 Dunk Madness driver module
// Based on MAME driver by Takahiro Nogi

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2610.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvSndROM[2];
static UINT8 *DrvZ80RAM;
static UINT8 *DrvLineRAM;
static UINT8 *DrvSprRAM[3];
static UINT8 *DrvSprBuf;
static UINT8 *DrvPalRAM;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *Drv68KRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 bg_palettebank;
static INT32 fg_palettebank;
static INT32 bg_scrollx;
static INT32 bg_scrolly;
static INT32 fg_scrolly;
static INT32 fg_scrollx;
static INT32 linescroll_enable;
static INT32 soundlatch;
static INT32 sound_flag;
static INT32 bankdata;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvJoy6[16];
static UINT8 DrvDips[2];
static UINT16 DrvInputs[6];
static UINT8 DrvReset;

static struct BurnInputInfo InufukuInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 4"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy5 + 0,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy5 + 2,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy6 + 0,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy6 + 1,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy6 + 2,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy6 + 3,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy6 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy6 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy6 + 6,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy6 + 7,	"p3 fire 4"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy5 + 1,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy5 + 3,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 0,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 1,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 2,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 fire 3"	},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 4,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy3 + 5,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Inufuku)

static struct BurnDIPInfo InufukuDIPList[]=
{
	{0x2b, 0xff, 0xff, 0x10, NULL				},

	{0   , 0xfe, 0   ,    2, "3P/4P"			},
	{0x2b, 0x01, 0x10, 0x10, "Off"				},
	{0x2b, 0x01, 0x10, 0x00, "On"				},
};

STDDIPINFO(Inufuku)

static void __fastcall inufuku_main_write_word(UINT32 address, UINT16 data)
{
	switch (address)
	{
		case 0x200000:
			EEPROMWriteBit((data & 0x0800) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((data & 0x1000) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetCSLine((data & 0x2000) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
		return;

		case 0x280000:
			soundlatch = data;
			sound_flag = 1;
			ZetNmi();
		return;
		
		case 0x780004:
			bg_palettebank = data >> 12;
		return;

		case 0x780006:
			fg_palettebank = data >> 12;
		return;

		case 0x7a0000:
			bg_scrollx = data + 1;
		return;

		case 0x7a0002:
			bg_scrolly = data + 0;
		return;

		case 0x7a0004:
			fg_scrollx = data - 3;
		return;

		case 0x7a0006:
			fg_scrolly = data + 1;
		return;

		case 0x7a0008:
			linescroll_enable = ((data & 0x0200) >> 9) ^ 1;
		return;
	}
}

static UINT16 __fastcall inufuku_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x180000:
		case 0x180002:
		case 0x180004:
		case 0x180006:
		case 0x18000a:
			return DrvInputs[(address / 2) & 7];

		case 0x180008:
		{
			UINT16 ret = (DrvInputs[4] & 0xff2f) | (DrvDips[0] & 0x10);
			ret |= EEPROMRead() ? 0x40 : 0;
			ret |= sound_flag ? 0 : 0x80;
			return ret;
		}
	}

	return 0;
}

static UINT8 __fastcall inufuku_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x180008:
			return 0xff;
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	bankdata = data & 3;

	ZetMapMemory(DrvZ80ROM + (bankdata * 0x8000), 0x8000, 0xffff, MAP_ROM);
}

static void __fastcall inufuku_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x04:
			sound_flag = 0;
		return;

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			BurnYM2610Write(port & 3, data);
		return;
	}
}

static UINT8 __fastcall inufuku_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return soundlatch;

		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			return BurnYM2610Read(port & 3);
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	if (ZetGetActive() == -1) return;

	ZetSetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static tilemap_callback( bg )
{
	UINT16 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvBgRAM + offs * 2)));

	TILE_SET_INFO(0, code, bg_palettebank, 0);
}

static tilemap_callback( fg )
{
	UINT16 code = BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvFgRAM + offs * 2)));

	TILE_SET_INFO(1, code, fg_palettebank, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
		
	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2610Reset();
	bankswitch(0);
	ZetClose();

	EEPROMReset();

	bg_palettebank = 0;
	fg_palettebank = 0;
	bg_scrollx = 0;
	bg_scrolly = 0;
	fg_scrolly = 0;
	fg_scrollx = 0;
	linescroll_enable = 0;
	soundlatch = 0;
	sound_flag = 0;

	return 0;
}

static INT32 MemIndex(INT32 gfx1len, INT32 gfx2len, INT32 snd0len, INT32 snd1len)
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x500000;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM[0]	= Next; Next += 0x400000;
	DrvGfxROM[1]	= Next; Next += gfx1len;
	DrvGfxROM[2]	= Next; Next += gfx2len;

	DrvSndROM[1]	= Next; Next += snd1len;
	DrvSndROM[0]	= Next; Next += snd0len;

	DrvPalette		= (UINT32*)Next; Next += 0x10010 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x0008000;
	DrvLineRAM		= Next; Next += 0x0004000;
	DrvSprRAM[0]	= Next; Next += 0x0020000;
	DrvSprRAM[1]	= Next; Next += 0x0200000;
	DrvSprRAM[2]	= Next; Next += 0x0020000;
	DrvSprBuf		= Next; Next += 0x0020000;
	DrvPalRAM		= Next; Next += 0x0020000;
	DrvBgRAM		= Next; Next += 0x0020000;
	DrvFgRAM		= Next; Next += 0x00e0000;
	Drv68KRAM		= Next; Next += 0x0100000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit(INT32 select)
{
	AllMem = NULL;
	MemIndex(select ? 0x200000 : 0x400000, select ? 0x4000000 : 0x1800000, select ? 0x100000 : 0x400000, select ? 0x300000 : 0);
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex(select ? 0x200000 : 0x400000, select ? 0x4000000 : 0x1800000, select ? 0x100000 : 0x400000, select ? 0x300000 : 0);

	INT32 k = 0;
	if (select == 0)
	{
		if (BurnLoadRom(Drv68KROM    + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x0080000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x0100000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0400000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0800000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x0000000, k++, 1)) return 1;
	}
	else
	{
		if (BurnLoadRom(Drv68KROM    + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x0080000, k++, 1)) return 1;
		if (BurnLoadRom(Drv68KROM    + 0x0100000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM    + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x0000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0400000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0800000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x0c00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[2] + 0x1000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[0] + 0x0000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM[1] + 0x0000000, k++, 1)) return 1;
	}

	BurnNibbleExpand(DrvGfxROM[2], NULL, select ? 0x2000000 : 0xc00000, select, 0);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,				0x300000, 0x301fff, MAP_RAM);
	SekMapMemory(DrvLineRAM,			0x380000, 0x3803ff, MAP_WRITE); // 0-1ff
	SekMapMemory(DrvBgRAM,				0x400000, 0x401fff, MAP_RAM);
	SekMapMemory(DrvFgRAM,				0x402000, 0x40ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM[0],			0x580000, 0x581fff, MAP_RAM);
	SekMapMemory(DrvSprRAM[1],			0x600000, 0x61ffff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x100000,	0x800000, 0xbfffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,				0xfd0000, 0xfdffff, MAP_RAM);
	SekSetWriteWordHandler(0,			inufuku_main_write_word);
	SekSetReadWordHandler(0,			inufuku_main_read_word);
	SekSetReadByteHandler(0,			inufuku_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,				0x0000, 0x77ff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,				0x7800, 0x7fff, MAP_RAM);
	ZetSetOutHandler(inufuku_sound_write_port);
	ZetSetInHandler(inufuku_sound_read_port);
	ZetClose();

	EEPROMInit(&eeprom_interface_93C46);

	INT32 DrvSndROM0Len = select ? 0x100000 : 0x400000, DrvSndROM1Len = select ? 0x300000 : 0;
	BurnYM2610Init(8000000, DrvSndROM[0], &DrvSndROM0Len, DrvSndROM[1], &DrvSndROM1Len, &DrvFMIRQHandler, 0);
	BurnTimerAttach(&ZetConfig, 8000000);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2610SetRoute(BURN_SND_YM2610_YM2610_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);
	BurnYM2610SetRoute(BURN_SND_YM2610_AY8910_ROUTE, 0.25, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback, 8, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 8,  8,  8, 0x400000, 0, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 8,  8,  8, select ? 0x200000 : 0x400000, 0, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 4, 16, 16, select ? 0x4000000 : 0x1800000, 0, 0x3f);
	GenericTilemapSetTransparent(0, 0xff);
	GenericTilemapSetTransparent(1, 0xff);
//	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	EEPROMExit();

	BurnYM2610Exit();

	ZetExit();
	SekExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x2000/2; i++)
	{
		UINT8 r = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  0) & 0x1f;
		UINT8 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >>  5) & 0x1f;
		UINT8 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (b >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}

	DrvPalette[0x1000] = 0; // black
}

static void draw_sprites()
{
	INT32 offs;
	UINT16 *spriteram = (UINT16*)DrvSprRAM[2];
	UINT16 *spritetable = (UINT16*)DrvSprRAM[1];
	GenericTilesGfx *gfx = &GenericGfxData[2];
	INT32 priority_table[4] = { 0, 0xf0, 0xfc, 0xfe };

	for (offs = 0; offs < (0x2000 / 16); offs++)
	{
		if (BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x4000) break;
	}

	offs--;
	while (offs != -1)
	{
		if ((BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x8000) == 0x0000)
		{
			INT32 attr_start = (BURN_ENDIAN_SWAP_INT16(spriteram[offs]) & 0x03ff) * 4;

			UINT16 *ram = &spriteram[attr_start];

			INT32 oy =    (BURN_ENDIAN_SWAP_INT16(ram[0]) & 0x01ff) + 1;
			INT32 ysize = (BURN_ENDIAN_SWAP_INT16(ram[0]) & 0x0e00) >> 9;
			INT32 zoomy = 32 - ((BURN_ENDIAN_SWAP_INT16(ram[0]) & 0xf000) >> 12);
			INT32 ox =    (BURN_ENDIAN_SWAP_INT16(ram[1]) & 0x01ff) + 0;
			INT32 xsize = (BURN_ENDIAN_SWAP_INT16(ram[1]) & 0x0e00) >> 9;
			INT32 zoomx = 32 - ((BURN_ENDIAN_SWAP_INT16(ram[1]) & 0xf000) >> 12);
			INT32 flipx = (BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x4000);
			INT32 flipy = (BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x8000);
			INT32 color =((BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x3f00) >> 8) << gfx->depth;
			INT32 pri   = (BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x3000) >> 12;
			INT32 map   = (BURN_ENDIAN_SWAP_INT16(ram[3]) & 0xffff) | (BURN_ENDIAN_SWAP_INT16(ram[2]) & 0x0001) << 16;

			INT32 priority_mask = priority_table[pri];

			INT32 ystart, yend, yinc;

			if (!flipy) { ystart = 0; yend = ysize+1; yinc = 1; }
			else         { ystart = ysize; yend = -1; yinc = -1; }

			INT32 ycnt = ystart;

			while (ycnt != yend)
			{
				INT32 xstart, xend, xinc;

				if (!flipx) { xstart = 0; xend = xsize+1; xinc = 1; }
				else        { xstart = xsize; xend = -1; xinc = -1; }

				INT32 xcnt = xstart;
				while (xcnt != xend)
				{
					INT32 startno = (((BURN_ENDIAN_SWAP_INT16(spritetable[map*2]) & 0x0007) << 16) + BURN_ENDIAN_SWAP_INT16(spritetable[(map*2)+ 1])) % gfx->code_mask;

					RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, startno, color, 0xf, ox + xcnt * zoomx/2,        oy + ycnt * zoomy/2, 	 flipx, flipy, 16, 16, zoomx << 11, zoomy << 11, priority_mask);
					RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, startno, color, 0xf, -0x200+ox + xcnt * zoomx/2, oy + ycnt * zoomy/2, 	 flipx, flipy, 16, 16, zoomx << 11, zoomy << 11, priority_mask);
					RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, startno, color, 0xf, ox + xcnt * zoomx/2,        -0x200+oy + ycnt * zoomy/2, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11, priority_mask);
					RenderZoomedPrioSprite(pTransDraw, gfx->gfxbase, startno, color, 0xf, -0x200+ox + xcnt * zoomx/2, -0x200+oy + ycnt * zoomy/2, flipx, flipy, 16, 16, zoomx << 11, zoomy << 11, priority_mask);

					xcnt+=xinc;
					map++;
				}
				ycnt+=yinc;
			}
		}

		offs-=1;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferClear(0x1000);

	if (linescroll_enable) {
		UINT16 *scroll = (UINT16*)DrvLineRAM;

		GenericTilemapSetScrollRows(0, 512);
		for (INT32 i = 0; i < 256; i++) {
			GenericTilemapSetScrollRow(0, (bg_scrolly + i) & 0x1ff, bg_scrollx + BURN_ENDIAN_SWAP_INT16(scroll[i]));
		}
	} else {
		GenericTilemapSetScrollRows(0, 1);
		GenericTilemapSetScrollX(0, bg_scrollx);
	}

	GenericTilemapSetScrollY(0, bg_scrolly);
	GenericTilemapSetScrollX(1, fg_scrollx);
	GenericTilemapSetScrollY(1, fg_scrolly);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 4);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		if (i == nInterleave-1) SekSetIRQLine(1, CPU_IRQSTATUS_AUTO);

		BurnTimerUpdate((nCyclesTotal[1] * (i + 1)) / nInterleave);
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2610Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	memcpy (DrvSprRAM[2], DrvSprRAM[0], 0x2000);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		ZetScan(nAction);
		EEPROMScan(nAction, pnMin);
		BurnYM2610Scan(nAction, pnMin);

		SCAN_VAR(bg_palettebank);
		SCAN_VAR(fg_palettebank);
		SCAN_VAR(bg_scrollx);
		SCAN_VAR(bg_scrolly);
		SCAN_VAR(fg_scrolly);
		SCAN_VAR(fg_scrollx);
		SCAN_VAR(linescroll_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_flag);
		SCAN_VAR(bankdata);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Quiz & Variety Sukusuku Inufuku (Japan)

static struct BurnRomInfo inufukuRomDesc[] = {
	{ "u147.bin",					0x080000, 0xab72398c, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "u146.bin",					0x080000, 0xe05e9bd4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lhmn5l28.148",				0x400000, 0x802d17e7, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "u107.bin",					0x020000, 0x1744ef90, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "lhmn5ku8.u40",				0x400000, 0x8cbca80a, 3 | BRF_GRA },           //  4 Background Tiles

	{ "lhmn5ku7.u8",				0x400000, 0xa6c0f07f, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "lhmn5kub.u34",				0x400000, 0x7753a7b6, 5 | BRF_GRA },           //  6 Sprites
	{ "lhmn5kua.u36",				0x400000, 0x1ac4402a, 5 | BRF_GRA },           //  7
	{ "lhmn5ku9.u38",				0x400000, 0xe4e9b1b6, 5 | BRF_GRA },           //  8

	{ "lhmn5ku6.u53",				0x400000, 0xb320c5c9, 6 | BRF_SND },           //  9 YM2610 ADPCM
};

STD_ROM_PICK(inufuku)
STD_ROM_FN(inufuku)

static INT32 InufukuInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvInufuku = {
	"inufuku", NULL, NULL, NULL, "1998",
	"Quiz & Variety Sukusuku Inufuku (Japan)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_QUIZ, 0,
	NULL, inufukuRomInfo, inufukuRomName, NULL, NULL, NULL, NULL, InufukuInputInfo, InufukuDIPInfo,
	InufukuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// 3 On 3 Dunk Madness (US, prototype? 1997/02/04)

static struct BurnRomInfo dunk3on3RomDesc[] = {
	{ "prog0_2_4_usa.u147",			0x080000, 0x957924ab, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "prog1_2_4_usa.u146",			0x080000, 0x2479e236, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "lh535l5y.u148",				0x400000, 0xaa33e02a, 1 | BRF_PRG | BRF_ESS }, //  2

	{ "sound_prog_97_1_13.u107",	0x020000, 0xd9d42805, 2 | BRF_PRG | BRF_ESS }, //  3 Z80 Code

	{ "lh525kwy.u40",				0x400000, 0xaaa426d1, 3 | BRF_GRA },           //  4 Background Tiles

	{ "lh537nn4.u8",				0x200000, 0x2b7be1d8, 4 | BRF_GRA },           //  5 Foreground Tiles

	{ "lh535kwz.u34",				0x400000, 0x7372ce78, 5 | BRF_GRA },           //  6 Sprites
	{ "lh535kv0.u36",				0x400000, 0x247e5741, 5 | BRF_GRA },           //  7
	{ "lh535kv2.u38",				0x400000, 0x76449b1e, 5 | BRF_GRA },           //  8
	{ "lh537nn5.u20",				0x200000, 0xf457cd3b, 5 | BRF_GRA },           //  9
	{ "lh536pnm.u32",				0x400000, 0xbc39e449, 5 | BRF_GRA },           // 10

	{ "lh5388r1.u53",				0x100000, 0x765d892f, 6 | BRF_SND },           // 11 YM2610 ADPCM

	{ "lh536pkl.u51",				0x300000, 0xe4919abf, 7 | BRF_SND },           // 12 YM2610 Speech
};

STD_ROM_PICK(dunk3on3)
STD_ROM_FN(dunk3on3)

static INT32 Dunk3on3Init()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvDunk3on3 = {
	"3on3dunk", NULL, NULL, NULL, "1996",
	"3 On 3 Dunk Madness (US, prototype? 1997/02/04)\0", NULL, "Video System Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_POST90S, GBF_SPORTSMISC, 0,
	NULL, dunk3on3RomInfo, dunk3on3RomName, NULL, NULL, NULL, NULL, InufukuInputInfo, InufukuDIPInfo,
	Dunk3on3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};
