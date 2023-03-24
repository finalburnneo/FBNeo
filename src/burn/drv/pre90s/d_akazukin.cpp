// FinalBurn Neo Sigma Akazukin driver module
// Based on MAME driver by Alberto Salso, Ignacio Seki

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvZ80ROM[2];
static UINT8 *DrvGfxROM[4];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM[2];
static UINT8 *DrvVidRAM[3];
static UINT8 *DrvColRAM[3];
static UINT8 *DrvAtrRAM[3];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nCyclesExtra[2];

static INT32 nmi_mask[2];
static INT32 soundlatch[2];
static INT32 flipscreen;
static INT32 sprite_priority;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo AkazukinInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Akazukin)

static struct BurnDIPInfo AkazukinDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0x9d, NULL							},
	{0x01, 0xff, 0xff, 0xef, NULL							},

	{0   , 0xfe, 0   ,    4, "Lives"						},
	{0x00, 0x01, 0x03, 0x03, "2"							},
	{0x00, 0x01, 0x03, 0x01, "3"							},
	{0x00, 0x01, 0x03, 0x02, "4"							},
	{0x00, 0x01, 0x03, 0x00, "255"							},

	{0   , 0xfe, 0   ,    2, "Flip Screen"					},
	{0x00, 0x01, 0x10, 0x10, "Off"							},
	{0x00, 0x01, 0x10, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Cabinet"						},
	{0x00, 0x01, 0x20, 0x00, "Upright"						},
	{0x00, 0x01, 0x20, 0x20, "Cocktail"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"					},
	{0x00, 0x01, 0x40, 0x40, "Off"							},
	{0x00, 0x01, 0x40, 0x00, "On"							},

	{0   , 0xfe, 0   ,    2, "Skip current level (Cheat)"	},
	{0x00, 0x01, 0x80, 0x80, "Off"							},
	{0x00, 0x01, 0x80, 0x00, "On"							},

	{0   , 0xfe, 0   ,    10, "Coinage"						},
	{0x01, 0x01, 0xf0, 0xb0, "3 Coins 1 Credit"				},
	{0x01, 0x01, 0xf0, 0xd0, "2 Coins 1 Credit"				},
	{0x01, 0x01, 0xf0, 0x90, "4 Coins 5 Credits"			},
	{0x01, 0x01, 0xf0, 0xa0, "2 Coins 3 Credits"			},
	{0x01, 0x01, 0xf0, 0xe0, "1 Coin  1 Credit"				},
	{0x01, 0x01, 0xf0, 0xc0, "1 Coin  2 Credits"			},
	{0x01, 0x01, 0xf0, 0x20, "1 Coin  3 Credits"			},
	{0x01, 0x01, 0xf0, 0x10, "1 Coin  5 Credits"			},
	{0x01, 0x01, 0xf0, 0x00, "1 Coin  6 Credits"			},
	{0x01, 0x01, 0xf0, 0xf0, "Free Play"					},
};

STDDIPINFO(Akazukin)

static void __fastcall main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7792:
		case 0x7793:
		case 0x7794:
		case 0x7795:
		return; // junk?

		case 0xac00:
			sprite_priority = data;
		return;

		case 0xc000:
			nmi_mask[0] = data & 1;
		return;

		case 0xc001:
			flipscreen = data & 1;
		return;

		case 0xc002:
			ZetSetRESETLine(1, ~data & 1);
		return;

		case 0xc003: // coin counter
		return;

		case 0xc005: // watchdog?
		return;
	}
}

static UINT8 __fastcall main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return DrvInputs[0];

		case 0xe800:
			return DrvInputs[1];

		case 0xf000:
			return DrvInputs[2];
	}

	return 0;
}

static void __fastcall main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
			soundlatch[0] = data;
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
		return;
	}
}

static UINT8 __fastcall main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x10:
			return soundlatch[1];
	}

	return 0;
}

static void __fastcall sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x5000:
			nmi_mask[1] = data & 1;
		return;
	}
}

static void __fastcall sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x10:
			soundlatch[1] = data;
			ZetSetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
		return;

		case 0x40:
		case 0x41:
		case 0x80:
		case 0x81:
			AY8910Write((port / 0x80) & 1, port & 1, data);
		return;
	}
}

static UINT8 __fastcall sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x10:
			return soundlatch[0];

		case 0x42:
		case 0x82:
			return AY8910Read((port / 0x80) & 1);
	}

	return 0;
}

