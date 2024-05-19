// FB Neo Oli-Boo-Chu driver module
// Based on MAME driver by Nicola Salmoria
// Sound emulation: hap

#include "tiles_generic.h"
#include "z80_intf.h"
#include "ay8910.h"
#include "hc55516.h"
#include "bitswap.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSamROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 sound_command;
static UINT16 sound_command_prev;
static UINT16 sample_address;
static UINT8 sample_latch;
static UINT8 soundlatch;
static UINT8 soundlatch1;
static UINT8 flipscreen;
static UINT8 Palette;		// Fake Dip

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo OlibochuInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",	BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",	BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Olibochu)

static struct BurnDIPInfo OlibochuDIPList[] =
{
	{0x11, 0xff, 0xff, 0x01, NULL                    },
};

static struct BurnDIPInfo PunchkidDIPList[] =
{
	{0x11, 0xff, 0xff, 0x00, NULL                    },
};

static struct BurnDIPInfo CommonDIPList[] =
{
	{0x0e, 0xff, 0xff, 0xff, NULL                    },
	{0x0f, 0xff, 0xff, 0xff, NULL                    },
	{0x10, 0xff, 0xff, 0xff, NULL                    },

	{0   , 0xfe, 0   ,    4, "Lives"                 },
	{0x0e, 0x01, 0x03, 0x00, "2"                     },
	{0x0e, 0x01, 0x03, 0x03, "3"                     },
	{0x0e, 0x01, 0x03, 0x02, "4"                     },
	{0x0e, 0x01, 0x03, 0x01, "5"                     },

	{0   , 0xfe, 0   ,    4, "Bonus Life"            },
	{0x0e, 0x01, 0x0c, 0x0c, "5000"                  },
	{0x0e, 0x01, 0x0c, 0x08, "10000"                 },
	{0x0e, 0x01, 0x0c, 0x04, "15000"                 },
	{0x0e, 0x01, 0x0c, 0x00, "None"                  },

	{0   , 0xfe, 0   ,    2, "Cabinet"               },
	{0x0e, 0x01, 0x40, 0x00, "Upright"               },
	{0x0e, 0x01, 0x40, 0x40, "Cocktail"              },

	{0   , 0xfe, 0   ,    2, "Cross Hatch Pattern"   },
	{0x0e, 0x01, 0x80, 0x80, "Off"                   },
	{0x0e, 0x01, 0x80, 0x00, "On"                    },

	{0   , 0xfe, 0   ,    2, "Freeze (Cheat)"        }, // Freeze: press P1 start to stop, P2 start to continue
	{0x10, 0x01, 0x01, 0x01, "Off"                   },
	{0x10, 0x01, 0x01, 0x00, "On"                    },

	{0   , 0xfe, 0   ,    8, "Coin A"                },
	{0x10, 0x01, 0x0e, 0x00, "4 Coins 1 Credits"     },
	{0x10, 0x01, 0x0e, 0x02, "3 Coins 1 Credits"     },
	{0x10, 0x01, 0x0e, 0x04, "2 Coins 1 Credits"     },
	{0x10, 0x01, 0x0e, 0x0e, "1 Coin  1 Credits"     },
	{0x10, 0x01, 0x0e, 0x0c, "1 Coin  2 Credits"     },
	{0x10, 0x01, 0x0e, 0x0a, "1 Coin  3 Credits"     },
	{0x10, 0x01, 0x0e, 0x08, "1 Coin  4 Credits"     },
	{0x10, 0x01, 0x0e, 0x06, "1 Coin  5 Credits"     },

	{0   , 0xfe, 0   ,    2, "Service Mode"          },
	{0x10, 0x01, 0x10, 0x10, "Off"                   },
	{0x10, 0x01, 0x10, 0x00, "On"                    },

	{0   , 0xfe, 0   ,    2, "Invincibility (Cheat)" },
	{0x10, 0x01, 0x20, 0x20, "Off"                   },
	{0x10, 0x01, 0x20, 0x00, "On"                    },

	{0   , 0xfe, 0   ,    2, "Level Select (Cheat)"  }, // Level Select: enable to select round at game start (turn off to start game)
	{0x10, 0x01, 0x40, 0x40, "Off"                   },
	{0x10, 0x01, 0x40, 0x00, "On"                    },

