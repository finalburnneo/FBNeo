// FB Alpha Tag Team / The Big Pro Wrestling driver module
// Based on MAME driver by Steve Ellenoff and Brad Oliver

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "ay8910.h"
#include "dac.h"
#include "resnet.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvM6502RAM0;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sound_nmi_mask;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 palette_bank;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo BigprowrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Bigprowr)

static struct BurnDIPInfo BigprowrDIPList[]=
{
	{0x11, 0xff, 0xff, 0x0f, NULL						},
	{0x12, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3 Credits"		},

	{0   , 0xfe, 0   ,    2, "Unused"					},
	{0x11, 0x01, 0x10, 0x00, "Off"						},
	{0x11, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    3, "Cabinet"					},
	{0x11, 0x01, 0x60, 0x00, "Upright"					},
	{0x11, 0x01, 0x60, 0x40, "Upright, Dual Controls"	},
	{0x11, 0x01, 0x60, 0x60, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x12, 0x01, 0x01, 0x01, "Normal"					},
	{0x12, 0x01, 0x01, 0x00, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x12, 0x01, 0x02, 0x02, "Off"						},
	{0x12, 0x01, 0x02, 0x00, "On"						},
};

STDDIPINFO(Bigprowr)

static struct BurnDIPInfo TagteamDIPList[]=
{
	{0x11, 0xff, 0xff, 0x0f, NULL						},
	{0x12, 0xff, 0xff, 0x01, NULL						},

	{0   , 0xfe, 0   ,    4, "Coin A"					},
	{0x11, 0x01, 0x03, 0x00, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x03, 0x03, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x03, 0x02, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x03, 0x01, "1 Coin  3/6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"					},
	{0x11, 0x01, 0x0c, 0x00, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x0c, 0x08, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x0c, 0x04, "1 Coin  3/6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Unused"					},
	{0x11, 0x01, 0x10, 0x00, "Off"						},
	{0x11, 0x01, 0x10, 0x10, "On"						},

	{0   , 0xfe, 0   ,    3, "Cabinet"					},
	{0x11, 0x01, 0x60, 0x00, "Upright"					},
	{0x11, 0x01, 0x60, 0x40, "Upright, Dual Controls"	},
	{0x11, 0x01, 0x60, 0x60, "Cocktail"					},

	{0   , 0xfe, 0   ,    2, "Difficulty"				},
	{0x12, 0x01, 0x01, 0x01, "Normal"					},
	{0x12, 0x01, 0x01, 0x00, "Hard"						},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"				},
	{0x12, 0x01, 0x02, 0x02, "Off"						},
	{0x12, 0x01, 0x02, 0x00, "On"						},

	{0   , 0xfe, 0   ,    2, "Coin Mode"				},
	{0x12, 0x01, 0xe0, 0x00, "Mode 1"					},
	{0x12, 0x01, 0xe0, 0x80, "Mode 2"					},
};

STDDIPINFO(Tagteam)

static inline INT32 mirror_addr(UINT16 offset)
{
	return (((offset >> 5) & 0x1f) | ((offset & 0x1f) << 5));
}

static void tagteam_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x4000) {
		DrvVidRAM[mirror_addr(address)] = data;
		return;
	}

	if ((address & 0xfc00) == 0x4400) {
		DrvColRAM[mirror_addr(address)] = data;
		return;
	}

	switch (address)
	{
		case 0x2000:
			flipscreen = data;
		return;

		case 0x2001:
			palette_bank = (data & 0x80) >> 6;
		return;

		case 0x2002:
			soundlatch = data;
			M6502Close();
			M6502Open(1);
			M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
			M6502Close();
			M6502Open(0);
		return;

		case 0x2003:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;
	}
}

static UINT8 tagteam_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x4000) {
		return DrvVidRAM[mirror_addr(address)];
	}

	if ((address & 0xfc00) == 0x4400) {
		return DrvColRAM[mirror_addr(address)];
	}

	if ((address & 0xf800) == 0x4800) {
		return 0; // nop
	}

	switch (address)
	{
		case 0x2000:
			return DrvInputs[1];

		case 0x2001:
			return DrvInputs[0];

		case 0x2002:
			return (DrvDips[0] & 0x7f) | (vblank ? 0x80 : 0);

		case 0x2003:
			return DrvDips[1];
	}

	return 0;
}