static UINT8 AY8910_0_port0(UINT32)
{
	return DrvDips[1];
}

static UINT8 AY8910_0_port1(UINT32)
{
	return 0xff;
}

static UINT8 AY8910_1_port0(UINT32)
{
	return DrvDips[0];
}

static tilemap_callback( fg )
{
	INT32 code  = DrvVidRAM[2][offs] | (DrvAtrRAM[2][offs] * 256);
	INT32 color = DrvColRAM[2][offs];

	TILE_SET_INFO(0, code, color, TILE_FLIPXY(code >> 10));
}

static tilemap_callback( bg0 )
{
	INT32 code  = DrvVidRAM[0][offs] | (DrvAtrRAM[0][offs] * 256);
	INT32 color = DrvColRAM[0][offs];

	TILE_SET_INFO(4, code, color, TILE_FLIPXY(code >> 10));
}

static tilemap_callback( bg1 )
{
	INT32 code  = DrvVidRAM[1][offs] | (DrvAtrRAM[1][offs] * 256);
	INT32 color = DrvColRAM[1][offs];

	TILE_SET_INFO(3, code, color, TILE_FLIPXY(code >> 10));
}

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	AY8910Reset(0);
	AY8910Reset(1);

	sprite_priority = 0;
	flipscreen = 0;
	soundlatch[0] = 0;
	soundlatch[1] = 0;
	nmi_mask[0] = 0;
	nmi_mask[1] = 0;

	nCyclesExtra[0] = nCyclesExtra[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM[0]		= Next; Next += 0x008000;
	DrvZ80ROM[1]		= Next; Next += 0x003000;

	DrvGfxROM[0]		= Next; Next += 0x008000;
	DrvGfxROM[1]		= Next; Next += 0x010000;
	DrvGfxROM[2]		= Next; Next += 0x004000;
	DrvGfxROM[3]		= Next; Next += 0x008000;

	DrvColPROM			= Next; Next += 0x000300;

	DrvPalette			= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM[0]		= Next; Next += 0x000800;
	DrvZ80RAM[1]		= Next; Next += 0x000800;

	DrvVidRAM[0]		= Next; Next += 0x000400;
	DrvVidRAM[1]		= Next; Next += 0x000400;
	DrvVidRAM[2]		= Next; Next += 0x000400;

	DrvColRAM[0]		= Next; Next += 0x000400;
	DrvColRAM[1]		= Next; Next += 0x000400;
	DrvColRAM[2]		= Next; Next += 0x000400;

	DrvAtrRAM[0]		= Next; Next += 0x000400;
	DrvAtrRAM[1]		= Next; Next += 0x000400;
	DrvAtrRAM[2]		= Next; Next += 0x000400;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static void DrvGfxDecode()
{
	static INT32 Plane[2]  = { 0,4 };
	static INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(128+64,1) };
	static INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM[0], 0x1000);

	GfxDecode(0x0100, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs, YOffs, 0x200, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x1000);

	GfxDecode(0x0100, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM[2]);

	memcpy (tmp, DrvGfxROM[3], 0x2000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM[3]);

	BurnFree (tmp);
}


