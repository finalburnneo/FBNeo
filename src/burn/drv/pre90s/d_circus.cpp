// FinalBurn Neo Exidy Circus driver module
// Based on MAME driver by Mike Coates

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "samples.h"
#include "dac.h"
#include "burn_gun.h" // spinner

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvSprROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvM6502RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 sprite_x;
static INT32 sprite_y;
static INT32 sprite_z;

static INT32 vblank;
static void (*sound_callback)(INT32 data) = NULL;
static void (*scanline_callback)(INT32 line) = NULL;

static UINT8 DrvJoy1[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[1];
static UINT8 DrvReset;

static INT16 Analog[2];

static INT32 is_ripcord = 0;
static INT32 is_robotbwl = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo CircusInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	A("P1 Spinner",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Circus)

static struct BurnInputInfo RobotbwlInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 right"	},
	{"P1 Hook Left",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left 2"	},
	{"P1 Hook Right",	BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right 2"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Robotbwl)

static struct BurnInputInfo CrashInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Crash)

static struct BurnInputInfo RipcordInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	A("P1 Spinner",     BIT_ANALOG_REL, &Analog[0],		"p1 x-axis"	),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A
STDINPUTINFO(Ripcord)

static struct BurnDIPInfo CircusDIPList[]=
{
	DIP_OFFSET(0x06)
	{0x00, 0xff, 0xff, 0x04, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x00, "3"					},
	{0x00, 0x01, 0x03, 0x01, "5"					},
	{0x00, 0x01, 0x03, 0x02, "7"					},
	{0x00, 0x01, 0x03, 0x03, "9"					},

	{0   , 0xfe, 0   ,    3, "Coinage"				},
	{0x00, 0x01, 0x0c, 0x0c, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "High Score"			},
	{0x00, 0x01, 0x10, 0x10, "Credit Awarded"		},
	{0x00, 0x01, 0x10, 0x00, "No Award"				},

	{0   , 0xfe, 0   ,    2, "Bonus"				},
	{0x00, 0x01, 0x20, 0x00, "Single Line"			},
	{0x00, 0x01, 0x20, 0x20, "Super Bonus"			},
};

STDDIPINFO(Circus)

static struct BurnDIPInfo RobotbwlDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x0c, NULL					},

	{0   , 0xfe, 0   ,    2, "Beer Frame"			},
	{0x00, 0x01, 0x04, 0x00, "Off"					},
	{0x00, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    3, "Coinage"				},
	{0x00, 0x01, 0x18, 0x10, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x18, 0x08, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x18, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Bowl Timer"			},
	{0x00, 0x01, 0x60, 0x00, "3 seconds"			},
	{0x00, 0x01, 0x60, 0x20, "5 seconds"			},
	{0x00, 0x01, 0x60, 0x40, "7 seconds"			},
	{0x00, 0x01, 0x60, 0x60, "9 seconds"			},
};

STDDIPINFO(Robotbwl)

static struct BurnDIPInfo CrashDIPList[]=
{
	DIP_OFFSET(0x09)
	{0x00, 0xff, 0xff, 0x05, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x00, "2"					},
	{0x00, 0x01, 0x03, 0x01, "3"					},
	{0x00, 0x01, 0x03, 0x02, "4"					},
	{0x00, 0x01, 0x03, 0x03, "5"					},

	{0   , 0xfe, 0   ,    3, "Coinage"				},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "High Score"			},
	{0x00, 0x01, 0x10, 0x00, "No Award"				},
	{0x00, 0x01, 0x10, 0x10, "Credit Awarded"		},
};

STDDIPINFO(Crash)

static struct BurnDIPInfo RipcordDIPList[]=
{
	DIP_OFFSET(0x07)
	{0x00, 0xff, 0xff, 0x07, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x00, "3"					},
	{0x00, 0x01, 0x03, 0x01, "5"					},
	{0x00, 0x01, 0x03, 0x02, "7"					},
	{0x00, 0x01, 0x03, 0x03, "9"					},

