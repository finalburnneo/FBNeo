// FinalBurn Neo The Deep driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6502_intf.h"
#include "burn_ym2203.h"
#include "burn_pal.h"
#include "mcs51.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM[3];
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVidRAM[2];
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrollRAM;

static UINT8 *scroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 rom_bank;
static UINT8 nmi_enable;
static UINT8 soundlatch;
static UINT8 flipscreen;
static INT32 mcu_p2;
static INT32 mcu_p3;
static INT32 maincpu_to_mcu;
static INT32 mcu_to_maincpu;
static INT32 coin_result;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[5];
static UINT8 DrvReset;

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy3 + 1,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvInputs + 2,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL					},
	{0x13, 0xff, 0xff, 0xff, NULL					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x03, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"				},
	{0x12, 0x01, 0x0c, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x12, 0x01, 0x10, 0x10, "Off"					},
	{0x12, 0x01, 0x10, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Start Stage"			},
	{0x12, 0x01, 0x20, 0x20, "1"					},
	{0x12, 0x01, 0x20, 0x00, "4"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x12, 0x01, 0x40, 0x40, "Off"					},
	{0x12, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x12, 0x01, 0x80, 0x80, "Off"					},
	{0x12, 0x01, 0x80, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x13, 0x01, 0x03, 0x02, "Easy"					},
	{0x13, 0x01, 0x03, 0x03, "Normal"				},
	{0x13, 0x01, 0x03, 0x01, "Harder"				},
	{0x13, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x13, 0x01, 0x0c, 0x0c, "3"					},
	{0x13, 0x01, 0x0c, 0x08, "4"					},
	{0x13, 0x01, 0x0c, 0x04, "5"					},
	{0x13, 0x01, 0x0c, 0x00, "6"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x13, 0x01, 0x30, 0x00, "50k"					},
	{0x13, 0x01, 0x30, 0x30, "50k 70k"				},
	{0x13, 0x01, 0x30, 0x20, "60k 80k"				},
	{0x13, 0x01, 0x30, 0x10, "80k 100k"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x13, 0x01, 0x80, 0x80, "Off"					},
	{0x13, 0x01, 0x80, 0x00, "On"					},
};

STDDIPINFO(Drv)

static void __fastcall thedeep_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			maincpu_to_mcu = data;
			mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_ACK);
		return;

		case 0xe004:
			nmi_enable = data;
			if (!nmi_enable) ZetSetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_NONE);
		return;

		case 0xe00c:
			soundlatch = data;
			M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK);
		return;

		case 0xe100:
		return; // nop

		case 0xe210:
		case 0xe211:
		case 0xe212:
		case 0xe213:
			scroll[address & 3] = data;
		return;
	}
}

static UINT8 __fastcall thedeep_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return mcu_to_maincpu;

		case 0xe004:
		{
			UINT8 ret = coin_result;
			coin_result = 0;
			return ret;
		}

		case 0xe008:
		case 0xe009:
		case 0xe00a:
		case 0xe00b:
			return DrvInputs[address & 3];
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	rom_bank = data & 3;

	ZetMapMemory(DrvZ80ROM + 0x10000 + (rom_bank * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void mcu_write_port(INT32 port, UINT8 data)
{
	switch (port)
	{
		case MCS51_PORT_P1:
			flipscreen = ~data & 1;
			bankswitch((data >> 1) & 3);
		return;

		case MCS51_PORT_P2:
			mcu_p2 = data;
		return;

		case MCS51_PORT_P3:
			if ((mcu_p3 & 0x01) != 0 && (data & 0x01) == 0) ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			if ((mcu_p3 & 0x02) != 0 && (data & 0x02) == 0) mcs51_set_irq_line(MCS51_INT0_LINE, CPU_IRQSTATUS_NONE);
			if ((mcu_p3 & 0x10) != 0 && (data & 0x10) == 0) coin_result = 1;
			if ((mcu_p3 & 0x40) != 0 && (data & 0x40) == 0) mcu_to_maincpu = mcu_p2;
			if ((mcu_p3 & 0x80) != 0 && (data & 0x80) == 0) mcs51_set_forced_input(2, maincpu_to_mcu);
			mcu_p3 = data;
		return;
	}
}

static UINT8 mcu_read_port(INT32 port)
{
	switch (port)
	{
		case MCS51_PORT_P0:
			return (DrvInputs[4] & 0xe) | (((DrvInputs[4] & 0xe) == 0x0e) ? 1 : 0);

		case MCS51_PORT_P2:
			return maincpu_to_mcu;
	}
	return 0;
}

static void thedeep_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0800:
		case 0x0801:
			BurnYM2203Write(0, address & 1, data);
		return;
	}
}