static INT32 DrvInit()
{
//	BurnSetRefreshRate(60.58);

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x02000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x04000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[0] + 0x06000, k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM[1] + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[3] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM   + 0x00200, k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM[0],		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM[1],		0x8000, 0x83ff, MAP_RAM);
	ZetMapMemory(DrvColRAM[1],		0x8400, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvAtrRAM[1],		0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[0],		0x9000, 0x93ff, MAP_RAM);
	ZetMapMemory(DrvColRAM[0],		0x9400, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvAtrRAM[0],		0x9800, 0x9bff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[2],		0xa000, 0xa3ff, MAP_RAM);
	ZetMapMemory(DrvAtrRAM[2],		0xa400, 0xa7ff, MAP_RAM);
	ZetMapMemory(DrvColRAM[2],		0xa800, 0xabff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM[0],		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(main_write);
	ZetSetReadHandler(main_read);
	ZetSetInHandler(main_read_port);
	ZetSetOutHandler(main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM[1],		0x0000, 0x2fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM[1],		0x4000, 0x47ff, MAP_RAM);
	ZetSetWriteHandler(sound_write);
	ZetSetInHandler(sound_read_port);
	ZetSetOutHandler(sound_write_port);
	ZetClose();

	AY8910Init(0, 1536000, 0);
	AY8910Init(1, 1536000, 1);
	AY8910SetPorts(0, &AY8910_0_port0, &AY8910_0_port1, NULL, NULL);
	AY8910SetPorts(1, &AY8910_1_port0, &AY8910_0_port1, NULL, NULL);
	AY8910SetAllRoutes(0, 0.53, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.53, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3072000);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, fg_map_callback,  8, 8, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, bg0_map_callback, 8, 8, 32, 32);
	GenericTilemapInit(2, TILEMAP_SCAN_ROWS, bg1_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 2,  8,  8, 0x04000, 0, 0x3f);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 2, 16, 16, 0x10000, 0, 0x3f);
	GenericTilemapSetGfx(2, DrvGfxROM[1], 2, 16, 32, 0x10000, 0, 0x3f);
	GenericTilemapSetGfx(3, DrvGfxROM[2], 2,  8,  8, 0x04000, 0, 0x3f);
	GenericTilemapSetGfx(4, DrvGfxROM[3], 2,  8,  8, 0x08000, 0, 0x3f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetTransparent(2, 0);
	GenericTilemapSetScrollCols(1, 32);
	GenericTilemapSetScrollCols(2, 32);
	GenericTilemapSetOffsets(TMAP_GLOBAL, 0, -16);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFreeMemIndex();

	return 0;
}

static inline void DrvPaletteInit() // same as vastar
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		INT32 bit0 = (DrvColPROM[0x000 + i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[0x000 + i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[0x000 + i] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[0x000 + i] >> 3) & 0x01;
		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x100 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x100 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x100 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x100 + i] >> 3) & 0x01;
		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[0x200 + i] >> 0) & 0x01;
		bit1 = (DrvColPROM[0x200 + i] >> 1) & 0x01;
		bit2 = (DrvColPROM[0x200 + i] >> 2) & 0x01;
		bit3 = (DrvColPROM[0x200 + i] >> 3) & 0x01;
		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites_bank(INT32 bank) // almost same as vastar
{
	for (INT32 offs = 0x1e + bank; offs >= 0x10 + bank; offs -= 2)
	{
		INT32 code  = ((DrvVidRAM[2][offs + 0] & 0xfc) >> 2) + ((DrvAtrRAM[2][offs + 0] & 0x01) << 6) + (bank << 2);
		INT32 sx    =   DrvVidRAM[2][offs + 1];
		INT32 sy    =   DrvColRAM[2][offs + 0];
		INT32 color =   DrvColRAM[2][offs + 1] & 0x3f;
		INT32 flipx =   DrvVidRAM[2][offs + 0] & 0x02;
		INT32 flipy =   DrvVidRAM[2][offs + 0] & 0x01;
		INT32 tall  =  (DrvAtrRAM[2][offs + 0] & 0x08) >> 3;

		if (flipscreen)
		{
			INT32 t = flipx;
			flipx   = !flipy;	// weird!
			flipy   = !t;
		}
		else
			sy = (256 - (16 << tall)) - sy;

		DrawGfxMaskTile(0, 1 + tall, code >> tall, sx,       (sy      ) - 16, flipx, flipy, color, 0);
		DrawGfxMaskTile(0, 1 + tall, code >> tall, sx,       (sy + 256) - 16, flipx, flipy, color, 0);
		DrawGfxMaskTile(0, 1 + tall, code >> tall, sx - 256, (sy      ) - 16, flipx, flipy, color, 0);
		DrawGfxMaskTile(0, 1 + tall, code >> tall, sx - 256, (sy + 256) - 16, flipx, flipy, color, 0);
	}
}