	{0   , 0xfe, 0   ,    3, "Coinage"				},
	{0x00, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x0c, 0x04, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x0c, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    2, "High Score"			},
	{0x00, 0x01, 0x10, 0x10, "Award Credit"			},
	{0x00, 0x01, 0x10, 0x00, "No Award"				},
};

STDDIPINFO(Ripcord)

static void play(INT32 sam, double volume)
{
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

	BurnSamplePlay(sam);
}

static void playif(INT32 sam, double volume)
{
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRoute(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

	if (BurnSampleGetStatus(sam) == SAMPLE_STOPPED)
		BurnSamplePlay(sam);
}

static void playifloop(INT32 sam, double volume, INT32 rate)
{
	BurnSampleSetRouteFade(sam, BURN_SND_SAMPLE_ROUTE_1, volume, BURN_SND_ROUTE_BOTH);
	BurnSampleSetRouteFade(sam, BURN_SND_SAMPLE_ROUTE_2, volume, BURN_SND_ROUTE_BOTH);

	BurnSampleSetLoop(sam, true);
	BurnSampleSetPlaybackRate(sam, rate);

	if (BurnSampleGetStatus(sam) == SAMPLE_STOPPED)
		BurnSamplePlay(sam);
}

static void circus_sound_write(INT32 data)
{
	if ((data & 0x80) == 0) { // audio enable
		switch ((data >> 4) & 7)
		{
			case 0: DACWrite(0, 0); break;
			case 1: DACWrite(0, 0x80); break;
			case 2: play(0, 0.55); break; // circus - pop, ripcord - splash
			case 3: break; // video - normal
			case 4: playif(1, 0.45); break; // circus - miss, ripcord - scream
			case 5: break; // video - inverted
			case 6: playif(2, 0.45); break; // circus - bounce - ripcord - chute open
			case 7: break; // circus - not used, ripcord see below
		}
	}
}

static void ripcord_sound_write(INT32 data)
{
	if ((data & 0x70) == 0x70) {
		playif(3, 0.75);
		return;
	}

	circus_sound_write(data);
}

static void robotbwl_sound_write(INT32 data)
{
	if ((data & 0x80) == 0) { // audio enable
		if (data & 0x01) BurnSamplePlay(4); // reward
		if (data & 0x02) BurnSamplePlay(3); // demerit
		//	if (data & 0x04) // invert
		//	if (data & 0x08) // discrete music bit off
		if (data & 0x10) BurnSamplePlay(2); // ball drop
		if (data & 0x20) BurnSamplePlay(1); // roll
		if (data & 0x40) BurnSamplePlay(0); //
	}
}

static void crash_sound_write(INT32 data)
{
	bool has_motor = false;

	if ((data & 0x80) == 0) { // audio enable
		switch ((data >> 4) & 7)
		{
			case 0: DACWrite(0, 0); break;
			case 1: DACWrite(0, 0x80); break;
			case 2: playif(0, 0.75); break; // crash
			case 3: playif(1, 1.75); break; // bip
			case 4: break; // skid
			case 5: playif(1, 1.75); break; // bip
			case 6: playifloop(2, 0.75, 100); has_motor = true; break; // hi motor
			case 7: playifloop(2, 0.75, 125); has_motor = true; break; // lo motor
		}
	}
	if (has_motor == false) {
		// fade out / no motor
		BurnSampleSetRouteFade(2, BURN_SND_SAMPLE_ROUTE_1, 0.00, BURN_SND_ROUTE_BOTH);
		BurnSampleSetRouteFade(2, BURN_SND_SAMPLE_ROUTE_2, 0.00, BURN_SND_ROUTE_BOTH);
	}
}

static void crash_scanline_callback(INT32 line)
{
	if (line == 239 || line == 255) {
		M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
	}
}

static void circus_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			sprite_x = 240 - data;
		return;

		case 0x3000:
			sprite_y = 240 - data;
		return;

		case 0x8000:
			sprite_z = (is_robotbwl) ? 0 : (data & 0xf);
			if (sound_callback) {
				sound_callback(data);
			}
		return;
	}
}