static UINT8 thedeep_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static tilemap_scan( bg )
{
	return (col & 0x0f) + ((col & 0x10) << 5) + (row << 4);;
}

static tilemap_callback( bg )
{
	INT32 attr = DrvVidRAM[0][offs * 2 + 1];
	INT32 code = DrvVidRAM[0][offs * 2 + 0] + ((attr & 0x0f) << 8);

	TILE_SET_INFO(1, code, attr >> 4, TILE_FLIPX);
}

static tilemap_callback( fg )
{
	INT32 attr = DrvVidRAM[1][offs * 2 + 1];
	INT32 code = DrvVidRAM[1][offs * 2 + 0] + ((attr & 0x0f) << 8);

	TILE_SET_INFO(2, code, attr >> 4, 0);
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	M6502SetIRQLine(0, nStatus ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	mcs51_reset(); // z80 banksw. via mcs51 port handler on reset..
	ZetReset();
	ZetClose();

	M6502Open(0);
	M6502Reset();
	BurnYM2203Reset();
	M6502Close();

	nmi_enable = 0;
	soundlatch = 0;
	flipscreen = 0;

	mcu_p2 = 0;
	mcu_p3 = 0;
	maincpu_to_mcu = 0;
	mcu_to_maincpu = 0;
	coin_result = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM			= Next; Next += 0x020000;
	DrvM6502ROM			= Next; Next += 0x008000;
	DrvMCUROM			= Next; Next += 0x001000;

	DrvGfxROM[0]		= Next; Next += 0x080000;
	DrvGfxROM[1]		= Next; Next += 0x080000;
	DrvGfxROM[2]		= Next; Next += 0x010000;

	DrvColPROM			= Next; Next += 0x000400;

	DrvPalette			= (UINT32*)Next; Next += 0x000201 * sizeof(UINT32);

	AllRam				= Next;

	DrvZ80RAM			= Next; Next += 0x002000;
	DrvM6502RAM			= Next; Next += 0x000800;
	DrvVidRAM[0]		= Next; Next += 0x000800;
	DrvVidRAM[1]		= Next; Next += 0x000800;
	DrvSprRAM			= Next; Next += 0x000400;
	DrvScrollRAM		= Next; Next += 0x000800;

	scroll				= Next; Next += 0x000004;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0, 4 };
	INT32 Plane1[4]  = { STEP4(0,0x80000) };
	INT32 XOffs0[8]  = { STEP4((0x4000*8)/2,1), STEP4(0,1) };
	INT32 XOffs1[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs[16]  = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM[0], 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp, DrvGfxROM[0]);

	memcpy (tmp, DrvGfxROM[1], 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs, 0x100, tmp, DrvGfxROM[1]);

	memcpy (tmp, DrvGfxROM[2], 0x04000);

	GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs, 0x040, tmp, DrvGfxROM[2]);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvZ80ROM + 0x00000,    k++, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x10000,    k++, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM,            k++, 1)) return 1;

		if (BurnLoadRom(DrvMCUROM,              k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[0] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[0] + 0x30000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[1] + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x20000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM[1] + 0x30000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM[2] + 0x00000, k++, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000,   k++, 1)) return 1; // not used
		if (BurnLoadRom(DrvColPROM + 0x00000,   k++, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200,   k++, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe400, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[1],	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvVidRAM[0],	0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvScrollRAM,	0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(thedeep_main_write);
	ZetSetReadHandler(thedeep_main_read);
	ZetClose();

	mcs51_init();
	mcs51_set_program_data(DrvMCUROM);
	mcs51_set_write_handler(mcu_write_port);
	mcs51_set_read_handler(mcu_read_port);

	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,	0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(thedeep_sound_write);
	M6502SetReadHandler(thedeep_sound_read);
	M6502Close();

	BurnYM2203Init(1,  3000000, &DrvYM2203IRQHandler, 0);
	BurnTimerAttachM6502(1500000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	GenericTilemapInit(0, bg_map_scan,       bg_map_callback, 16, 16, 32, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, fg_map_callback,  8,  8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM[0], 4, 16, 16, 0x80000, 0x080, 0x7);
	GenericTilemapSetGfx(1, DrvGfxROM[1], 4, 16, 16, 0x80000, 0x100, 0xf);
	GenericTilemapSetGfx(2, DrvGfxROM[2], 2,  8,  8, 0x10000, 0x000, 0xf);
	GenericTilemapSetScrollCols(0, 32);
	GenericTilemapSetTransparent(0, 0);
	GenericTilemapSetTransparent(1, 0);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	mcs51_exit();
	M6502Exit();

	BurnYM2203Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 512; i++) {
		DrvPalette[i] = BurnHighCol(pal4bit(DrvColPROM[0x200 + i]), pal4bit(DrvColPROM[0x200 + i] >> 4), pal4bit(DrvColPROM[i]), 0);
	}

	DrvPalette[0x200] = BurnHighCol(0, 0, 0, 0); // black
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x400/2;)
	{
		INT32 sy = spriteram[offs];
		INT32 sx = spriteram[offs + 2];

		if (((sx & 0x0800) && (nCurrentFrame & 1) == 0) || (sy & 0x8000) == 0) {
			offs += 4;
			continue; // flash or disabled
		}
		INT32 color = sx >> 12;
		INT32 flipx = (sy & 0x2000) >> 13;
		INT32 flipy = (sy & 0x4000) >> 14;
		INT32 h = (1 << ((sy & 0x1800) >> 11));
		INT32 w = (1 << ((sy & 0x0600) >>  9));

		sx &= 0x01ff;
		sy &= 0x01ff;
		if (sx >= 256) sx -= 512;
		if (sy >= 256) sy -= 512;
		sx = 240 - sx;
		sy = 240 - sy;

		INT32 mult = -16;
		if (flipscreen)
		{
			sy = 240 - sy;
			sx = 240 - sx;
			flipx ^= 1;
			flipy ^= 1;
			mult = 16;
		}
		INT32 incy = ((flipy) ? -1 : 1);

		for (INT32 x = 0; x < w; x++)
		{
			INT32 code = (spriteram[offs + 1] & 0x1fff) & ~(h - 1);
			code += ((flipy) ? 0 : (h - 1));

			for (INT32 y = 0; y < h; y++)
			{
				DrawGfxMaskTile(0, 0, (code - y * incy) & 0x7ff, sx + (mult * x), sy + (mult * y), flipx, flipy, color, 0);
			}

			offs += 4;
			if (offs >= 0x400/2)
				continue;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapSetScrollX(0, scroll[0] + (scroll[1] << 8));

	for (INT32 col = 0; col < 32; col++)
	{
		INT32 yscroll = scroll[2] + DrvScrollRAM[col*2+0] + ((DrvScrollRAM[col*2+1] + scroll[3]) * 256);

		GenericTilemapSetScrollCol(0, col, yscroll);
	}

	BurnTransferClear(0x200);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) draw_sprites();

	if (nBurnLayer & 4) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	mcs51NewFrame();
	M6502NewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[4] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256 * 4; // must keep "main <-> mcu" comms. happy
	INT32 nCyclesTotal[3] = { 6000000 / 60, 8000000 / 12 / 60, 1500000 / 60};
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	ZetOpen(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Zet);
		if (i == 0 && nmi_enable) ZetSetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_ACK);

		CPU_RUN(1, mcs51);
		if (i == 127 * 4) mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_ACK);
		if (i == 128 * 4) mcs51_set_irq_line(MCS51_INT1_LINE, CPU_IRQSTATUS_NONE);

		BurnTimerUpdate((i + 1) * (nCyclesTotal[2] / nInterleave));
	}

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	ZetClose();

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
		mcs51_scan(nAction);
		M6502Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(rom_bank);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(mcu_p2);
		SCAN_VAR(mcu_p3);
		SCAN_VAR(maincpu_to_mcu);
		SCAN_VAR(mcu_to_maincpu);
		SCAN_VAR(coin_result);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(rom_bank);
		ZetClose();
	}

	return 0;
}