static void draw_sprites()
{
	draw_sprites_bank(0);
	draw_sprites_bank(0x20);
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetFlip(TMAP_GLOBAL, flipscreen ? TMAP_FLIPXY : 0);

	for (INT32 i = 0; i < 32; i++) {
		GenericTilemapSetScrollCol(1, i, DrvColRAM[2][0x3e0 + i]);
		GenericTilemapSetScrollCol(2, i, DrvColRAM[2][0x3c0 + i]);
	}

	// almost same as vastar
	if (~nBurnLayer & 1) BurnTransferClear();

	switch (sprite_priority)
	{
		case 0:
			if (nBurnLayer & 1) GenericTilemapDraw(1, 0, TMAP_FORCEOPAQUE);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 2) GenericTilemapDraw(2, 0, 0);
			if (nBurnLayer & 4) GenericTilemapDraw(0, 0, 0);
		break;

		case 1:
			if (nBurnLayer & 1) GenericTilemapDraw(1, 0, TMAP_FORCEOPAQUE);
			if (nBurnLayer & 2) GenericTilemapDraw(2, 0, 0);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 4) GenericTilemapDraw(0, 0, 0);
		break;

		case 2:
			if (nBurnLayer & 1) GenericTilemapDraw(1, 0, TMAP_FORCEOPAQUE);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 1) GenericTilemapDraw(1, 0, 0);
			if (nBurnLayer & 2) GenericTilemapDraw(2, 0, 0);
			if (nBurnLayer & 4) GenericTilemapDraw(0, 0, 0);
		break;

		case 3:
			if (nBurnLayer & 1) GenericTilemapDraw(1, 0, TMAP_FORCEOPAQUE);
			if (nBurnLayer & 2) GenericTilemapDraw(2, 0, 0);
			if (nBurnLayer & 4) GenericTilemapDraw(0, 0, 0);
			if (nSpriteEnable & 1) draw_sprites();
		break;

		case 4:
			if (nBurnLayer & 1) GenericTilemapDraw(0, 0, TMAP_FORCEOPAQUE);
			if (nSpriteEnable & 1) draw_sprites();
			if (nBurnLayer & 2) GenericTilemapDraw(1, 0, 0);
			if (nBurnLayer & 4) GenericTilemapDraw(2, 0, 0);
		break;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	ZetNewFrame();

	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 10;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { nCyclesExtra[0], nCyclesExtra[1] };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);
		if (i == (nInterleave - 1) && nmi_mask[0]) ZetNmi();
		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		if (i == (nInterleave - 1) && nmi_mask[1]) ZetNmi();
		ZetClose();
	}

	nCyclesExtra[0] = nCyclesDone[0] - nCyclesTotal[0];
	nCyclesExtra[1] = nCyclesDone[1] - nCyclesTotal[1];

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);
		AY8910Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(nmi_mask);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sprite_priority);

		SCAN_VAR(nCyclesExtra);
	}

	return 0;
}


// Aka Zukin (Japan)

static struct BurnRomInfo akazukinRomDesc[] = {
	{ "1.j2",	0x2000, 0x8987f1e0, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "2.l2",	0x2000, 0xc8b040e4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "3.m2",	0x2000, 0x262edb0d, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "4.n2",	0x2000, 0x3569221f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "5.h7",	0x2000, 0xc34486ae, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code
	{ "6.j7",	0x1000, 0xb5e1d77f, 2 | BRF_PRG | BRF_ESS }, //  5

	{ "10.b9",	0x1000, 0x0145ded8, 3 | BRF_GRA },           //  6 Foreground Tiles

	{ "9.e9",	0x2000, 0x5fecc3d7, 4 | BRF_GRA },           //  7 Sprites
	{ "11.e7",	0x2000, 0x448b28b8, 4 | BRF_GRA },           //  8

	{ "7.t4",	0x1000, 0x7b4124da, 5 | BRF_GRA },           //  9 Background Tiles (Layer 1)

	{ "8.p4",	0x2000, 0x193f6bcb, 6 | BRF_GRA },           // 10 Background Tiles (Layer 0)

	{ "r.r6",	0x0100, 0x77ccc932, 7 | BRF_GRA },           // 11 Color PROMs
	{ "g.m6",	0x0100, 0xeddc3acf, 7 | BRF_GRA },           // 12
	{ "b.l6",	0x0100, 0x059dae45, 7 | BRF_GRA },           // 13

	{ "8n",		0x0100, 0xe1b815ca, 0 | BRF_OPT },           // 14 Unknown PROM
};

STD_ROM_PICK(akazukin)
STD_ROM_FN(akazukin)

struct BurnDriver BurnDrvAkazukin = {
	"akazukin", NULL, NULL, NULL, "1983",
	"Aka Zukin (Japan)\0", NULL, "Sigma", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_ACTION, 0,
	NULL, akazukinRomInfo, akazukinRomName, NULL, NULL, NULL, NULL, AkazukinInputInfo, AkazukinDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	256, 224, 4, 3
};
