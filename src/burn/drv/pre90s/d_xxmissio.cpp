// FB Alpha XX Mission driver module
// Based on MAME driver by Uki

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvShareRAM0;
static UINT8 *DrvShareRAM1;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[2];

static UINT8 scrollx;
static UINT8 scrollx_shifted;
static UINT8 scrolly;
static UINT8 cpu_bank;
static UINT8 cpu_status;
static UINT8 flipscreen;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT8 DrvInputs[2];

static struct BurnInputInfo XxmissioInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 7,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Xxmissio)

static struct BurnDIPInfo XxmissioDIPList[]=
{
	DIP_OFFSET(0x10)
	{0x00, 0xff, 0xff, 0xd7, NULL			},
	{0x01, 0xff, 0xff, 0xef, NULL			},

	{0   , 0xfe, 0   ,    4, "Coinage"		},
	{0x00, 0x01, 0x03, 0x00, "3 Coins 1 Credits"	},
	{0x00, 0x01, 0x03, 0x02, "2 Coins 1 Credits"	},
	{0x00, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x00, 0x01, 0x03, 0x01, "1 Coin  2 Credits"	},

//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x00, 0x01, 0x04, 0x04, "Off"			},
//	{0x00, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x00, 0x01, 0x08, 0x08, "Off"			},
	{0x00, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x00, 0x01, 0x10, 0x10, "Normal"		},
	{0x00, 0x01, 0x10, 0x00, "Hard"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x00, 0x01, 0x20, 0x00, "Upright"		},
	{0x00, 0x01, 0x20, 0x20, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Endless Game (Cheat)"	},
	{0x00, 0x01, 0x40, 0x40, "Off"			},
	{0x00, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x00, 0x01, 0x80, 0x80, "Off"			},
	{0x00, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x01, 0x01, 0x03, 0x01, "2"			},
	{0x01, 0x01, 0x03, 0x03, "3"			},
	{0x01, 0x01, 0x03, 0x02, "4"			},
	{0x01, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "First Bonus"		},
	{0x01, 0x01, 0x04, 0x04, "30000"		},
	{0x01, 0x01, 0x04, 0x00, "40000"		},

	{0   , 0xfe, 0   ,    4, "Bonus Every"		},
	{0x01, 0x01, 0x18, 0x18, "50000"		},
	{0x01, 0x01, 0x18, 0x08, "70000"		},
	{0x01, 0x01, 0x18, 0x10, "90000"		},
	{0x01, 0x01, 0x18, 0x00, "None"			},
};

STDDIPINFO(Xxmissio)

static void palette_update(INT32 entry)
{
	UINT8 i = (DrvPalRAM[entry] >> 0) & 0x03;
	UINT8 r =((DrvPalRAM[entry] >> 0) & 0x0c) | i;
	UINT8 g =((DrvPalRAM[entry] >> 2) & 0x0c) | i;
	UINT8 b =((DrvPalRAM[entry] >> 4) & 0x0c) | i;

	r |= r << 4;
	g |= g << 4;
	b |= b << 4;

	DrvPalette[entry] = BurnHighCol(r,g,b,0);
}

static void sync_sub()
{
	ZetCPUPush(1);
	BurnTimerUpdate(ZetTotalCycles(0));
	ZetCPUPop();
}

static void sync_main()
{
	INT32 cyc = ZetTotalCycles(1) - ZetTotalCycles(0);
	if (cyc > 0) {
		ZetRun(0, cyc);
	}
}

static void __fastcall xxmission_main_write(UINT16 address, UINT8 data)
{
	if (address >= 0xd800 && address <= 0xdaff) {
		DrvPalRAM[address & 0x3ff] = data;
		palette_update(address & 0x3ff);
		return;
	}

	if ((address & 0xf800) == 0xc800) {
		DrvBgRAM[(address & 0x7e0) | (((address & 0x1f) + scrollx_shifted) & 0x1f)] = data;
		return;
	}

	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			BurnYM2203Write((address/2)&1, address & 1, data);
		return;

		case 0xa002:
		{
			sync_sub();
			switch (data)
			{
				case 0x00: cpu_status |= 0x20; break;
				case 0x80: cpu_status |= 0x04; break;

				case 0x40: {
					cpu_status &= ~0x08;
					ZetSetVector(1, 0x10);
					ZetSetIRQLine(1, 0, CPU_IRQSTATUS_HOLD);
				}
				break;
			}
		}
		return;

		case 0xa003:
			flipscreen = data & 0x01;
		return;
	}
}

static void bankswitch(INT32 data)
{
	cpu_bank = (data & 0x07);

	ZetMapMemory(DrvZ80ROM1 + 0x10000 + (cpu_bank * 0x4000), 0x4000, 0x7fff, MAP_ROM);
}