static UINT8 circus_read(UINT16 address)
{
	switch (address)
	{
		case 0x8000:
			return sprite_z; // ??

		case 0xa000:
			return DrvInputs[0];

		case 0xc000:
			return (DrvDips[0] & 0x7f) | (vblank ? 0x80 : 0);

		case 0xd000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return BurnTrackballRead(0);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnSampleReset();
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);
	DACReset();

	BurnTrackballSetResetDefault(115);
	BurnTrackballReadReset();

	sprite_x = 0;
	sprite_y = 0;
	sprite_z = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x001000;

	DrvGfxROM		= Next; Next += 0x004000;
	DrvSprROM		= Next; Next += 0x001000;

	DrvPalette		= (UINT32*)Next; Next += 0x0010 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x000400;
	DrvM6502RAM		= Next; Next += 0x000200;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 sprite_len)
{
	INT32 Planes[1] = { 0 };
	INT32 XOffs[16] = { STEP8(0,1), STEP8(128,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x00800);

	GfxDecode(0x100, 1,  8,  8, Planes, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	memcpy (tmp, DrvSprROM, 0x00200);

	if (sprite_len == 0x20) { // robotbwl
		for (INT32 i = 0; i < 0x20; i++) tmp[i] ^= 0xff; // invert
		GfxDecode(0x001, 1,  8,  8, Planes, XOffs, YOffs, 0x008, tmp, DrvSprROM);
	} else {
		GfxDecode(0x010, 1, 16, 16, Planes, XOffs, YOffs, 0x100, tmp, DrvSprROM);
	}

	BurnFree (tmp);

	return 0;
}

static INT32 DrvLoadRoms()
{
	char* pRomName;
	struct BurnRomInfo ri;
	UINT8 *pLoad[3] = { DrvM6502ROM, DrvGfxROM, DrvSprROM };

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);

		if ((ri.nType & 3) >= 1 && (ri.nType & 3) <= 3)
		{
			if (BurnLoadRom(pLoad[(ri.nType - 1) & 3], i, 1)) return 1;
			pLoad[(ri.nType - 1) & 3] += ri.nLen;
			continue;
		}
	}

	if (DrvGfxDecode(pLoad[2] - DrvSprROM)) return 1;

	return 0;
}

static tilemap_callback( bg )
{
	INT32 code = DrvVidRAM[offs];

	TILE_SET_INFO(0, code, 0, 0);
}

static INT32 DrvInit(void (*sound_cb)(INT32), void (*scanline_cb)(INT32))
{
	BurnSetRefreshRate(60.00); // 60hz for proper sound sync in circus. 57 fks up, do not touch -dink

	BurnAllocMemIndex();

	if (DrvLoadRoms()) return 1;

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,					0x0000, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,					0x1000, 0x1fff, MAP_ROM);
	M6502MapMemory(DrvVidRAM,					0x4000, 0x43ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,					0xf000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(circus_write);
	M6502SetReadHandler(circus_read);
	M6502Close();

	BurnSampleInit(0);
	BurnSampleSetBuffered(M6502TotalCycles, 705562);
	BurnSampleSetAllRoutesAllSamples(1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6502TotalCycles, 705562);
	DACSetRoute(0, 0.75, BURN_SND_ROUTE_BOTH);
	DACDCBlock(1);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM, 1, 8, 8, 0x4000, 0, 0);

	BurnTrackballInit(1);

	sound_callback = sound_cb;
	scanline_callback = scanline_cb;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	BurnSampleExit();
	DACExit();

	BurnTrackballExit();

	BurnFreeMemIndex();

	sound_callback = NULL;
	scanline_callback = NULL;

	is_ripcord = 0;
	is_robotbwl = 0;

	return 0;
}

static void DrvPaletteUpdate()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = BurnHighCol(0xff, 0xff, 0xff, 0);
		DrvRecalc = 0;
	}
}

