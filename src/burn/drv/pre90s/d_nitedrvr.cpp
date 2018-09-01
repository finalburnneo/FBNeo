// FB Alpha Atari Nite Driver Module
// Based on MAME driver by Mike Balfour

// to do:
//	hook up samples?

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvHVCRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 crash_en;
static UINT8 crash_data_en;
static UINT8 crash_data;
static INT32 crash_timer;
static UINT8 steering_val;
static UINT8 last_steering_val;
static INT32 steering_buf;
static UINT8 ac_line;
static INT32 m_gear;
static UINT8 m_track;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2f[8];
static UINT8 DrvJoy3f[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo NitedrvrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 start"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3f + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3f + 0,	"p1 right"	},
	{"P1 Accelerator",	BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 1"	},
	{"P1 Gear Up",		BIT_DIGITAL,	DrvJoy2f + 0,	"p1 fire 2"	},
	{"P1 Gear Down",	BIT_DIGITAL,	DrvJoy2f + 1,	"p1 fire 3"	},
	{"P1 Novice Track",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 4"	},
	{"P1 Expert Track",	BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 5"	},
	{"P1 Pro Track",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 6"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Nitedrvr)

static struct BurnDIPInfo NitedrvrDIPList[]=
{
	{0x0b, 0xff, 0xff, 0x90, NULL					},
	{0x0c, 0xff, 0xff, 0xa0, NULL					},
	{0x0d, 0xff, 0xff, 0x00, NULL					},

	{0   , 0xfe, 0   ,    3, "Coinage"				},
	{0x0b, 0x01, 0x30, 0x30, "2 Coins 1 Credits"	},
	{0x0b, 0x01, 0x30, 0x10, "1 Coin  1 Credits"	},
	{0x0b, 0x01, 0x30, 0x00, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Playing Time"			},
	{0x0b, 0x01, 0xc0, 0x00, "50"					},
	{0x0b, 0x01, 0xc0, 0x40, "75"					},
	{0x0b, 0x01, 0xc0, 0x80, "100"					},
	{0x0b, 0x01, 0xc0, 0xc0, "125"					},

	{0   , 0xfe, 0   ,    2, "Track Set"			},
	{0x0c, 0x01, 0x10, 0x00, "Normal"				},
	{0x0c, 0x01, 0x10, 0x10, "Reverse"				},

	{0   , 0xfe, 0   ,    2, "Bonus Time"			},
	{0x0c, 0x01, 0x20, 0x00, "No"					},
	{0x0c, 0x01, 0x20, 0x20, "Score = 350"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0c, 0x01, 0x80, 0x00, "On"					},
	{0x0c, 0x01, 0x80, 0x80, "Off"					},

	{0   , 0xfe, 0   ,    2, "Difficult Bonus"		},
	{0x0d, 0x01, 0x20, 0x00, "Normal"				},
	{0x0d, 0x01, 0x20, 0x20, "Difficult"			},
};

STDDIPINFO(Nitedrvr)

static INT32 nitedrvr_steering()
{
	if (DrvJoy3f[0])
	{
		steering_val = 0xc0;
	}
	else if (DrvJoy3f[1])
	{
		steering_val = 0x80;
	}
	else
	{
		steering_val = 0x00;
	}

	return steering_val;
}

static UINT8 nitedrvr_in0_r(UINT8 offset)
{
	{ // gear logic
		static INT32 last = 0;
		if ((last & (1<<0)) == 0 && DrvInputs[1] & (1<<0)) {
			m_gear++;
		}
		if ((last & (1<<1)) == 0 && DrvInputs[1] & (1<<1)) {
			m_gear--;
		}
		last = DrvInputs[1];

		if (m_gear < 1) m_gear = 1;
		if (m_gear > 4) m_gear = 4;
	}

	switch (offset & 0x03)
	{
		case 0x00:
			return DrvDips[0];

		case 0x01:
			return (DrvDips[1] & ~0x40) | (vblank ? 0x40 : 0);

		case 0x02:
			if (m_gear == 1)
				return 0xe0;
			else if (m_gear == 2)
				return 0xd0;
			else if (m_gear == 3)
				return 0xb0;
			else
				return 0x70;

		case 0x03:
			return ((DrvDips[2] & 0x20) | nitedrvr_steering());
	}

	return 0xff;
}

static UINT8 nitedrvr_in1_r(UINT8 offset)
{
	INT32 port = DrvInputs[0] ^ 0x0f;

	ac_line = (ac_line + 1) % 3;

	if (port & 0x10) m_track = 0;
	else if (port & 0x20) m_track = 1;
	else if (port & 0x40) m_track = 2;

	switch (offset & 0x07)
	{
		case 0x00:
			return ((port & 0x01) << 7);

		case 0x01:
			return ((port & 0x02) << 6);

		case 0x02:
			return ((port & 0x04) << 5);

		case 0x03:
			return ((port & 0x08) << 4);

		case 0x04:
			if (m_track == 1) return 0x80; else return 0x00;

		case 0x05:
			if (m_track == 0) return 0x80; else return 0x00;

		case 0x06:
			if (ac_line == 0) return 0x80; else return 0x00;

		case 0x07:
			return 0x00;
	}

	return 0xff;
}

static void out1_write(UINT8 data)
{
//	led = BIT(data, 4);

	crash_en = data & 0x01;

	//m_discrete->write(space, NITEDRVR_CRASH_EN,   data & 0x01); // Crash enable
	//m_discrete->write(space, NITEDRVR_ATTRACT_EN, data & 0x02);      // Attract enable (sound disable)
	//m_discrete->write(space, NITEDRVR_BANG_DATA, crash_data_en ? crash_data : 0);    // Crash Volume

	if ((data & 1) == 0)
	{
		crash_data_en = 1;
		crash_data = 0x0f;

		DrvPalette[1] = 0;
		DrvPalette[0] = ~0;
	}
}

static void nitedrvr_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfe00) == 0x0200) {
		DrvVidRAM[address & 0x7f] = data;
		return;
	}

	if ((address & 0xfe00) == 0x0400)
	{
		address &= 0x3f;

		if (address <= 0x042f) {
			DrvHVCRAM[address] = data;
			return;
		}
		BurnWatchogWrite();
		return;
	}

	if ((address & 0xfe00) == 0x0a00) {
	//	m_discrete->write(space, NITEDRVR_MOTOR_DATA, data & 0x0f);  // Motor freq data
	//	m_discrete->write(space, NITEDRVR_SKID1_EN, data & 0x10);    // Skid1 enable
	//	m_discrete->write(space, NITEDRVR_SKID2_EN, data & 0x20);    // Skid2 enable
		return;
	}

	if ((address & 0xfe00) == 0x0c00) {
		out1_write(data);
		return;
	}

	if ((address & 0xfc00) == 0x8400) {
		steering_val = 0;
		return;
	}
}

static UINT8 nitedrvr_read(UINT16 address)
{
	if ((address & 0xfe00) == 0x0600) {
		return nitedrvr_in0_r(address);
	}

	if ((address & 0xfe00) == 0x0800) {
		return nitedrvr_in1_r(address);
	}

	if ((address & 0xfc00) == 0x8000) {
		return DrvVidRAM[address & 0x7f];
	}

	if ((address & 0xfc00) == 0x8400) {
		steering_val = 0;
		return 0;
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs], 0, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();

	DrvPalette[1] = ~0; // white

	crash_en = 0;
	crash_data_en = 0;
	crash_data = 0;
	crash_timer = 0;
	steering_buf = 0;
	ac_line = 0;
	m_track = 0;
	steering_val = 0;
	last_steering_val = 0;
	m_gear = 1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x001000;

	DrvGfxROM		= Next; Next += 0x001000;

	AllRam			= Next;

	DrvPalette		= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);

	DrvM6502RAM		= Next; Next += 0x000100;
	DrvVidRAM		= Next; Next += 0x000080;
	DrvHVCRAM		= Next; Next += 0x000080;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x200);

	GfxDecode(0x040, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(57.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvM6502ROM + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0800,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x0000,  2, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,		0x0000, 0x00ff, MAP_RAM);
	M6502MapMemory(DrvM6502RAM,		0x0100, 0x01ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,		0x9000, 0x9fff, MAP_ROM);
	M6502MapMemory(DrvM6502ROM + 0x0f00,	0xff00, 0xffff, MAP_ROM); // vectors
	M6502SetWriteHandler(nitedrvr_write);
	M6502SetReadHandler(nitedrvr_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180); // ?

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 4);
	GenericTilemapSetGfx(0, DrvGfxROM, 1, 8, 8, 0x1000, 0, 0);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	BurnFree(AllMem);

	return 0;
}

static void draw_roadway()
{
	for (INT32 roadway = 0; roadway < 16; roadway++)
	{
		INT32 bx = DrvHVCRAM[roadway];
		INT32 by = DrvHVCRAM[roadway + 16];
		INT32 ex = bx + ((DrvHVCRAM[roadway + 32] & 0xf0) >> 4);
		INT32 ey = by + (16 - (DrvHVCRAM[roadway + 32] & 0x0f));

		if (ey > nScreenHeight) ey = nScreenHeight;
		if (ex > nScreenWidth) ex = nScreenWidth;

		for (INT32 y = by; y < ey; y++)
		{
			UINT16 *dst = pTransDraw + (y * nScreenWidth);

			for (INT32 x = bx; x < ex; x++)
			{
				dst[x] = 1;
			}
		}
	}
}

static INT32 DrvDraw()
{
	// palette updated in routines

	BurnTransferClear();
	
	GenericTilesSetClip(-1, -1, -1, 31);
	GenericTilemapDraw(0, pTransDraw, 0);
	GenericTilesClearClip();

	draw_roadway();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void crash_toggle_callback()
{
	if (crash_en && crash_data_en)
	{
		crash_data--;

		// m_discrete->write(space, NITEDRVR_BANG_DATA, m_crash_data);  // Crash Volume

		if (!crash_data)
			crash_data_en = 0;    // Done counting?

		DrvPalette[0 ^ (crash_data & 0x01)] = 0;
		DrvPalette[1 ^ (crash_data & 0x01)] = ~0;
	}
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0;
		DrvInputs[1] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2f[i] & 1) << i;
		}
	}

	M6502Open(0);
	vblank = 0;
	M6502Run(16285);
	vblank = 1;
	M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
	M6502Run(1399);
	M6502Close();

	crash_timer++;
	if (crash_timer == 7) {
		crash_toggle_callback();
		crash_timer = 0;
	}

	if (pBurnSoundOut) {

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

		M6502Scan(nAction);
		BurnWatchdogScan(nAction);;

		SCAN_VAR(crash_en);
		SCAN_VAR(crash_data_en);
		SCAN_VAR(crash_data);
		SCAN_VAR(crash_timer);
		SCAN_VAR(steering_val);
		SCAN_VAR(last_steering_val);
		SCAN_VAR(steering_buf);
		SCAN_VAR(ac_line);
		SCAN_VAR(m_gear);
		SCAN_VAR(m_track);
	}

	return 0;
}


// Night Driver

static struct BurnRomInfo nitedrvrRomDesc[] = {
	{ "006569-01.d2",	0x0800, 0x7afa7542, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "006570-01.f2",	0x0800, 0xbf5d77b1, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "006568-01.p2",	0x0200, 0xf80d8889, 2 | BRF_GRA },           //  2 Tile Graphics

	{ "006559-01.h7",	0x0100, 0x5a8d0e42, 0 | BRF_OPT },           //  3 Sync PROM
};

STD_ROM_PICK(nitedrvr)
STD_ROM_FN(nitedrvr)

struct BurnDriver BurnDrvNitedrvr = {
	"nitedrvr", NULL, NULL, NULL, "1976",
	"Night Driver\0", "No sound", "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, nitedrvrRomInfo, nitedrvrRomName, NULL, NULL, NitedrvrInputInfo, NitedrvrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	256, 256, 4, 3
};