static void __fastcall xxmission_sub_write(UINT16 address, UINT8 data)
{
	if (address >= 0xd800 && address <= 0xdaff) {
		DrvPalRAM[address & 0x3ff] = data;
		palette_update(address & 0x3ff);
		return;
	}

	if ((address & 0xf800) == 0xc800) {
		DrvBgRAM[(address & 0x7e0) | (((address & 0x1f) + scrollx_shifted) & 0x1f)] = data;
		return;
	}

	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			BurnYM2203Write((address/2)&1, address & 1, data);
		return;

		case 0x8006:
			bankswitch(data);
		return;

		case 0xa002:
		{
			sync_main();
			switch (data)
			{
				case 0x00: cpu_status |= 0x10; break;
				case 0x40: cpu_status |= 0x08; break;

				case 0x80: {
					cpu_status &= ~0x04;
					ZetSetVector(0, 0x10);
					ZetSetIRQLine(0, 0, CPU_IRQSTATUS_HOLD);
				}
				break;
			}
		}
		return;

		case 0xa003:
			flipscreen = data & 0x01;
		return;
	}
}

static UINT8 __fastcall xxmission_read(UINT16 address)
{
	if ((address & 0xf800) == 0xc800) {
		return DrvBgRAM[(address & 0x7e0) | (((address & 0x1f) + scrollx_shifted) & 0x1f)];
	}

	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8002:
		case 0x8003:
			return BurnYM2203Read((address/2)&1, address & 1);

		case 0xa000:
		case 0xa001:
			return DrvInputs[address & 1];

		case 0xa002:
			if (ZetGetActive() == 0) sync_sub(); else sync_main();
			return (cpu_status & 0xfd) | ((vblank) ? 0x00 : 0x02); // status!
	}

	return 0;
}

static UINT8 DrvYM2203ReadPortA(UINT32)
{
	return DrvDips[0];
}

static UINT8 DrvYM2203ReadPortB(UINT32)
{
	return DrvDips[1];
}

static void DrvYM2203WritePortA(UINT32, UINT32 data)
{
	scrollx = data;
	scrollx_shifted = scrollx >> 3;
}