static void draw_line(INT32 x1, INT32 y1, INT32 x2, INT32 y2, INT32 dotted)
{
	INT32 skip = (dotted > 0) ? 2 : 1;

	if (x1 == x2) {
		if (x1 >= 0 && x1 < nScreenWidth) {
			for (INT32 count = y2; count >= y1; count -= skip) {
				if (count >= 0 && count < nScreenHeight) {
					pTransDraw[count * nScreenWidth + x1] = 1;
				}
			}
		}
	} else {
		if (y1 >= 0 && y1 < nScreenHeight) {
			for (INT32 count = x2; count >= x1; count -= skip) {
				if (count >= 0 && count <= nScreenWidth) {
					pTransDraw[y1 * nScreenWidth + count] = 1;
				}
			}
		}
	}
}

static void circus_draw_fg()
{
	draw_line(0,    18, 255,  18, 0);
	draw_line(0,   249, 255, 249, 1);
	draw_line(0,    18,   0, 248, 0);
	draw_line(247,  18, 247, 248, 0);
	draw_line(0,   136,  17, 136, 0);
	draw_line(231, 136, 248, 136, 0);
	draw_line(0,   192,  17, 192, 0);
	draw_line(231, 192, 248, 192, 0);
}

static void robotbwl_draw_box(INT32 x, INT32 y)
{
	draw_line(x,      y,      x + 24, y,      0);
	draw_line(x,      y + 26, x + 24, y + 26, 0);
	draw_line(x,      y,      x,      y + 26, 0);
	draw_line(x + 24, y,      x + 24, y + 26, 0);
	draw_line(x + 8,  y + 10, x + 24, y + 10, 0);
	draw_line(x + 8,  y,      x + 8,  y + 10, 0);
	draw_line(x + 16, y,      x + 16, y + 10, 0);
}

static void robotbwl_draw_scoreboard()
{
	for (INT32 offs = 15; offs <= 63; offs += 24)
	{
		robotbwl_draw_box(offs, 31);
		robotbwl_draw_box(offs, 63);
		robotbwl_draw_box(offs, 95);
		robotbwl_draw_box(offs + 152, 31);
		robotbwl_draw_box(offs + 152, 63);
		robotbwl_draw_box(offs + 152, 95);
	}

	robotbwl_draw_box(39, 127);
	draw_line(39, 137, 47, 137, 0);
	robotbwl_draw_box(39 + 152, 127);
	draw_line(39 + 152, 137, 47 + 152, 137, 0);
}

static void robotbwl_draw_bowling_alley()
{
	draw_line(103, 17, 103, 205, 0);
	draw_line(111, 17, 111, 203, 1);
	draw_line(152, 17, 152, 205, 0);
	draw_line(144, 17, 144, 203, 1);
}

static void circus_draw_sprite()
{
	INT32 collision = 0;

	for (INT32 sy = 0; sy < 16; sy++) {
		INT32 dy = sprite_x + sy-1;

		if (dy >= 0 && dy < nScreenHeight) {
			for (INT32 sx = 0; sx < 16; sx++) {
				INT32 dx = sprite_y + sx;

				if (dx >= 0 && dx < nScreenWidth) {
					INT32 pixel = DrvSprROM[(sprite_z * 16 * 16) + sy * 16 + sx];

					if (pixel) {
						collision |= pTransDraw[(dy * nScreenWidth) + dx];
						pTransDraw[(dy * nScreenWidth) + dx] = pixel;
					}
				}
			}
		}
	}

	if (collision) {  // plays hell with synchronicity DINKFIXME
		M6502Open(0);
		M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6502Close();
	}
}

static void draw_sprite(INT32 size, INT32 xadj, INT32 yadj)
{
	DrawCustomMaskTile(pTransDraw, size, size, sprite_z, sprite_y + xadj, sprite_x + yadj, 0, 0, 0, 1, 0, 0, DrvSprROM);
}

static void do_color(INT32 x, INT32 y, INT32 color, INT32 bgcolor)
{
	UINT16 *p = (UINT16*)&pTransDraw[(y * nScreenWidth) + x];
	if (p[0] == 1) p[0] = color;
	if (p[0] == 0) p[0] = bgcolor;
}

static void circus_colorize()
{
	for (INT32 y = 0; y < nScreenHeight; y++) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			if (y >= 20 && y <= 31) do_color(x, y, 4, 0); // top balloons (blueish)
			if (y >= 32 && y <= 47) do_color(x, y, 5, 0); // middle balloons (greenish)
			if (y >= 48 && y <= 63) do_color(x, y, 6, 0); // bottom balloons (yellowish)
		}
	}
}