static void tagteam_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
		case 0x2001:
			AY8910Write(0, ~address & 1, data);
		return;

		case 0x2002:
		case 0x2003:
			AY8910Write(1, ~address & 1, data);
		return;

		case 0x2004:
			DACSignedWrite(0, data);
		return;

		case 0x2005:
			sound_nmi_mask = data;
		return;
	}
}

static UINT8 tagteam_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x2007:
			return soundlatch;
	}

	return 0;
}

static tilemap_callback( bg )
{
	offs ^= 0x1f; // flipx
	TILE_SET_INFO(0, DrvVidRAM[offs] + (DrvColRAM[offs] * 256), palette_bank, 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	DACReset();
	M6502Close();

	AY8910Reset(0);

	sound_nmi_mask = 0;
	soundlatch = 0;
	flipscreen = 0;
	palette_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM0		= Next; Next += 0x010000;
	DrvM6502ROM1		= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x030000;
	DrvGfxROM1			= Next; Next += 0x040000;

	DrvColPROM			= Next; Next += 0x000020;

	DrvPalette			= (UINT32*)Next; Next += 0x0020 * sizeof(UINT32);

	AllRam				= Next;

	DrvM6502RAM0		= Next; Next += 0x000800;
	DrvM6502RAM1		= Next; Next += 0x000400;
	DrvVidRAM			= Next; Next += 0x000400;
	DrvColRAM			= Next; Next += 0x000400;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[3] = { 0x60000, 0x30000, 0 };
	INT32 XOffs[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x12000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x12000);

	GfxDecode(0x0c00, 3,  8,  8, Plane, XOffs + 8, YOffs, 0x040, tmp, DrvGfxROM0);
	GfxDecode(0x0300, 3, 16, 16, Plane, XOffs + 0, YOffs, 0x100, tmp, DrvGfxROM1);

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
		if (BurnLoadRom(DrvM6502ROM0 + 0x08000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0a000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0c000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0e000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x04000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x06000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x08000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x0a000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x0c000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x0e000,  9, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x02000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x04000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x06000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x08000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x0a000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x0c000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x0e000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x10000, 18, 1)) return 1;

		if (BurnLoadRom(DrvColPROM   + 0x00000, 19, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,			0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,				0x4800, 0x4bff, MAP_RAM);
	M6502MapMemory(DrvColRAM,				0x4c00, 0x4fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(tagteam_main_write);
	M6502SetReadHandler(tagteam_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,			0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1 + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(tagteam_sound_write);
	M6502SetReadHandler(tagteam_sound_read);
	M6502Close();

	AY8910Init(0, 1500000, 0);
	AY8910Init(1, 1500000, 1);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.30, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, M6502TotalCycles, 1000000);
	DACSetRoute(0, 0.20, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 3, 8, 8, 0x30000, 0, 3);
	GenericTilemapSetOffsets(0, 0, -8);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	AY8910Exit(0);
	AY8910Exit(1);
	DACExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	static const res_net_info tagteam_net_info = {
		RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_TTL_OUT,
		{{ RES_NET_AMP_EMITTER, 4700, 0, 3, { 4700, 3300, 1500 } },
		 { RES_NET_AMP_EMITTER, 4700, 0, 3, { 4700, 3300, 1500 } },
		 { RES_NET_AMP_EMITTER, 4700, 0, 2, {       3300, 1500 } }}
	};

	static const res_net_decode_info tagteam_decode_info = {
		1,
		0x000, 0x01f,
		{  0x00, 0x00, 0x00 },
		{  0x00, 0x03, 0x06 },
		{  0x07, 0x07, 0x03 }
	};

	compute_res_net_all(DrvPalette, DrvColPROM, tagteam_decode_info, tagteam_net_info);

#if 0
	for (INT32 i = 0; i < 0x20; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 1;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 1;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 1;

		INT32 r = (((bit2 * 4700) + (bit1 * 3300) + (bit0 * 1500)) * 255) / 9500;

		bit0 = (DrvColPROM[i] >> 3) & 1;
		bit1 = (DrvColPROM[i] >> 4) & 1;
		bit2 = (DrvColPROM[i] >> 5) & 1;

		INT32 g = (((bit2 * 4700) + (bit1 * 3300) + (bit0 * 1500)) * 255) / 9500;

		bit0 = (DrvColPROM[i] >> 6) & 1;
		bit1 = (DrvColPROM[i] >> 7) & 1;

		INT32 b = (((bit1 * 3300) + (bit0 * 1500)) * 255) / 4800;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
#endif
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x20; offs += 4)
	{
		INT32 attr = DrvVidRAM[offs];
		INT32 spritebank = (attr & 0x30) << 4;
		INT32 code = DrvVidRAM[offs + 0x01] + spritebank;
		INT32 sx = 240 - DrvVidRAM[offs + 3];
		INT32 sy = 240 - DrvVidRAM[offs + 2] - 8;
		INT32 flipx = attr & 0x04;
		INT32 flipy = attr & 0x02;

		if ((attr & 0x01) == 0) continue;

		if (flipscreen)
		{
			sx = 240 - sx;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, palette_bank|1, 3, 0, 0, DrvGfxROM1);

		code = DrvVidRAM[offs + 0x20] + spritebank;
		sy += (flipscreen ? -256 : 256);

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, palette_bank|0, 3, 0, 0, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	M6502NewFrame();

	{
		INT32 previous_coin = DrvInputs[0] & 0xc0;

		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		if (previous_coin != (DrvInputs[0] & 0xc0)) {
			M6502Open(0);
			M6502SetIRQLine(0x20, ((DrvInputs[0] & 0xc0) == 0xc0) ? CPU_IRQSTATUS_NONE : CPU_IRQSTATUS_ACK);
			M6502Close();
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1500000 / 57, 1000000 / 57 };
	INT32 nCyclesDone[2] = { 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 240) vblank = 1;

		M6502Open(0);
		nCyclesDone[0] += M6502Run(nCyclesTotal[0] / nInterleave);
		if ((i%16) == 15) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
		M6502Close();

		M6502Open(1);
		nCyclesDone[1] += M6502Run(nCyclesTotal[1] / nInterleave);
		if (sound_nmi_mask && (i%16) == 15) M6502SetIRQLine(0x20, CPU_IRQSTATUS_AUTO);
		M6502Close();
	}

	if (pBurnSoundOut) {
		AY8910Render(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
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
		AY8910Scan(nAction, pnMin);
		DACScan(nAction, pnMin);

		SCAN_VAR(palette_bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(sound_nmi_mask);
	}

	return 0;
}


// The Big Pro Wrestling!

static struct BurnRomInfo bigprowrRomDesc[] = {
	{ "bf00-1.20",		0x2000, 0x8aba32c9, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "bf01.33",		0x2000, 0x0a41f3ae, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "bf02.34",		0x2000, 0xa28b0a0e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "bf03.46",		0x2000, 0xd4cf7ec7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bf4.8",			0x2000, 0x0558e1d8, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 #1 Code
	{ "bf5.7",			0x2000, 0xc1073f24, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bf6.6",			0x2000, 0x208cd081, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "bf7.3",			0x2000, 0x34a033dc, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "bf8.2",			0x2000, 0xeafe8056, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "bf9.1",			0x2000, 0xd589ce1b, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "bf10.89",		0x2000, 0xb1868746, 3 | BRF_GRA },           // 10 Graphics
	{ "bf11.94",		0x2000, 0xc3fe99c1, 3 | BRF_GRA },           // 11
	{ "bf12.103",		0x2000, 0xc8717a46, 3 | BRF_GRA },           // 12
	{ "bf13.91",		0x2000, 0x23ee34d3, 3 | BRF_GRA },           // 13
	{ "bf14.95",		0x2000, 0xa6721142, 3 | BRF_GRA },           // 14
	{ "bf15.105",		0x2000, 0x60ae1078, 3 | BRF_GRA },           // 15
	{ "bf16.93",		0x2000, 0xd33dc245, 3 | BRF_GRA },           // 16
	{ "bf17.96",		0x2000, 0xccf42380, 3 | BRF_GRA },           // 17
	{ "bf18.107",		0x2000, 0xfd6f006d, 3 | BRF_GRA },           // 18

	{ "fko.8",			0x0020, 0xb6ee1483, 4 | BRF_GRA },           // 19 Color Data

	{ "fjo.25",			0x0020, 0x24da2b63, 0 | BRF_OPT },           // 20 Timing PROM
};

STD_ROM_PICK(bigprowr)
STD_ROM_FN(bigprowr)

struct BurnDriver BurnDrvBigprowr = {
	"bigprowr", NULL, NULL, NULL, "1983",
	"The Big Pro Wrestling!\0", NULL, "Technos Japan", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, bigprowrRomInfo, bigprowrRomName, NULL, NULL, NULL, NULL, BigprowrInputInfo, BigprowrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	240, 256, 3, 4
};


// Tag Team Wrestling

static struct BurnRomInfo tagteamRomDesc[] = {
	{ "prowbf0.bin",	0x2000, 0x6ec3afae, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "prowbf1.bin",	0x2000, 0xb8fdd176, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "prowbf2.bin",	0x2000, 0x3d33a923, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "prowbf3.bin",	0x2000, 0x518475d2, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "bf4.8",			0x2000, 0x0558e1d8, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 #1 Code
	{ "bf5.7",			0x2000, 0xc1073f24, 2 | BRF_PRG | BRF_ESS }, //  5
	{ "bf6.6",			0x2000, 0x208cd081, 2 | BRF_PRG | BRF_ESS }, //  6
	{ "bf7.3",			0x2000, 0x34a033dc, 2 | BRF_PRG | BRF_ESS }, //  7
	{ "bf8.2",			0x2000, 0xeafe8056, 2 | BRF_PRG | BRF_ESS }, //  8
	{ "bf9.1",			0x2000, 0xd589ce1b, 2 | BRF_PRG | BRF_ESS }, //  9

	{ "prowbf10.bin",	0x2000, 0x48165902, 3 | BRF_GRA },           // 10 Graphics
	{ "bf11.94",		0x2000, 0xc3fe99c1, 3 | BRF_GRA },           // 11
	{ "prowbf12.bin",	0x2000, 0x69de1ea2, 3 | BRF_GRA },           // 12
	{ "prowbf13.bin",	0x2000, 0xecfa581d, 3 | BRF_GRA },           // 13
	{ "bf14.95",		0x2000, 0xa6721142, 3 | BRF_GRA },           // 14
	{ "prowbf15.bin",	0x2000, 0xd0de7e03, 3 | BRF_GRA },           // 15
	{ "prowbf16.bin",	0x2000, 0x75ee5705, 3 | BRF_GRA },           // 16
	{ "bf17.96",		0x2000, 0xccf42380, 3 | BRF_GRA },           // 17
	{ "prowbf18.bin",	0x2000, 0xe73a4bba, 3 | BRF_GRA },           // 18

	{ "fko.8",			0x0020, 0xb6ee1483, 4 | BRF_GRA },           // 19 Color Data

	{ "fjo.25",			0x0020, 0x24da2b63, 0 | BRF_OPT },           // 20 Timing PROM
};

STD_ROM_PICK(tagteam)
STD_ROM_FN(tagteam)

struct BurnDriver BurnDrvTagteam = {
	"tagteam", "bigprowr", NULL, NULL, "1983",
	"Tag Team Wrestling\0", NULL, "Technos Japan (Data East license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VSFIGHT, 0,
	NULL, tagteamRomInfo, tagteamRomName, NULL, NULL, NULL, NULL, BigprowrInputInfo, TagteamDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x20,
	240, 256, 3, 4
};