static void DrvYM2203WritePortB(UINT32, UINT32 data)
{
	scrolly = data;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	bankswitch(0);
	BurnYM2203Reset();
	ZetClose();

	scrollx = 0;
	scrollx_shifted = 0;
	scrolly = 0;
	cpu_status = 0;
	flipscreen = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x008000;
	DrvZ80ROM1		= Next; Next += 0x028000;

	DrvGfxROM0		= Next; Next += 0x008000;
	DrvGfxROM1		= Next; Next += 0x040000;
	DrvGfxROM2		= Next; Next += 0x020000;

	DrvPalette		= (UINT32*)Next; Next += 0x0300 * sizeof(UINT32);

	AllRam			= Next;

	DrvBgRAM		= Next; Next += 0x000800;
	DrvFgRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000300;
	DrvShareRAM0	= Next; Next += 0x001000;
	DrvShareRAM1	= Next; Next += 0x001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	INT32 Planes[4] = { STEP4(0,1) };
	INT32 XOffs[32] = { STEP16(0,4), STEP16((64*8),4) };
	INT32 YOffs[16] = { STEP8(0,64), STEP8((64*16), 64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM1, 0x20000);

	GfxDecode(0x0100, 4, 16,  8, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM0);
	GfxDecode(0x0200, 4, 32, 16, Planes, XOffs, YOffs, 0x800, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x10000);

	GfxDecode(0x0400, 4, 16,  8, Planes, XOffs, YOffs, 0x200, tmp, DrvGfxROM2);

	BurnFree (tmp);
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x10000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x18000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x20000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00001,  5, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  6, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10001,  7, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x00000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x00001, 10, 2)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,		0xc000, 0xc7ff, MAP_RAM);
//	ZetMapMemory(DrvBgRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xd800, 0xdaff, MAP_ROM);
	ZetMapMemory(DrvShareRAM0,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM1,	0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(xxmission_main_write);
	ZetSetReadHandler(xxmission_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvFgRAM,		0xc000, 0xc7ff, MAP_RAM);
//	ZetMapMemory(DrvBgRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,		0xd800, 0xdaff, MAP_ROM);
	ZetMapMemory(DrvShareRAM1,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM0,	0xf000, 0xffff, MAP_RAM);
	ZetSetWriteHandler(xxmission_sub_write);
	ZetSetReadHandler(xxmission_read);
	ZetClose();

	BurnYM2203Init(2,  1500000, NULL, 0);
	BurnYM2203SetPorts(0, &DrvYM2203ReadPortA, &DrvYM2203ReadPortB, NULL, NULL);
	BurnYM2203SetPorts(1, NULL, NULL, &DrvYM2203WritePortA, &DrvYM2203WritePortB);
	BurnTimerAttachZet(3000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_YM2203_ROUTE,   0.40, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_1, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_2, 0.15, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(1, BURN_SND_YM2203_AY8910_ROUTE_3, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM2203Exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_bg_layer()
{
	INT32 xscroll = scrollx * 2;
	INT32 yscroll = (scrolly + 32) & 0xff;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 16;
		INT32 sy = (offs / 0x20) * 8;

		sx -= xscroll;
		if (sx < -15) sx += 512;
		sy -= yscroll;
		if (sy <  -7) sy += 256;

		INT32 code  = DrvBgRAM[offs] + ((DrvBgRAM[0x400 + offs] & 0xc0) << 2);
		INT32 color = DrvBgRAM[0x400 + offs] & 0x0f;

		RenderCustomTile_Clip(pTransDraw, 16, 8, code, sx, sy, color, 4, 0x200, DrvGfxROM2);
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = (2*32); offs < (32 * 32)-(2*32); offs++)
	{
		INT32 sx = (offs & 0x1f) * 16;
		INT32 sy = ((offs / 0x20) * 8) - 32;

		INT32 code  = DrvFgRAM[offs];
		INT32 color = DrvFgRAM[0x400 + offs] & 0x07;

		RenderCustomTile_Mask_Clip(pTransDraw, 16, 8, code, sx, sy, color, 4, 0, 0x100, DrvGfxROM0);
	}
}

static inline void draw_single_sprite(INT32 code, INT32 color, INT32 sx, INT32 sy, INT32 flipx, INT32 flipy)
{
	DrawCustomMaskTile(pTransDraw, 32, 16, code, sx, sy, flipx, flipy, color, 4, 0, 0, DrvGfxROM1);
}

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x800; offs += 0x20)
	{
		INT32 attr  =  DrvSprRAM[offs + 3];
		INT32 sy    =  DrvSprRAM[offs + 2] - 32;
		INT32 sx    =((DrvSprRAM[offs + 1] << 1) - 8) & 0x1ff;
		INT32 code  =  DrvSprRAM[offs + 0] | ((attr & 0x40) << 2);
		INT32 color = attr & 0x07;
		INT32 flipx = attr & 0x10;
		INT32 flipy = attr & 0x20;

		if (sy < -15 || sy > 192) continue;

		draw_single_sprite(code, color, sx, sy, flipx, flipy);

		if (sx > 0x1e0) // wrap
			draw_single_sprite(code, color, sx - 0x200, sy, flipx, flipy);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x300; i++) {
			palette_update(i);
		}
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();
	if ( nBurnLayer & 1) draw_bg_layer();
	if (nSpriteEnable & 1) draw_sprites();
	if ( nBurnLayer & 2) draw_fg_layer();

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
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	ZetIdle(0, nExtraCycles[0]); // syncint
	ZetIdle(1, nExtraCycles[1]); // timer

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		if (i == 0) {
			vblank = 1;
			cpu_status &= ~0x20;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		if (i == 14) { vblank = 0; }
		CPU_RUN_SYNCINT(0, Zet);
		ZetClose();

		ZetOpen(1);
		if (i == 0 || i == 49) { // 120hz
			cpu_status &= ~0x10;
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
		CPU_RUN_TIMER(1);
		ZetClose();

		if (i == 48 && pBurnDraw) {
			DrvDraw();
		}
	}

	nExtraCycles[0] = ZetTotalCycles(0) - nCyclesTotal[0];
	nExtraCycles[1] = ZetTotalCycles(1) - nCyclesTotal[1];

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029709;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		ZetScan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(flipscreen);
		SCAN_VAR(cpu_status);
		SCAN_VAR(cpu_bank);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrollx_shifted);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(1);
		bankswitch(cpu_bank);
		ZetClose();
	}

	return 0;
}


// XX Mission

static struct BurnRomInfo xxmissioRomDesc[] = {
	{ "xx1.4l",	0x8000, 0x86e07709, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code

	{ "xx2.4b",	0x4000, 0x13fa7049, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 #1 Code
	{ "xx3.6a",	0x8000, 0x16fdacab, 2 | BRF_PRG | BRF_ESS }, //  2
	{ "xx4.6b",	0x8000, 0x274bd4d2, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "xx5.6d",	0x8000, 0xc5f35535, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "xx6.8j",	0x8000, 0xdc954d01, 3 | BRF_GRA },           //  5 Characters & Sprites
	{ "xx8.8f",	0x8000, 0xa9587cc6, 3 | BRF_GRA },           //  6
	{ "xx7.8h",	0x8000, 0xabe9cd68, 3 | BRF_GRA },           //  7
	{ "xx9.8e",	0x8000, 0x854e0e5f, 3 | BRF_GRA },           //  8

	{ "xx10.4c",	0x8000, 0xd27d7834, 4 | BRF_GRA },           //  9 Background Tiles
	{ "xx11.4b",	0x8000, 0xd9dd827c, 4 | BRF_GRA },           // 10
};

STD_ROM_PICK(xxmissio)
STD_ROM_FN(xxmissio)

struct BurnDriver BurnDrvXxmissio = {
	"xxmissio", NULL, NULL, NULL, "1986",
	"XX Mission\0", NULL, "UPL", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, xxmissioRomInfo, xxmissioRomName, NULL, NULL, NULL, NULL, XxmissioInputInfo, XxmissioDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	192, 512, 3, 4
};