static INT32 CircusDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvPalette[4] = BurnHighCol(0x36, 0xa4, 0xe9, 0); // blueish
		DrvPalette[5] = BurnHighCol(0x52, 0xff, 0x52, 0); // greenish
		DrvPalette[6] = BurnHighCol(0xff, 0xff, 0x52, 0); // yellowish
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ( nBurnLayer & 2) circus_draw_fg();

	if ( nSpriteEnable & 1) circus_draw_sprite();

	if (nBurnLayer & 8) circus_colorize();

	if (pBurnDraw) BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 RobotbwlDraw()
{
	DrvPaletteUpdate();

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ( nBurnLayer & 2) robotbwl_draw_scoreboard();
	if ( nBurnLayer & 4) robotbwl_draw_bowling_alley();

	if ( nSpriteEnable & 1) draw_sprite(8, 8, 8);

	if (pBurnDraw) BurnTransferCopy(DrvPalette);

	return 0;
}

static void crash_colorize()
{
	for (INT32 y = 0; y < nScreenHeight; y++) {
		for (INT32 x = 0; x < nScreenWidth; x++) {
			if (y <= 85 || y >= 171) do_color(x, y, 4, 0);
			else if (x <= 85 || x >= 161) do_color(x, y, 4, 0);
			else do_color(x, y, 5, 6);

		}
	}
}

static INT32 CrashDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvPalette[4] = BurnHighCol(0x01, 0x91, 0xfe, 0); // blueish
		DrvPalette[5] = BurnHighCol(0xfe, 0xfe, 0x7f, 0); // yellowish
		DrvPalette[6] = BurnHighCol(0x0a, 0x0a, 0x05, 0); // grey-bg ish
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ( nSpriteEnable & 1) draw_sprite(16, 0, -1);

	if (nBurnLayer & 8) crash_colorize();

	if (pBurnDraw) BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 RipcordDraw()
{
	DrvPaletteUpdate();

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if ( nSpriteEnable & 1) circus_draw_sprite();

	if (pBurnDraw) BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		BurnTrackballConfig(0, (is_ripcord) ? AXIS_REVERSED : AXIS_NORMAL, AXIS_NORMAL);
		BurnTrackballConfigStartStopPoints(0, 64, 167, 64, 167);
		BurnTrackballFrame(0, Analog[0], 0, 0, 0x3f);
		BurnTrackballUpdate(0);
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 705562 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		vblank = (i >= 240) ? 1 : 0;

		CPU_RUN(0, M6502);

		if (scanline_callback) {
			scanline_callback(i);
		}
	}

	M6502Close();

	if (pBurnSoundOut) {
		BurnSampleRender(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	BurnDrvRedraw(); // pBurnDraw check in *draw() :) (needed for collision det. / irq generation)

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

		M6502Scan(nAction);
		BurnSampleScan(nAction, pnMin);
		DACScan(nAction, pnMin);
		BurnTrackballScan();

		SCAN_VAR(sprite_x);
		SCAN_VAR(sprite_y);
		SCAN_VAR(sprite_z);
	}

	return 0;
}


// Circus / Acrobat TV

static struct BurnRomInfo circusRomDesc[] = {
	{ "9004.1a",			0x0200, 0x7654ea75, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "9005.2a",			0x0200, 0xb8acdbc5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9006.3a",			0x0200, 0x901dfff6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "9007.5a",			0x0200, 0x9dfdae38, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9008.6a",			0x0200, 0xc8681cf6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "9009.7a",			0x0200, 0x585f633e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "9010.8a",			0x0200, 0x69cc409f, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "9011.9a",			0x0200, 0xaff835eb, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "9003.4c",			0x0200, 0x6efc315a, 2 | BRF_GRA },           //  8 Background Tiles
	{ "9002.3c",			0x0200, 0x30d72ef5, 2 | BRF_GRA },           //  9
	{ "9001.2c",			0x0200, 0x361da7ee, 2 | BRF_GRA },           // 10
	{ "9000.1c",			0x0200, 0x1f954bb3, 2 | BRF_GRA },           // 11

