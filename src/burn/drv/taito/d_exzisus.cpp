// FB Alpha Exzisus driver module
// Based on MAME driver by Yochizo

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "taito_ic.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvZ80ROM3;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvZ80RAM3;
static UINT8 *DrvSharedRAMAB;
static UINT8 *DrvSharedRAMAC;
static UINT8 *DrvObjRAM0;
static UINT8 *DrvObjRAM1;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 *flipscreen;
static UINT8 *nBank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo ExzisusInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 0,	"service"	},
	{"Tilt",		BIT_DIGITAL,	DrvJoy3 + 1,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Exzisus)

static struct BurnDIPInfo ExzisusDIPList[]=
{
	{0x13, 0xff, 0xff, 0xfe, NULL			},
	{0x14, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x01, 0x00, "Upright"		},
	{0x13, 0x01, 0x01, 0x01, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"			},
	{0x13, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x08, 0x00, "Off"			},
	{0x13, 0x01, 0x08, 0x08, "On"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"			},
	{0x14, 0x01, 0x03, 0x03, "Medium"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"			},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x14, 0x01, 0x0c, 0x08, "100k and every 150k"	},
	{0x14, 0x01, 0x0c, 0x0c, "150k and every 200k"	},
	{0x14, 0x01, 0x0c, 0x04, "150k"			},
	{0x14, 0x01, 0x0c, 0x00, "200k"			},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x30, 0x00, "2"			},
	{0x14, 0x01, 0x30, 0x30, "3"			},
	{0x14, 0x01, 0x30, 0x20, "4"			},
	{0x14, 0x01, 0x30, 0x10, "5"			},

	{0   , 0xfe, 0   ,    2, "Service Mode (buggy)"	},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Exzisus)

static void bankswitch(INT32 cpu, INT32 data)
{
	if ((data & 0x0f) >= 2)
	{
		nBank[cpu] = (data & 0x0f);

		ZetMapMemory((cpu ? DrvZ80ROM1 : DrvZ80ROM0) + ((nBank[cpu] - 2) * 0x4000) + 0x8000, 0x8000, 0xbfff, MAP_ROM);
	}

	*flipscreen = data & 0x40;
}

static void __fastcall exzisus_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf400:
			bankswitch(0, data);
		return;

		case 0xf404:
			ZetClose();
			ZetOpen(2);
			ZetReset();
			ZetClose();
			ZetOpen(0);
		return;
	}
}

static void __fastcall exzisus_cpub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
			ZetClose();
			TC0140SYTPortWrite(data);
			ZetOpen(0);
		return;

		case 0xf001:
			ZetClose();
			TC0140SYTCommWrite(data);
			ZetOpen(0);
		return;

		case 0xf400:
			bankswitch(1, data);
		return;

		case 0xf402:
			// coin counter
		return;

		case 0xf404:
			// nop
		return;
	}
}

static UINT8 __fastcall exzisus_cpub_read(UINT16 address)
{
	switch (address)
	{
		case 0xf000:
			return 0; // nop

		case 0xf001:
			return TC0140SYTCommRead();

		case 0xf400:
			return DrvInputs[0];

		case 0xf401:
			return DrvInputs[1];

		case 0xf402:
			return DrvInputs[2] ^ 0x30; // coins high

		case 0xf404:
		case 0xf405:
			return DrvDips[address & 1];
	}

	return 0;
}

static void __fastcall exzisus_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x9001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xa000:
			TC0140SYTSlavePortWrite(data);
		return;

		case 0xa001:
			TC0140SYTSlaveCommWrite(data);
		return;
	}
}

static UINT8 __fastcall exzisus_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9000:
		case 0x9001:
			return BurnYM2151ReadStatus();

		case 0xa001:
			return TC0140SYTSlaveCommRead();
	}

	return 0;
}