	{0   , 0xfe, 0   ,    2, "Palette"               }, // change the default
	{0x11, 0x01, 0x01, 0x01, "Oli-Boo-Chu"           },
	{0x11, 0x01, 0x01, 0x00, "Punching Kid"          },
};

STDDIPINFOEXT(Olibochu, Common, Olibochu)
STDDIPINFOEXT(Punchkid, Common, Punchkid)

static UINT8 leading0bits(UINT32 val)
{
	INT32 count;
	for (count = 0; INT32(val) >= 0; count++) val <<= 1;
	return count;
}

static inline void update_soundlatch()
{
	UINT8 c;
	UINT16 hi = sound_command & 0xffc0;
	UINT16 lo = sound_command & 0x003f;

	// soundcmd low bits (edge-triggered) = soundlatch d4-d7
	if (lo && lo != (sound_command_prev & 0x3f))
	{
		c = leading0bits(lo) - 26;
		soundlatch1 = c & 0xf;
	}

	// soundcmd high bits = soundlatch d0-d3
	for (c = 0; c < 16 && !BIT(hi, c); c++) { ; }
	soundlatch = (16 - c) & 0xf;
}

static void __fastcall olibochu_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa800:
			sound_command = (sound_command & 0x00ff) | (data << 8);
			update_soundlatch();
		return;

		case 0xa801:
			sound_command_prev = sound_command;
			sound_command = (sound_command & 0xff00) | (data << 0);
			update_soundlatch();
		return;

		case 0xa802:
			flipscreen = data & 0x80;
		return;
	}
}

static UINT8 __fastcall olibochu_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvInputs[0];

		case 0xa001:
			return DrvInputs[1];

		case 0xa002:
			return DrvInputs[2];

		case 0xa003:
			return DrvDips[0];

		case 0xa004:
			return DrvDips[1];

		case 0xa005:
			return DrvDips[2];
	}

	return 0;
}

static void __fastcall olibochu_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x7000:
		case 0x7001:
			AY8910Write(0, address & 1, data);
		return;

		case 0x7004:
			sample_latch = data;
		return;
		case 0x7006:
			if (data & 0x80) {
				soundlatch1 = 0;
				sample_address = sample_latch * 0x20 * 8;
			}

			hc55516_mute_w(~data & 0x80);
		return;
	}
}

static UINT8 __fastcall olibochu_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x7000:
			return (soundlatch & 0xf) | ((soundlatch1 << 4) & 0xf0);
	}

	return 0;
}

static void cvsd_tick()
{
	hc55516_digit_w(BIT(DrvSamROM[sample_address / 8], ~sample_address & 7));
	sample_address = (sample_address + 1) & 0xffff;
	hc55516_clock_w(0);
	hc55516_clock_w(1);
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[offs + 0x400];
	INT32 code = DrvVidRAM[offs] + ((attr & 0x20) << 3);

	TILE_SET_INFO(0, code, attr | 0x20, TILE_FLIPYX(attr >> 6) | TILE_GROUP((attr & 0x20) >> 5));
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

	AY8910Reset(0);
	hc55516_reset();
	hc55516_mute_w(1);

	flipscreen = 0;

	sound_command = 0;
	sound_command_prev = 0;
	sample_address = 0;
	sample_latch = 0;
	soundlatch = 0;
	soundlatch1 = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x002000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvSamROM       = Next; Next += 0x002000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x001000;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[2]  = { 0x2000*8, 0x0000 };
	INT32 XOffs[16] = { STEP8(7,-1), STEP8(128+7,-1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x4000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x4000);

	GfxDecode(0x0200, 2,  8,  8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x0100, 2, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x3000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x5000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x7000,  7, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  8, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x1000,  9, 1)) return 1;

		if (BurnLoadRom(DrvSamROM  + 0x0000, 10, 1)) return 1;
		if (BurnLoadRom(DrvSamROM  + 0x1000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x2000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x2000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x3000, 17, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 18, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0120, 20, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0x9000, 0x9fff, MAP_RAM); // 9000-903f and 9800-983f
	ZetMapMemory(DrvZ80RAM0,		0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(olibochu_main_write);
	ZetSetReadHandler(olibochu_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x6000, 0x63ff, MAP_RAM);
	ZetSetWriteHandler(olibochu_sound_write);
	ZetSetReadHandler(olibochu_sound_read);
	ZetClose();

	AY8910Init(0, 3072000 / 2, 0);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);
	AY8910SetBuffered(ZetTotalCycles, 3072000);

	hc55516_init(ZetTotalCycles, 3072000);
	hc55516_volume(0.65);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 2, 8, 8, 0x8000, 0x80, 0x1f);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetOffsets(0, 0, -8);

	Palette = DrvDips[3]; // get default!

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();
	AY8910Exit(0);
	hc55516_exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pal[32];

	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 1;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 1;
		bit1 = (DrvColPROM[i] >> 4) & 1;
		bit2 = (DrvColPROM[i] >> 5) & 1;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;
		INT32 b = 0x4f * bit0 + 0xa8 * bit1;

		pal[i] = BurnHighCol(r,g,b,0);
	}

	INT32 bank = (Palette & 1) ? 0x10 : 0x00;

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = pal[(DrvColPROM[0x020 + i] & 0x0f) | ((~i >> 4) & bank)];
	}
}