	{ "9012.14d",			0x0200, 0x2fde3930, 3 | BRF_GRA },           // 12 Sprites
};

STD_ROM_PICK(circus)
STD_ROM_FN(circus)

static struct BurnSampleInfo CircusSampleDesc[] = {
	{ "pop", 		SAMPLE_NOLOOP },
	{ "miss", 		SAMPLE_NOLOOP },
	{ "bounce", 	SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Circus)
STD_SAMPLE_FN(Circus)

static INT32 CircusInit()
{
	return DrvInit(circus_sound_write, NULL);
}

struct BurnDriver BurnDrvCircus = {
	"circus", NULL, NULL, "circus", "1977",
	"Circus / Acrobat TV\0", NULL, "Exidy / Taito", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, circusRomInfo, circusRomName, NULL, NULL, CircusSampleInfo, CircusSampleName, CircusInputInfo, CircusDIPInfo,
	CircusInit, DrvExit, DrvFrame, CircusDraw, DrvScan, &DrvRecalc, 2,
	248, 256, 4, 3
};


// Springboard (bootleg of Circus)

static struct BurnRomInfo springbdRomDesc[] = {
	{ "93448.1a",			0x0200, 0x44d65ccd, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "93448.2a",			0x0200, 0xb8acdbc5, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "93448.3a",			0x0200, 0xf2e25f7a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "93448.5a",			0x0200, 0x9dfdae38, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "93448.6a",			0x0200, 0xc8681cf6, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "93448.7a",			0x0200, 0x585f633e, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "93448.8a",			0x0200, 0xd7c0dc05, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "93448.9a",			0x0200, 0xaff835eb, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "93448.4c",			0x0200, 0x6efc315a, 2 | BRF_GRA },           //  8 Background Tiles
	{ "93448.3c",			0x0200, 0x30d72ef5, 2 | BRF_GRA },           //  9
	{ "93448.2c",			0x0200, 0x361da7ee, 2 | BRF_GRA },           // 10
	{ "93448.1c",			0x0200, 0x1f954bb3, 2 | BRF_GRA },           // 11

	{ "93448.14d",			0x0200, 0x2fde3930, 3 | BRF_GRA },           // 12 Sprites
};

STD_ROM_PICK(springbd)
STD_ROM_FN(springbd)

struct BurnDriver BurnDrvSpringbd = {
	"springbd", "circus", NULL, "circus", "1977",
	"Springboard (bootleg of Circus)\0", NULL, "bootleg (Sub-Electro)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, springbdRomInfo, springbdRomName, NULL, NULL, CircusSampleInfo, CircusSampleName, CircusInputInfo, CircusDIPInfo,
	CircusInit, DrvExit, DrvFrame, CircusDraw, DrvScan, &DrvRecalc, 2,
	248, 256, 4, 3
};


// Robot Bowl

static struct BurnRomInfo robotbwlRomDesc[] = {
	{ "robotbwl.1a",		0x0200, 0xdf387a0b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "robotbwl.2a",		0x0200, 0xc948274d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "robotbwl.3a",		0x0200, 0x8fdb3ec5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "robotbwl.5a",		0x0200, 0xba9a6929, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "robotbwl.6a",		0x0200, 0x16fd8480, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "robotbwl.7a",		0x0200, 0x4cadbf06, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "robotbwl.8a",		0x0200, 0xbc809ed3, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "robotbwl.9a",		0x0200, 0x07487e27, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "robotbwl.4c",		0x0200, 0xa5f7acb9, 2 | BRF_GRA },           //  8 Background Tiles
	{ "robotbwl.3c",		0x0200, 0xd5380c9b, 2 | BRF_GRA },           //  9
	{ "robotbwl.2c",		0x0200, 0x47b3e39c, 2 | BRF_GRA },           // 10
	{ "robotbwl.1c",		0x0200, 0xb2991e7e, 2 | BRF_GRA },           // 11