static void exzisusYM2151IrqHandler(INT32 state)
{
	ZetSetIRQLine(0, (state) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
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
	ZetClose();

	ZetOpen(3);
	ZetReset();
	BurnYM2151Reset();
	TC0140SYTReset();
	ZetClose();

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x030000;
	DrvZ80ROM2		= Next; Next += 0x008000;
	DrvZ80ROM3		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x100000;

	DrvColPROM		= Next; Next += 0x000c00;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM1		= Next; Next += 0x001000;
	DrvZ80RAM2		= Next; Next += 0x001000;
	DrvZ80RAM3		= Next; Next += 0x001000;

	DrvSharedRAMAB		= Next; Next += 0x000800;
	DrvSharedRAMAC		= Next; Next += 0x001000;

	DrvObjRAM0		= Next; Next += 0x000600;
	DrvObjRAM1		= Next; Next += 0x000600;
	DrvVidRAM0		= Next; Next += 0x001a00;
	DrvVidRAM1		= Next; Next += 0x001a00;

	flipscreen		= Next; Next += 0x000001;
	nBank			= Next; Next += 0x000002;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(UINT8 *rom)
{
	INT32 Plane[4]  = { 0x40000*8, 0x40000*8+4, 0, 4 };
	INT32 XOffs[8] = { STEP4(3, -1), STEP4(8+3,-1) };
	INT32 YOffs[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x80000; i++) {
		tmp[i] = rom[i] ^ 0xff;
	}

	GfxDecode((0x80000 * 2) / 0x40, 4, 8, 8, Plane, XOffs, YOffs, 0x80, tmp, rom);

	BurnFree (tmp);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x400; i++)
	{
		int bit0,bit1,bit2,bit3,r,g,b;

		bit0 = (DrvColPROM[i + 0x000] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x000] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x000] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x000] >> 3) & 0x01;
		r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x400] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x400] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x400] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x400] >> 3) & 0x01;
		g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + 0x800] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + 0x800] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + 0x800] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + 0x800] >> 3) & 0x01;
		b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 DrvInit(INT32 bigcolprom)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x10000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x20000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,  5, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM3 + 0x00000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x50000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x60000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x40000, 15, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x50000, 16, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x00000, 17, 1)) return 1;

		if (bigcolprom) {
			if (BurnLoadRom(DrvColPROM + 0x00400, 18, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00800, 19, 1)) return 1;
		} else {
			if (BurnLoadRom(DrvColPROM + 0x00100, 18, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00400, 19, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00500, 20, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00800, 21, 1)) return 1;
			if (BurnLoadRom(DrvColPROM + 0x00900, 22, 1)) return 1;
		}

		DrvGfxDecode(DrvGfxROM0);
		DrvGfxDecode(DrvGfxROM1);
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSharedRAMAC,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvObjRAM1,	0xc000, 0xc5ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0xc600, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSharedRAMAB,	0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(exzisus_main_write);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvObjRAM0,	0xc000, 0xc5ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0xc600, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,	0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvSharedRAMAB,	0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(exzisus_cpub_write);
	ZetSetReadHandler(exzisus_cpub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvObjRAM1,	0x8000, 0x85ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0x8600, 0x9fff, MAP_RAM);
	ZetMapMemory(DrvSharedRAMAC,	0xa000, 0xafff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM2,	0xb000, 0xbfff, MAP_RAM);
	ZetClose();

	ZetInit(3);
	ZetOpen(3);
	ZetMapMemory(DrvZ80ROM3,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM3,	0x8000, 0x8fff, MAP_RAM);
	ZetSetWriteHandler(exzisus_sound_write);
	ZetSetReadHandler(exzisus_sound_read);
	ZetClose();

	BurnYM2151Init(4000000);
	BurnYM2151SetIrqHandler(&exzisusYM2151IrqHandler);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_RIGHT);

	TC0140SYTInit(3);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	TC0140SYTReset();
	BurnYM2151Exit();

	BurnFree(AllMem);

	return 0;
}

