// FB Alpha Kyuukoukabakugekitai - Dive Bomber Squad driver module
// Based on MAME driver by David Haywood, Phil Bennett

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "konamiic.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 bankdata;
static bool has_fromsprite;
static bool has_fromroz;
static INT32 from_sprite;
static INT32 from_roz;
static INT32 to_spritecpu;
static INT32 to_rozcpu;
static INT32 roz_enable[2];
static INT32 roz_palettebank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DivebombInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Divebomb)

static struct BurnDIPInfo DivebombDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL				},
	{0x13, 0xff, 0xff, 0xff, NULL				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x13, 0x01, 0x03, 0x03, "3"				},
	{0x13, 0x01, 0x03, 0x02, "5"				},
	{0x13, 0x01, 0x03, 0x01, "7"				},
	{0x13, 0x01, 0x03, 0x00, "9"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x13, 0x01, 0x04, 0x00, "Upright"			},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"			},
};

STDDIPINFO(Divebomb)

static inline void update_irqs()
{
	INT32 active = ZetGetActive();

	if (active != 0)
	{
		ZetClose();
		ZetOpen(0);
	}

	ZetSetIRQLine(0, (has_fromsprite || has_fromroz) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);

	if (active != 0)
	{
		ZetClose();
		ZetOpen(active);
	}
}

static void __fastcall divebomb_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
			SN76496Write(port & 7, data);
		return;

		case 0x10:
		{
			ZetSetIRQLine(2, 0, CPU_IRQSTATUS_ACK);

			to_rozcpu = data;
		}
		return;

		case 0x20:
		{
			ZetSetIRQLine(1, 0, CPU_IRQSTATUS_ACK);

			to_spritecpu = data;
		}
		return;
	}
}

static UINT8 __fastcall divebomb_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x10:
			has_fromroz = false;
			update_irqs();
			return from_roz;

		case 0x20:
			has_fromsprite = false;
			update_irqs();
			return from_sprite;

		case 0x30:
		case 0x31:
			return DrvInputs[port & 1];

		case 0x32:
		case 0x33:
			return DrvDips[port & 1];

		case 0x34:
		case 0x35:
			return 0xff; // unused DIPs

		case 0x36:
			return DrvInputs[2];

		case 0x37:
		{
			UINT8 ret = 0;
			if (has_fromroz) ret |= 1;
			if (has_fromsprite) ret |= 2;
			return ret;
		}
	}

	return 0;
}

static void __fastcall divebomb_sprite_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			// unknown
		return;

		case 0x80:
			from_sprite = data;
			has_fromsprite = true;
			update_irqs();
		return;
	}
}

static UINT8 __fastcall divebomb_sprite_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x80:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return to_spritecpu;
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	INT32 bank = ((data & 0x10) >> 1) | ((data & 0x20) >> 3) | ((data & 0x40) >> 5) | ((data & 0x80) >> 7);

	bankdata = data;

	ZetMapMemory(DrvZ80ROM2 + 0x10000 + bank * 0x4000, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall divebomb_roz_write(UINT16 address, UINT8 data)
{
	if ((address & 0xe800) == 0xc000) {
		K051316Write((address >> 12) & 1, address & 0x7ff, data);
		return;
	}
}

static UINT8 __fastcall divebomb_roz_read(UINT16 address)
{
	if ((address & 0xe800) == 0xc000) {
		return K051316Read((address >> 12) & 1, address & 0x7ff);
	}

	return 0;
}

static void __fastcall divebomb_roz_write_port(UINT16 port, UINT8 data)
{
	if ((port & 0xe0) == 0x20) {
		K051316WriteCtrl((port >> 4) & 1, port & 0xf, data);
		return;
	}

	switch (port & 0xff)
	{
		case 0x00:
			bankswitch(data);
		return;

		case 0x10:
			K051316WrapEnable(1, ~data & 1);
		return;

		case 0x12:
		case 0x13:
			roz_enable[port & 1] = ~data & 1;
		return;

		case 0x14:
			K051316WrapEnable(0, ~data & 1);
		return;

		case 0x40:
			from_roz = data;
			has_fromroz = true;
			update_irqs();
		return;

		case 0x50:
		{
			if ((roz_palettebank >> 4) != (data >> 4)) {
				K051316RedrawTiles(0);
			}

			if ((roz_palettebank & 3) != (data & 3)) {
				K051316RedrawTiles(1);
			}

			roz_palettebank = data;
		}
		return;
	}
}

static UINT8 __fastcall divebomb_roz_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x40:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return to_rozcpu;
	}

	return 0;
}