	{ "robotbwl.14d",		0x0020, 0xa402ac06, 3 | BRF_GRA },           // 12 Sprites
};

STD_ROM_PICK(robotbwl)
STD_ROM_FN(robotbwl)

/*
samples don't exist yet.
static struct BurnSampleInfo RobotbwlSampleDesc[] = {
	{ "hit", 		SAMPLE_NOLOOP },
	{ "roll", 		SAMPLE_NOLOOP },
	{ "balldrop", 	SAMPLE_NOLOOP },
	{ "demerit", 	SAMPLE_NOLOOP },
	{ "reward", 	SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Robotbwl)
STD_SAMPLE_FN(Robotbwl)
*/

static INT32 RobotbwlInit()
{
	is_robotbwl = 1;
	return DrvInit(robotbwl_sound_write, NULL);
}

struct BurnDriver BurnDrvRobotbwl = {
	"robotbwl", NULL, NULL, NULL, "1977",
	"Robot Bowl\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, robotbwlRomInfo, robotbwlRomName, NULL, NULL, NULL, NULL, RobotbwlInputInfo, RobotbwlDIPInfo,
	RobotbwlInit, DrvExit, DrvFrame, RobotbwlDraw, DrvScan, &DrvRecalc, 2,
	248, 256, 4, 3
};


// Crash

static struct BurnRomInfo crashRomDesc[] = {
	{ "crash.a1",			0x0200, 0xb9571203, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "crash.a2",			0x0200, 0xb4581a95, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "crash.a3",			0x0200, 0x597555ae, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "crash.a4",			0x0200, 0x0a15d69f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "crash.a5",			0x0200, 0xa9c7a328, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "crash.a6",			0x0200, 0xc7d62d27, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "crash.a7",			0x0200, 0x5e5af244, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "crash.a8",			0x0200, 0x3dc50839, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "crash.c4",			0x0200, 0xba16f9e8, 2 | BRF_GRA },           //  8 Background Tiles
	{ "crash.c3",			0x0200, 0x3c8f7560, 2 | BRF_GRA },           //  9
	{ "crash.c2",			0x0200, 0x38f3e4ed, 2 | BRF_GRA },           // 10
	{ "crash.c1",			0x0200, 0xe9adf1e1, 2 | BRF_GRA },           // 11

	{ "crash.d14",			0x0200, 0x833f81e4, 3 | BRF_GRA },           // 12 Sprites
};

STD_ROM_PICK(crash)
STD_ROM_FN(crash)

static struct BurnSampleInfo CrashSampleDesc[] = {
	{ "crash", 		SAMPLE_NOLOOP },
	{ "bip", 		SAMPLE_NOLOOP },
	{ "motor", 		SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Crash)
STD_SAMPLE_FN(Crash)

static INT32 CrashInit()
{
	return DrvInit(crash_sound_write, crash_scanline_callback);
}

struct BurnDriver BurnDrvCrash = {
	"crash", NULL, NULL, "crash", "1979",
	"Crash\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, crashRomInfo, crashRomName, NULL, NULL, CrashSampleInfo, CrashSampleName, CrashInputInfo, CrashDIPInfo,
	CrashInit, DrvExit, DrvFrame, CrashDraw, DrvScan, &DrvRecalc, 2,
	248, 256, 4, 3
};


// Crash (Alt)

static struct BurnRomInfo crashaRomDesc[] = {
	{ "nsa7.a8",			0x0800, 0x2e47c5ee, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "nsa3.a4",			0x0800, 0x11c8c461, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "nsc2.c2",			0x0800, 0xa16cd133, 2 | BRF_GRA },           //  2 Background Tiles