// The Deep (Japan)

static struct BurnRomInfo thedeepRomDesc[] = {
	{ "dp-10.rom",	0x08000, 0x7480b7a5, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "dp-09.rom",	0x10000, 0xc630aece, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dp-12.rom",	0x08000, 0xc4e848c4, 2 | BRF_PRG | BRF_ESS }, //  2 M65c02 Code

	{ "dp-14",		0x01000, 0x0b886dad, 3 | BRF_PRG | BRF_ESS }, //  3 i8751 Code

	{ "dp-08.rom",	0x10000, 0xc5624f6b, 4 | BRF_GRA },           //  4 Sprites
	{ "dp-07.rom",	0x10000, 0xc76768c1, 4 | BRF_GRA },           //  5
	{ "dp-06.rom",	0x10000, 0x98adea78, 4 | BRF_GRA },           //  6
	{ "dp-05.rom",	0x10000, 0x76ea7dd1, 4 | BRF_GRA },           //  7

	{ "dp-03.rom",	0x10000, 0x6bf5d819, 5 | BRF_GRA },           //  8 Background Tiles
	{ "dp-01.rom",	0x10000, 0xe56be2fe, 5 | BRF_GRA },           //  9
	{ "dp-04.rom",	0x10000, 0x4db02c3c, 5 | BRF_GRA },           // 10
	{ "dp-02.rom",	0x10000, 0x1add423b, 5 | BRF_GRA },           // 11