static void TC0010VCU(UINT8 *ObjRAM, UINT8 *VidRAM, UINT8 *gfx, INT32 color_offset)
{
	int offs;
	int sx, sy, xc, yc;
	int gfx_num, gfx_attr, gfx_offs;

	sx = 0;
	for (offs = 0 ; offs < 0x600; offs += 4)
	{
		int height;

		if ( !(*(UINT32 *)(&ObjRAM[offs])) )
		{
			continue;
		}

		gfx_num = ObjRAM[offs + 1];
		gfx_attr = ObjRAM[offs + 3];

		if ((gfx_num & 0x80) == 0)
		{
			gfx_offs = ((gfx_num & 0x7f) << 3);
			height = 2;

			sx = ObjRAM[offs + 2];
			sx |= (gfx_attr & 0x40) << 2;
		}
		else
		{
			gfx_offs = ((gfx_num & 0x3f) << 7) + 0x0400;
			height = 32;

			if (gfx_num & 0x40)
			{
				sx += 16;
			}
			else
			{
				sx = ObjRAM[offs + 2];
				sx |= (gfx_attr & 0x40) << 2;
			}
		}

		sy = 256 - (height << 3) - (ObjRAM[offs]);

		for (xc = 0 ; xc < 2 ; xc++)
		{
			int goffs = gfx_offs;
			for (yc = 0 ; yc < height ; yc++)
			{
				int code, color, x, y;

				code  = (VidRAM[goffs + 1] << 8) | VidRAM[goffs];
				color = (VidRAM[goffs + 1] >> 6) | (gfx_attr & 0x0f);
				x = (sx + (xc << 3)) & 0xff;
				y = (sy + (yc << 3)) & 0xff;

				if (*flipscreen)
				{
					x = 248 - x;
					y = 248 - y;

					Render8x8Tile_Mask_FlipXY_Clip(pTransDraw, code & 0x3fff, x, y - 16, color, 4, 0xf, color_offset, gfx);
				}
				else
				{
					Render8x8Tile_Mask_Clip(pTransDraw, code & 0x3fff, x, y - 16, color, 4, 0xf, color_offset, gfx);
				}

				goffs += 2;
			}
			gfx_offs += height << 1;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0x3ff;
	}

	TC0010VCU(DrvObjRAM0, DrvVidRAM0, DrvGfxROM0, 0x000);
	TC0010VCU(DrvObjRAM1, DrvVidRAM1, DrvGfxROM1, 0x100);

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
	}

	INT32 nSegment;
	INT32 nInterleave = 10;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[4] = { 6000000 / 60, 6000000 / 60, 6000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };
	
	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);

		nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);

		nSegment = ZetTotalCycles();

		ZetClose();

		ZetOpen(1);
		nCyclesDone[0] += ZetRun(nSegment - ZetTotalCycles());
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(2);
		nCyclesDone[0] += ZetRun(nSegment - ZetTotalCycles());
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(3);
		nCyclesDone[0] += ZetRun(nSegment - ZetTotalCycles());

		if (pBurnSoundOut) {
			nSegment = nBurnSoundLen / nInterleave;

			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);

			nSoundBufferPos += nSegment;
		}

		ZetClose();
	}

	ZetOpen(3);

	if (pBurnSoundOut) {
		nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			BurnYM2151Render(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}


static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029698;
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

		BurnYM2151Scan(nAction);
		TC0140SYTScan(nAction);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(0,nBank[0]);
		ZetClose();

		ZetOpen(1);
		bankswitch(0,nBank[1]);
		ZetClose();

		DrvRecalc = 1;
	}

	return 0;
}


// Exzisus (Japan, dedicated)