	{ "sd14.d14",			0x0200, 0x833f81e4, 3 | BRF_GRA },           //  3 Sprites
};

STD_ROM_PICK(crasha)
STD_ROM_FN(crasha)

struct BurnDriver BurnDrvCrasha = {
	"crasha", "crash", NULL, "crash", "1979",
	"Crash (Alt)\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, crashaRomInfo, crashaRomName, NULL, NULL, CrashSampleInfo, CrashSampleName, CrashInputInfo, CrashDIPInfo,
	CrashInit, DrvExit, DrvFrame, CrashDraw, DrvScan, &DrvRecalc, 2,
	248, 256, 4, 3
};


// Smash (Crash bootleg)

static struct BurnRomInfo smashRomDesc[] = {
	{ "smash.a1",			0x0200, 0xb9571203, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "smash.a2",			0x0200, 0xb4581a95, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "smash.a3",			0x0200, 0x597555ae, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "smash.a4",			0x0200, 0x0a15d69f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "smash.a5",			0x0200, 0xa9c7a328, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "smash.a6",			0x0200, 0xc7d62d27, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "smash.a7",			0x0200, 0x5e5af244, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "smash.a8",			0x0200, 0x3dc50839, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "smash.c4",			0x0200, 0x442500e5, 2 | BRF_GRA },           //  8 Background Tiles
	{ "smash.c3",			0x0200, 0x3c8f7560, 2 | BRF_GRA },           //  9
	{ "smash.c2",			0x0200, 0x38f3e4ed, 2 | BRF_GRA },           // 10
	{ "smash.c1",			0x0200, 0xe9adf1e1, 2 | BRF_GRA },           // 11

	{ "smash.d14",			0x0200, 0x833f81e4, 3 | BRF_GRA },           // 12 Sprites
};

STD_ROM_PICK(smash)
STD_ROM_FN(smash)

struct BurnDriver BurnDrvSmash = {
	"smash", "crash", NULL, "crash", "1979",
	"Smash (Crash bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, smashRomInfo, smashRomName, NULL, NULL, CrashSampleInfo, CrashSampleName, CrashInputInfo, CrashDIPInfo,
	CrashInit, DrvExit, DrvFrame, CrashDraw, DrvScan, &DrvRecalc, 2,
	248, 256, 4, 3
};


// Rip Cord

static struct BurnRomInfo ripcordRomDesc[] = {
	{ "9027.1a",			0x0200, 0x56b8dc06, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "9028.2a",			0x0200, 0xa8a78a30, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "9029.4a",			0x0200, 0xfc5c8e07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "9030.5a",			0x0200, 0xb496263c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "9031.6a",			0x0200, 0xcdc7d46e, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "9032.7a",			0x0200, 0xa6588bec, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "9033.8a",			0x0200, 0xfd49b806, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "9034.9a",			0x0200, 0x7caf926d, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "9026.5c",			0x0200, 0x06e7adbb, 2 | BRF_GRA },           //  8 Background Tiles
	{ "9025.4c",			0x0200, 0x3129527e, 2 | BRF_GRA },           //  9
	{ "9024.2c",			0x0200, 0xbcb88396, 2 | BRF_GRA },           // 10
	{ "9023.1c",			0x0200, 0x9f86ed5b, 2 | BRF_GRA },           // 11
	
	{ "9035.14d",			0x0200, 0xc9979802, 3 | BRF_GRA },           // 12 Sprites
};

STD_ROM_PICK(ripcord)
STD_ROM_FN(ripcord)
/*
samples don't exist.
static struct BurnSampleInfo RipcordSampleDesc[] = {
	{ "splash", 	SAMPLE_NOLOOP },
	{ "scream", 	SAMPLE_NOLOOP },
	{ "chute", 		SAMPLE_NOLOOP },
	{ "whistle", 	SAMPLE_NOLOOP },
	{ "", 0 }
};

STD_SAMPLE_PICK(Ripcord)
STD_SAMPLE_FN(Ripcord)
*/

static INT32 RipcordInit()
{
	is_ripcord = 1;
	return DrvInit(ripcord_sound_write, NULL);
}

struct BurnDriver BurnDrvRipcord = {
	"ripcord", NULL, NULL, NULL, "1979",
	"Rip Cord\0", NULL, "Exidy", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_ACTION, 0,
	NULL, ripcordRomInfo, ripcordRomName, NULL, NULL, NULL, NULL, RipcordInputInfo, RipcordDIPInfo,
	RipcordInit, DrvExit, DrvFrame, RipcordDraw, DrvScan, &DrvRecalc, 2,
	248, 256, 4, 3
};