static void divebomb_roz0_callback(INT32 *code, INT32 *color, INT32 *)
{
	*code |= (*color & 0x03) << 8;
	*color = ((roz_palettebank >> 4) & 3);
}

static void divebomb_roz1_callback(INT32 *code, INT32 *color, INT32 *)
{
	*code |= (*color & 0x03) << 8;
	*color = (roz_palettebank & 3) + 4;
}

static tilemap_callback( txt )
{
	INT32 attr = DrvFgRAM[offs + 0x400];
	INT32 code = DrvFgRAM[offs + 0x000] + ((attr & 3) << 8);
	INT32 color = attr >> 4;

	TILE_SET_INFO(0, code, color, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	ZetOpen(2);
	ZetReset();
	bankswitch(0);
	ZetClose();

	K051316Reset();
	K051316WrapEnable(0, 0);
	K051316WrapEnable(1, 0);

	roz_enable[0] = 0;
	roz_enable[1] = 0;

	has_fromsprite = false;
	has_fromroz = false;
	from_sprite = 0;
	from_roz = 0;
	to_spritecpu = 0;
	to_rozcpu = 0;
	roz_palettebank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x008000;
	DrvZ80ROM2		= Next; Next += 0x040000;

	DrvGfxROM0		= Next; Next += 0x040000;
	DrvGfxROM1		= Next; Next += 0x100000;
	DrvGfxROM2		= Next; Next += 0x040000;
	DrvGfxROM3		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x003000;

	DrvPalette		= (UINT32*)Next; Next += 0xd00 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x002000;
	DrvZ80RAM1		= Next; Next += 0x002000;
	DrvZ80RAM2		= Next; Next += 0x002000;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes0[2] = { 8, 0 };
	INT32 XOffs0[8]  = { STEP8(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };
	INT32 Planes1[4] = { STEP4(24,-8) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(32,1) };
	INT32 YOffs1[16] = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);

	memcpy (tmp, DrvGfxROM0, 0x10000);

	GfxDecode(0x1000, 2,  8,  8, Planes0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x80000);

	GfxDecode(0x1000, 4, 16, 16, Planes1, XOffs1, YOffs1, 0x400, tmp, DrvGfxROM1);

	BurnFree (tmp);
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
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM2 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM2 + 0x20000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM2 + 0x30000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  k++, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x00001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00002,  k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00003,  k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x20000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM3 + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x20000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM3 + 0x30000,  k++, 1)) return 1;

		// note the funky loading - used to simplify the palette calculations
		if (BurnLoadRom(DrvColPROM + 0x00000,  k++, 1)) return 1; // roz #0 chip palette
		if (BurnLoadRom(DrvColPROM + 0x01000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x02000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00400,  k++, 1)) return 1; // roz #1 chip palette
		if (BurnLoadRom(DrvColPROM + 0x01400,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x02400,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00800,  k++, 1)) return 1; // fg layer palette
		if (BurnLoadRom(DrvColPROM + 0x01800,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x02800,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00c00,  k++, 1)) return 1; // sprite palette
		if (BurnLoadRom(DrvColPROM + 0x01c00,  k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x02c00,  k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(divebomb_main_write_port);
	ZetSetInHandler(divebomb_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,			0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,		0xe000, 0xffff, MAP_RAM);
	ZetSetOutHandler(divebomb_sprite_write_port);
	ZetSetInHandler(divebomb_sprite_read_port);
	ZetClose();;

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x7fff, MAP_ROM);
//	ZetMapMemory(DrvRozRAM0,		0xc000, 0xc7ff, MAP_RAM);
//	ZetMapMemory(DrvRozRAM1,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM2,		0xe000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(divebomb_roz_write);
	ZetSetReadHandler(divebomb_roz_read);
	ZetSetOutHandler(divebomb_roz_write_port);
	ZetSetInHandler(divebomb_roz_read_port);
	ZetClose();

	SN76489Init(0, 3000000, 0);
	SN76489Init(1, 3000000, 1);
	SN76489Init(2, 3000000, 1);
	SN76489Init(3, 3000000, 1);
	SN76489Init(4, 3000000, 1);
	SN76489Init(5, 3000000, 1);
	SN76496SetRoute(0, 0.15, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.15, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 0.15, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(3, 0.15, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(4, 0.15, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(5, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, txt_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x40000, 0x800, 0xf);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(0, 0, -16);

	K051316Init(0, NULL, DrvGfxROM2, 0x3ffff, divebomb_roz0_callback, 8, 0xff);
	K051316Init(1, NULL, DrvGfxROM3, 0x3ffff, divebomb_roz1_callback, 8, 0xff);
	K051316SetOffset(0, -88, -16);
	K051316SetOffset(1, -88, -16);

	DrvDoReset();
	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	K051316Exit();

	KonamiICExit(); // allocated konami-bitmaps for k051316

	SN76496Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0xd00; i++)
	{
		INT32 bit0 = (DrvColPROM[0x2000 + i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[0x2000 + i] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[0x2000 + i] >> 2) & 1;
		INT32 bit3 = (DrvColPROM[0x2000 + i] >> 3) & 1;
		INT32 r = (bit3 * 2000) + (bit2 * 1000) + (bit1 * 470) + (bit0 * 220);

		bit0 = (DrvColPROM[0x1000 + i] >> 0) & 1;
		bit1 = (DrvColPROM[0x1000 + i] >> 1) & 1;
		bit2 = (DrvColPROM[0x1000 + i] >> 2) & 1;
		bit3 = (DrvColPROM[0x1000 + i] >> 3) & 1;
		INT32 g = (bit3 * 2000) + (bit2 * 1000) + (bit1 * 470) + (bit0 * 220);

		bit0 = (DrvColPROM[0x0000 + i] >> 0) & 1;
		bit1 = (DrvColPROM[0x0000 + i] >> 1) & 1;
		bit2 = (DrvColPROM[0x0000 + i] >> 2) & 1;
		bit3 = (DrvColPROM[0x0000 + i] >> 3) & 1;
		INT32 b = (bit3 * 2000) + (bit2 * 1000) + (bit1 * 470) + (bit0 * 220);

		DrvPalette[i] = BurnHighCol((r * 255) / 3690, (g * 255) / 3690, (b * 255) / 3690, 0);
	}
}

static void draw_sprites()
{
	for (INT32 i = 0; i < 0x800; i+=4)
	{
		INT32 sx    = DrvSprRAM[i + 0];
		INT32 attr  = DrvSprRAM[i + 1];
		INT32 code  = DrvSprRAM[i + 2] + ((attr & 0xf) << 8);
		INT32 sy    = DrvSprRAM[i + 3];
		INT32 color = attr >> 4;

		Draw16x16MaskTile(pTransDraw, code, sx, sy -   0, 0, 0, color, 4, 0, 0xc00, DrvGfxROM1);
		Draw16x16MaskTile(pTransDraw, code, sx, sy - 256, 0, 0, color, 4, 0, 0xc00, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear(0x800); // BLACK?

	if (roz_enable[1] && (nBurnLayer & 1)) K051316_zoom_draw(1, K051316_16BIT);
	if (roz_enable[0] && (nBurnLayer & 2)) K051316_zoom_draw(0, K051316_16BIT);

	if (nSpriteEnable & 1) draw_sprites();

	if (1 && (nBurnLayer & 4)) GenericTilemapDraw(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

//	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256*4; // high interleave!
	INT32 nCyclesTotal[3] = { 6000000 / 60, 6000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 224*4) ZetNmi();
		INT32 nCycles = ZetTotalCycles();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCycles - ZetTotalCycles());
		if (i == 224*4) ZetNmi();
		ZetClose();

		ZetOpen(2);
		nCyclesDone[2] += ZetRun(nCycles - ZetTotalCycles());
		if (i == 224*4) ZetNmi();
		ZetClose();
	}

	if (pBurnSoundOut)
	{
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(2, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(3, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(4, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(5, pBurnSoundOut, nBurnSoundLen);
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
		SN76496Scan(nAction, pnMin);
		K051316Scan(nAction);

		SCAN_VAR(bankdata);
		SCAN_VAR(has_fromsprite);
		SCAN_VAR(has_fromroz);
		SCAN_VAR(from_sprite);
		SCAN_VAR(from_roz);
		SCAN_VAR(to_spritecpu);
		SCAN_VAR(to_rozcpu);
		SCAN_VAR(roz_enable);
		SCAN_VAR(roz_palettebank);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(2);
		bankswitch(bankdata);
		ZetClose();
	}

	return 0;
}


// Kyuukoukabakugekitai - Dive Bomber Squad (Japan, prototype)

static struct BurnRomInfo divebombRomDesc[] = {
	{ "u20.27256",		0x08000, 0x89a30c82, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Text)

	{ "u21.27256",		0x08000, 0x3896d3f6, 2 | BRF_GRA },           //  1 Z80 #1 Code (Sprite)

	{ "u19.27256",		0x08000, 0x16e26fb9, 3 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code (Roz)
	{ "u9.27512",		0x10000, 0xc842f831, 3 | BRF_PRG | BRF_ESS }, //  3
	{ "u10.27512",		0x10000, 0xc77f3574, 3 | BRF_PRG | BRF_ESS }, //  4
	{ "u11.27512",		0x10000, 0x8d46be7d, 3 | BRF_PRG | BRF_ESS }, //  5

	{ "u22.27256",		0x08000, 0xf816f9c5, 4 | BRF_GRA },           //  6 Text Tiles
	{ "u23.27256",		0x08000, 0xd2600570, 4 | BRF_GRA },           //  7

	{ "u15.27c100",		0x20000, 0xccba7fa0, 5 | BRF_GRA },           //  8 Sprites
	{ "u16.27c100",		0x20000, 0x16891fef, 5 | BRF_GRA },           //  9
	{ "u17.27c100",		0x20000, 0xf4cbc97f, 5 | BRF_GRA },           // 10
	{ "u18.27c100",		0x20000, 0x91ab9d89, 5 | BRF_GRA },           // 11

	{ "u1.27512",		0x10000, 0x99af1e18, 6 | BRF_GRA },           // 12 K051316 #0 Tiles
	{ "u2.27512",		0x10000, 0x99c8d516, 6 | BRF_GRA },           // 13
	{ "u3.27512",		0x10000, 0x5ab4af3c, 6 | BRF_GRA },           // 14

	{ "u5.27512",		0x10000, 0x6726d022, 7 | BRF_GRA },           // 15 K051316 #1 Tiles
	{ "u6.27512",		0x10000, 0x756b8a12, 7 | BRF_GRA },           // 16
	{ "u7.27512",		0x10000, 0x01c07d84, 7 | BRF_GRA },           // 17
	{ "u8.27512",		0x10000, 0x5b9e7caa, 7 | BRF_GRA },           // 18

	{ "u29.mb7122.bin",	0x00400, 0x8b3d60d2, 8 | BRF_GRA },           // 19 K051316 #0 Palette
	{ "u30.mb7122.bin",	0x00400, 0x0aeb1a88, 8 | BRF_GRA },           // 20
	{ "u31.mb7122.bin",	0x00400, 0x75cf5f3d, 8 | BRF_GRA },           // 21
	{ "u34.mb7122.bin",	0x00400, 0xe0e2d93b, 8 | BRF_GRA },           // 22 K051316 #1 Palette
	{ "u33.mb7122.bin",	0x00400, 0x4df75f4f, 8 | BRF_GRA },           // 23
	{ "u32.mb7122.bin",	0x00400, 0xe2e4b443, 8 | BRF_GRA },           // 24
	{ "u37.mb7122.bin",	0x00400, 0x55c17465, 8 | BRF_GRA },           // 25 Text Palette
	{ "u36.mb7122.bin",	0x00400, 0x0c1ecdb5, 8 | BRF_GRA },           // 26
	{ "u35.mb7122.bin",	0x00400, 0xe890259d, 8 | BRF_GRA },           // 27
	{ "u83.mb7114.bin",	0x00100, 0xd216110d, 8 | BRF_GRA },           // 28 Sprite Palette
	{ "u82.mb7114.bin",	0x00100, 0x52637774, 8 | BRF_GRA },           // 29
	{ "u81.mb7114.bin",	0x00100, 0xc59b0857, 8 | BRF_GRA },           // 30
};

STD_ROM_PICK(divebomb)
STD_ROM_FN(divebomb)

struct BurnDriver BurnDrvDivebomb = {
	"divebomb", NULL, NULL, NULL, "1989",
	"Kyuukoukabakugekitai - Dive Bomber Squad (Japan, prototype)\0", "bugs are normal!", "Konami", "GX840",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_PREFIX_KONAMI, GBF_VERSHOOT, 0,
	NULL, divebombRomInfo, divebombRomName, NULL, NULL, NULL, NULL, DivebombInputInfo, DivebombDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0xd00,
	224, 256, 3, 4
};