static struct BurnRomInfo exzisusRomDesc[] = {
	{ "b12-09.7d",	0x10000, 0xe80f49a9, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Master)
	{ "b12-11.9d",	0x10000, 0x11fcda2c, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b12-10.7f",	0x10000, 0xa60227f1, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code (Slave A)
	{ "b12-12.8f",	0x10000, 0xa662be67, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "b12-13.10f",	0x10000, 0x04a29633, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "b12-14.12c",	0x08000, 0xb5ce5e75, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code (Slave B)

	{ "b12-21.19f",	0x08000, 0xb7e0f00e, 4 | BRF_PRG | BRF_ESS }, //  6 Z80 #3 Code (Sound)

	{ "b12-16.17d",	0x10000, 0x6fec6acb, 5 | BRF_GRA },           //  7 Chip #0 Tiles
	{ "b12-18.19d",	0x10000, 0x64e358aa, 5 | BRF_GRA },           //  8
	{ "b12-20.20d",	0x10000, 0x87f52e89, 5 | BRF_GRA },           //  9
	{ "b12-15.17c",	0x10000, 0xd81107c8, 5 | BRF_GRA },           // 10
	{ "b12-17.19c",	0x10000, 0xdb1d5a6c, 5 | BRF_GRA },           // 11
	{ "b12-19.20c",	0x10000, 0x772b2641, 5 | BRF_GRA },           // 12

	{ "b12-05.1c",	0x10000, 0xbe5c5cc1, 6 | BRF_GRA },           // 13 Chip #1 Tiles
	{ "b12-07.3c",	0x10000, 0x9353e39f, 6 | BRF_GRA },           // 14
	{ "b12-06.1d",	0x10000, 0x8571e6ed, 6 | BRF_GRA },           // 15
	{ "b12-08.3d",	0x10000, 0x55ea5cca, 6 | BRF_GRA },           // 16

	{ "b12-27.13l",	0x00100, 0x524c9a01, 7 | BRF_GRA },           // 17 Color Proms
	{ "b12-24.6m",	0x00100, 0x1aa5bde9, 7 | BRF_GRA },           // 18
	{ "b12-26.12l",	0x00100, 0x65f42c61, 7 | BRF_GRA },           // 19
	{ "b12-23.4m",	0x00100, 0xfad4db5f, 7 | BRF_GRA },           // 20
	{ "b12-25.11l",	0x00100, 0x3e30f45b, 7 | BRF_GRA },           // 21
	{ "b12-22.2m",	0x00100, 0x936855d2, 7 | BRF_GRA },           // 22
};

STD_ROM_PICK(exzisus)
STD_ROM_FN(exzisus)

static INT32 exzisusInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvExzisus = {
	"exzisus", NULL, NULL, NULL, "1987",
	"Exzisus (Japan, dedicated)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, exzisusRomInfo, exzisusRomName, NULL, NULL, ExzisusInputInfo, ExzisusDIPInfo,
	exzisusInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Exzisus (Japan, conversion)

static struct BurnRomInfo exzisusaRomDesc[] = {
	{ "b23-10.7d",	0x10000, 0xc80216fc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Master)
	{ "b23-12.9d",	0x10000, 0x13637f54, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b23-11.7f",	0x10000, 0xd6a79cef, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code (Slave A)
	{ "b12-12.8f",	0x10000, 0xa662be67, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "b12-13.10f",	0x10000, 0x04a29633, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "b23-13.12c",	0x08000, 0x51110aa1, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code (Slave B)

	{ "b23-14.19f",	0x08000, 0xf7ca7df2, 4 | BRF_PRG | BRF_ESS }, //  6 Z80 #3 Code (Sound)

	{ "b12-16.17d",	0x10000, 0x6fec6acb, 5 | BRF_GRA },           //  7 Chip #0 Tiles
	{ "b12-18.19d",	0x10000, 0x64e358aa, 5 | BRF_GRA },           //  8
	{ "b12-20.20d",	0x10000, 0x87f52e89, 5 | BRF_GRA },           //  9
	{ "b12-15.17c",	0x10000, 0xd81107c8, 5 | BRF_GRA },           // 10
	{ "b12-17.19c",	0x10000, 0xdb1d5a6c, 5 | BRF_GRA },           // 11
	{ "b12-19.20c",	0x10000, 0x772b2641, 5 | BRF_GRA },           // 12

	{ "b23-06.1c",	0x10000, 0x44f8f661, 6 | BRF_GRA },           // 13 Chip #1 Tiles
	{ "b23-08.3c",	0x10000, 0x1ce498c1, 6 | BRF_GRA },           // 14
	{ "b23-07.1d",	0x10000, 0xd7f6ec89, 6 | BRF_GRA },           // 15
	{ "b23-09.3d",	0x10000, 0x6651617f, 6 | BRF_GRA },           // 16

	{ "b23-04.15l",	0x00400, 0x5042cffa, 7 | BRF_GRA },           // 17 Color Proms
	{ "b23-03.14l",	0x00400, 0x9458fd45, 7 | BRF_GRA },           // 18
	{ "b23-05.16l",	0x00400, 0x87f0f69a, 7 | BRF_GRA },           // 19
};

STD_ROM_PICK(exzisusa)
STD_ROM_FN(exzisusa)

static INT32 exzisusaInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvExzisusa = {
	"exzisusa", "exzisus", NULL, NULL, "1987",
	"Exzisus (Japan, conversion)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, exzisusaRomInfo, exzisusaRomName, NULL, NULL, ExzisusInputInfo, ExzisusDIPInfo,
	exzisusaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};


// Exzisus (TAD license)

static struct BurnRomInfo exzisustRomDesc[] = {
	{ "b23-10.7d",	0x10000, 0xc80216fc, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code (Master)
	{ "b23-12.9d",	0x10000, 0x13637f54, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "b23-15.7f",	0x10000, 0x2f8b3752, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #2 Code (Slave A)
	{ "b12-12.8f",	0x10000, 0xa662be67, 2 | BRF_PRG | BRF_ESS }, //  3
	{ "b12-13.10f",	0x10000, 0x04a29633, 2 | BRF_PRG | BRF_ESS }, //  4

	{ "b23-13.12c",	0x08000, 0x51110aa1, 3 | BRF_PRG | BRF_ESS }, //  5 Z80 #2 Code (Slave B)

	{ "b23-14.19f",	0x08000, 0xf7ca7df2, 4 | BRF_PRG | BRF_ESS }, //  6 Z80 #3 Code (Sound)

	{ "b12-16.17d",	0x10000, 0x6fec6acb, 5 | BRF_GRA },           //  7 Chip #0 Tiles
	{ "b12-18.19d",	0x10000, 0x64e358aa, 5 | BRF_GRA },           //  8
	{ "b12-20.20d",	0x10000, 0x87f52e89, 5 | BRF_GRA },           //  9
	{ "b12-15.17c",	0x10000, 0xd81107c8, 5 | BRF_GRA },           // 10
	{ "b12-17.19c",	0x10000, 0xdb1d5a6c, 5 | BRF_GRA },           // 11
	{ "b12-19.20c",	0x10000, 0x772b2641, 5 | BRF_GRA },           // 12

	{ "b23-06.1c",	0x10000, 0x44f8f661, 6 | BRF_GRA },           // 13 Chip #1 Tiles
	{ "b23-08.3c",	0x10000, 0x1ce498c1, 6 | BRF_GRA },           // 14
	{ "b23-07.1d",	0x10000, 0xd7f6ec89, 6 | BRF_GRA },           // 15
	{ "b23-09.3d",	0x10000, 0x6651617f, 6 | BRF_GRA },           // 16

	{ "b23-04.15l",	0x00400, 0x5042cffa, 7 | BRF_GRA },           // 17 Color Proms
	{ "b23-03.14l",	0x00400, 0x9458fd45, 7 | BRF_GRA },           // 18
	{ "b23-05.16l",	0x00400, 0x87f0f69a, 7 | BRF_GRA },           // 19
};

STD_ROM_PICK(exzisust)
STD_ROM_FN(exzisust)

struct BurnDriver BurnDrvExzisust = {
	"exzisust", "exzisus", NULL, NULL, "1987",
	"Exzisus (TAD license)\0", NULL, "Taito Corporation (TAD license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, exzisustRomInfo, exzisustRomName, NULL, NULL, ExzisusInputInfo, ExzisusDIPInfo,
	exzisusaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	256, 224, 4, 3
};