static void draw_sprites_16x16()
{
	UINT8 *spriteram = DrvSprRAM;

	for (INT32 offs = 0x20 - 4; offs >= 0; offs -= 4)
	{
		INT32 attr = spriteram[offs + 1];
		INT32 code = spriteram[offs];
		INT32 color = attr & 0x3f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 sx = spriteram[offs + 3];
		INT32 sy = ((spriteram[offs + 2] + 8) & 0xff) - 8;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 8;

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 2, 0, 0x100, DrvGfxROM1);
	}
}

static void draw_sprites_8x8()
{
	UINT8 *spriteram = DrvSprRAM + 0x800;

	for (INT32 offs = 0x40 - 4; offs >= 0; offs -= 4)
	{
		INT32 attr = spriteram[offs + 1];
		INT32 code = spriteram[offs];
		INT32 color = attr & 0x3f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x80;
		INT32 sx = spriteram[offs + 3];
		INT32 sy = spriteram[offs + 2];

		if (flipscreen)
		{
			sx = 248 - sx;
			sy = 248 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sy -= 8;

		Draw8x8MaskTile(pTransDraw, code, sx, sy, flipx, flipy, color, 2, 0, 0, DrvGfxROM0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, TMAP_FORCEOPAQUE);

	if (nSpriteEnable & 1) draw_sprites_8x8();
	if (nSpriteEnable & 2) draw_sprites_16x16();

	if (nBurnLayer & 2) GenericTilemapDraw(0, pTransDraw, TMAP_SET_GROUP(1));

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
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (Palette != DrvDips[3]) {
			Palette = DrvDips[3];
			DrvRecalc = 1;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 3072000 / 60, 3072000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		CPU_RUN(0, Zet);

		if (i == 128) {
			ZetSetVector(0xcf);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		if (i == 255) {
			ZetSetVector(0xd7);
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}

		ZetClose();

		ZetOpen(1);
		CPU_RUN(1, Zet);
		cvsd_tick();
		if (i == 128 || i == 255) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		hc55516_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
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
		hc55516_scan(nAction, pnMin);

		SCAN_VAR(sound_command);
		SCAN_VAR(sound_command_prev);
		SCAN_VAR(sample_address);
		SCAN_VAR(sample_latch);
		SCAN_VAR(soundlatch);
		SCAN_VAR(soundlatch1);
		SCAN_VAR(flipscreen);
	}

	return 0;
}


// Punching Kid (Japan)

static struct BurnRomInfo punchkidRomDesc[] = {
	{ "pka_1.n3",	0x1000, 0x18f1fa10, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "pka_2.m3",	0x1000, 0x7766d9be, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "pka_3.k3",	0x1000, 0xbb90e21b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "pka_4.j3",	0x1000, 0xce18a851, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "pka_5.h3",	0x1000, 0x426c8254, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "pka_6.f3",	0x1000, 0x288b223e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "pka_7.e3",	0x1000, 0xc689e057, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "pka_8.d3",	0x1000, 0x61c118e0, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "pka_17.j4",	0x1000, 0x57f07402, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "pka_18.l4",	0x1000, 0x0a903e9c, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "pka_15.k1",	0x1000, 0xfb5dd281, 3 | BRF_SND },           // 10 Samples?
	{ "pka_16.m1",	0x1000, 0xc07614a5, 3 | BRF_SND },           // 11

	{ "pka_13.n6",	0x1000, 0x388f2bfd, 4 | BRF_GRA },           // 12 Characters
	{ "pka_14.n4",	0x1000, 0xb5bf456f, 4 | BRF_GRA },           // 13

	{ "pka_9.a6",	0x1000, 0xfa69e16e, 5 | BRF_GRA },           // 14 Sprites
	{ "pka_10.a2",	0x1000, 0x10359f84, 5 | BRF_GRA },           // 15
	{ "pka_11.a4",	0x1000, 0x1d968f5f, 5 | BRF_GRA },           // 16
	{ "pka_12.b2",	0x1000, 0xd8f0c157, 5 | BRF_GRA },           // 17

	{ "c-1.n2",		0x0020, 0xe488e831, 6 | BRF_GRA },           // 18 Color data
	{ "c-2.k6",		0x0100, 0x698a3ba0, 6 | BRF_GRA },           // 19
	{ "c-3.d6",		0x0100, 0xefc4e408, 6 | BRF_GRA },           // 20
};

STD_ROM_PICK(punchkid)
STD_ROM_FN(punchkid)

struct BurnDriver BurnDrvPunchkid = {
	"punchkid", "olibochu", NULL, NULL, "1981",
	"Punching Kid (Japan)\0", NULL, "Irem", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_IREM_MISC, GBF_MAZE, 0,
	NULL, punchkidRomInfo, punchkidRomName, NULL, NULL, NULL, NULL, OlibochuInputInfo, PunchkidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 256, 3, 4
};


// Oli-Boo-Chu (USA)

static struct BurnRomInfo olibochuRomDesc[] = {
	{ "obc_1b.n3",	0x1000, 0xbf17f4f4, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "obc_2b.m3",	0x1000, 0x63833b0d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "obc_3b.k3",	0x1000, 0xa4038e8b, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "obc_4b.j3",	0x1000, 0xaad4bec4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "obc_5b.h3",	0x1000, 0x66efa79f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "obc_6b.f3",	0x1000, 0x1123d1ef, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "obc_7c.e3",	0x1000, 0x89c26fb4, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "obc_8b.d3",	0x1000, 0xaf19e5a5, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "obc_17.j4",	0x1000, 0x57f07402, 2 | BRF_PRG | BRF_ESS }, //  8 Z80 #1 Code
	{ "obc_18.l4",	0x1000, 0x0a903e9c, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "obc_15.k1",	0x1000, 0xfb5dd281, 3 | BRF_SND },           // 10 Samples?
	{ "obc_16.m1",	0x1000, 0xc07614a5, 3 | BRF_SND },           // 11

	{ "obc_13.n6",	0x1000, 0xb4fcf9af, 4 | BRF_GRA },           // 12 Characters
	{ "obc_14.n4",	0x1000, 0xaf54407e, 4 | BRF_GRA },           // 13

	{ "obc_9.a6",	0x1000, 0xfa69e16e, 5 | BRF_GRA },           // 14 Sprites
	{ "obc_10.a2",	0x1000, 0x10359f84, 5 | BRF_GRA },           // 15
	{ "obc_11.a4",	0x1000, 0x1d968f5f, 5 | BRF_GRA },           // 16
	{ "obc_12.b2",	0x1000, 0xd8f0c157, 5 | BRF_GRA },           // 17

	{ "c-1.n2",		0x0020, 0xe488e831, 6 | BRF_GRA },           // 18 Color data
	{ "c-2.k6",		0x0100, 0x698a3ba0, 6 | BRF_GRA },           // 19
	{ "c-3.d6",		0x0100, 0xefc4e408, 6 | BRF_GRA },           // 20
};

STD_ROM_PICK(olibochu)
STD_ROM_FN(olibochu)

struct BurnDriver BurnDrvOlibochu = {
	"olibochu", NULL, NULL, NULL, "1981",
	"Oli-Boo-Chu (USA)\0", NULL, "Irem (GDI license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_IREM_MISC, GBF_MAZE, 0,
	NULL, olibochuRomInfo, olibochuRomName, NULL, NULL, NULL, NULL, OlibochuInputInfo, OlibochuDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	240, 256, 3, 4
};