	{ "dp-11.rom",	0x04000, 0x196e23d1, 6 | BRF_GRA },           // 12 Foreground Tiles

	{ "fi-1",		0x00200, 0xf31efe09, 7 | BRF_OPT },           // 13 Color Data
	{ "fi-2",		0x00200, 0xf305c8d5, 7 | BRF_GRA },           // 14
	{ "fi-3",		0x00200, 0xf61a9686, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(thedeep)
STD_ROM_FN(thedeep)

struct BurnDriver BurnDrvThedeep = {
	"thedeep", NULL, NULL, NULL, "1987",
	"The Deep (Japan)\0", NULL, "Woodplace Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, thedeepRomInfo, thedeepRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x201,
	248, 256, 3, 4
};


// Run Deep

static struct BurnRomInfo rundeepRomDesc[] = {
	{ "3",			0x08000, 0xc9c9e194, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "9",			0x10000, 0x931f4e67, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dp-12.rom",	0x08000, 0xc4e848c4, 2 | BRF_PRG | BRF_ESS }, //  2 M65c02 Code

	{ "dp-14",		0x01000, 0x0b886dad, 3 | BRF_PRG | BRF_ESS }, //  3 i8751 Code

	{ "dp-08.rom",	0x10000, 0xc5624f6b, 4 | BRF_GRA },           //  4 sprites
	{ "dp-07.rom",	0x10000, 0xc76768c1, 4 | BRF_GRA },           //  5
	{ "dp-06.rom",	0x10000, 0x98adea78, 4 | BRF_GRA },           //  6
	{ "dp-05.rom",	0x10000, 0x76ea7dd1, 4 | BRF_GRA },           //  7

	{ "dp-03.rom",	0x10000, 0x6bf5d819, 5 | BRF_GRA },           //  8 Background Tiles
	{ "dp-01.rom",	0x10000, 0xe56be2fe, 5 | BRF_GRA },           //  9
	{ "dp-04.rom",	0x10000, 0x4db02c3c, 5 | BRF_GRA },           // 10
	{ "dp-02.rom",	0x10000, 0x1add423b, 5 | BRF_GRA },           // 11

	{ "11",			0x04000, 0x5d29e4b9, 6 | BRF_GRA },           // 12 Foreground Tiles

	{ "fi-1",		0x00200, 0xf31efe09, 7 | BRF_OPT },           // 13 Color Data
	{ "fi-2",		0x00200, 0xf305c8d5, 7 | BRF_GRA },           // 14
	{ "fi-3",		0x00200, 0xf61a9686, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(rundeep)
STD_ROM_FN(rundeep)

struct BurnDriver BurnDrvRundeep = {
	"rundeep", "thedeep", NULL, NULL, "1988",
	"Run Deep\0", NULL, "Cream Co., Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, rundeepRomInfo, rundeepRomName, NULL, NULL, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x201,
	248, 256, 3, 4
};
