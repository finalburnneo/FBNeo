// FB Alpha The Deep driver module
// Based on MAME driver by Luca Elia

#include "tiles_generic.h"
#include "z80_intf.h"
#include "m6502_intf.h"
#include "burn_ym2203.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvMCUROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvVidRAM0;
static UINT8 *DrvVidRAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvScrollRAM;

static UINT8 *scroll;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 rom_bank;
static UINT8 nmi_enable;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 protection_data;
static UINT8 protection_command;
static INT32 protection_index;
static UINT8 protection_irq;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[5];
static UINT8 DrvInput4;
static UINT8 DrvReset;

static INT32 hold_coin[4];

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvInputs + 2,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvInputs + 3,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x12, 0xff, 0xff, 0xbf, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x03, 0x03, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x03, 0x02, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x03, 0x01, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x03, 0x00, "1 Coin  4 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0x0c, 0x00, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x04, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0c, 0x0c, "1 Coin  1 Credits"	},

	{0   , 0xfe, 0   ,    2, "Invulnerability"	},
	{0x12, 0x01, 0x10, 0x10, "Off"			},
	{0x12, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Start Stage"		},
	{0x12, 0x01, 0x20, 0x20, "1"			},
	{0x12, 0x01, 0x20, 0x00, "4"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x12, 0x01, 0x40, 0x40, "Off"			},
	{0x12, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Harder"		},
	{0x13, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x0c, 0x0c, "3"			},
	{0x13, 0x01, 0x0c, 0x08, "4"			},
	{0x13, 0x01, 0x0c, 0x04, "5"			},
	{0x13, 0x01, 0x0c, 0x00, "6"			},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x13, 0x01, 0x30, 0x00, "50k"			},
	{0x13, 0x01, 0x30, 0x30, "50k 70k"		},
	{0x13, 0x01, 0x30, 0x20, "60k 80k"		},
	{0x13, 0x01, 0x30, 0x10, "80k 100k"		},

	{0   , 0xfe, 0   ,    0, "Service Mode"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Drv)

static void bankswitch(INT32 data)
{
	rom_bank = data;

	ZetMapMemory(DrvZ80ROM + 0x10000 + (rom_bank * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void protection_write(UINT8 data)
{
	protection_command = data;

	switch (protection_command)
	{
		case 0x11:
		case 0x20:
			flipscreen = protection_command & 1;
		break;

		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
			bankswitch(protection_command & 3);
		break;

		case 0x59:
		{
			if (protection_index < 0) protection_index = 0;

			if (protection_index < 0x19b)
				protection_data = DrvMCUROM[0x185 + protection_index++];
			else
				protection_data = 0xc9;

			protection_irq  = 1;
		}
		break;
	}
}

static void __fastcall thedeep_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			protection_write(data);
		return;

		case 0xe004:
			nmi_enable = data;
		return;

		case 0xe00c:
			soundlatch = data;
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_ACK);
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
			protection_irq = 0;
			return protection_data;

		case 0xe004:
			return protection_irq ? 1 : 0;

		case 0xe008:
		case 0xe009:
		case 0xe00a:
		case 0xe00b:
			return DrvInputs[address & 3];
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
			M6502SetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

inline static void DrvYM2203IRQHandler(INT32, INT32 nStatus)
{
	M6502SetIRQLine(0, ((nStatus) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE));
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)(M6502TotalCycles() * nSoundRate / 1500000);
}

inline static double DrvGetTime()
{
	return (double)M6502TotalCycles() / 1500000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetClose();

	M6502Open(0);
	M6502Reset();
	BurnYM2203Reset();
	M6502Close();

	nmi_enable = 0;	
	soundlatch = 0;
	flipscreen = 0;
	protection_data = 0;
	protection_command = 0;
	protection_index = -1;
	protection_irq = 0;

	memset (hold_coin, 0, 4 * sizeof(INT32));
	DrvInput4 = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x020000;
	DrvM6502ROM		= Next; Next += 0x008000;
	DrvMCUROM		= Next; Next += 0x001000;

	DrvGfxROM0		= Next; Next += 0x080000;
	DrvGfxROM1		= Next; Next += 0x080000;
	DrvGfxROM2		= Next; Next += 0x010000;

	DrvColPROM		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x000201 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x002000;
	DrvM6502RAM		= Next; Next += 0x000800;
	DrvVidRAM0		= Next; Next += 0x000800;
	DrvVidRAM1		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000400;
	DrvScrollRAM		= Next; Next += 0x000800;

	scroll			= Next; Next += 0x000004;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0, 4 };
	INT32 XOffs0[8]  = { STEP4((0x4000*8)/2,1), STEP4(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,8) };

	INT32 Plane1[4]  = { 0x80000 * 0, 0x80000 * 1, 0x80000 * 2, 0x80000 * 3 };
	INT32 XOffs1[16] = { STEP8(128,1), STEP8(0,1) };
	INT32 YOffs1[16] = { STEP16(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x40000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x40000);

	GfxDecode(0x0800, 4, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x04000);

	GfxDecode(0x0400, 2,  8,  8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
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
		if (BurnLoadRom(DrvZ80ROM + 0x00000,   0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM + 0x10000,   1, 1)) return 1;
	
		if (BurnLoadRom(DrvM6502ROM,           2, 1)) return 1;
	
		if (BurnLoadRom(DrvMCUROM,             3, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM0 + 0x00000,  4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x10000,  5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x20000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x30000,  7, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM1 + 0x00000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x10000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x20000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x30000, 11, 1)) return 1;
	
		if (BurnLoadRom(DrvGfxROM2 + 0x00000, 12, 1)) return 1;

	//	if (BurnLoadRom(DrvColPROM + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00000, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x00200, 15, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xc000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe400, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvVidRAM1,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvVidRAM0,	0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvScrollRAM,	0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(thedeep_main_write);
	ZetSetReadHandler(thedeep_main_read);
	ZetClose();

	M6502Init(0, TYPE_M65C02);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,	0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(thedeep_sound_write);
	M6502SetReadHandler(thedeep_sound_read);
	M6502Close();

	BurnYM2203Init(1,  3000000, &DrvYM2203IRQHandler, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachM6502(1500000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE,   0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	M6502Exit();

	BurnYM2203Exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 512; i++)
	{
		UINT8 r = DrvColPROM[0x200 + i] & 0xf;
		INT32 g = DrvColPROM[0x200 + i] >> 4;
		INT32 b = DrvColPROM[0x000 + i] & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}

	DrvPalette[0x200] = 0; // black
}

static void draw_background()
{
	INT32 scrollx = (scroll[0] + (scroll[1] << 8)) & 0x1ff;
	INT32 scrolly = (scroll[2] + (scroll[3] << 8)) & 0x1ff;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 col = offs & 0x1f; // sx
		INT32 row = offs / 0x20; // sy

		INT32 ofst = (col & 0x0f) + ((col & 0x10) << 5) + (row << 4);

		INT32 sx = col * 16;
		INT32 sy = row * 16;

		INT32 yscroll = (scrolly + (DrvScrollRAM[col*2+0] + ((DrvScrollRAM[col*2+1] & 1) * 256))) & 0x1ff;

		sx -= scrollx;
		if (sx < -15) sx += 512;
		sy -= yscroll;
		if (sy < -15) sy += 512;

		if (sy >= nScreenHeight || sx >= nScreenWidth) continue;

		INT32 attr = DrvVidRAM0[ofst * 2 + 1];
		INT32 code = DrvVidRAM0[ofst * 2 + 0] + ((attr & 0x0f) << 8);
		INT32 color = attr >> 4;

		Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code & 0x7ff, sx, sy, color, 4, 0, 0x100, DrvGfxROM1);
	}
}

static void draw_foreground()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr = DrvVidRAM1[offs * 2 + 1];
		INT32 code = DrvVidRAM1[offs * 2 + 0] + ((attr & 0x0f) << 8);
		INT32 color = attr >> 4;

		Render8x8Tile_Mask_Clip(pTransDraw, code & 0x3ff, sx, sy, color, 2, 0, 0, DrvGfxROM2);
	}
}

static void draw_sprites()
{
	int offs = 0;
	UINT16 *spriteram = (UINT16*)DrvSprRAM;

	while (offs < (0x400 / 2))
	{
		int sx, sy, code, color, w, h, flipx, flipy, incy, flash, mult, x, y;

		sy = spriteram[offs];
		sx = spriteram[offs + 2];
		color = sx >> 12;

		flash = sx & 0x800;

		flipx = sy & 0x2000;
		flipy = sy & 0x4000;
		h = (1 << ((sy & 0x1800) >> 11));
		w = (1 << ((sy & 0x0600) >>  9));

		sx = sx & 0x01ff;
		sy = sy & 0x01ff;
		if (sx >= 256) sx -= 512;
		if (sy >= 256) sy -= 512;
		sx = 240 - sx;
		sy = 240 - sy;

		if (flipscreen)
		{
			sy = 240 - sy;
			sx = 240 - sx;
			if (flipx) flipx = 0; else flipx = 1;
			if (flipy) flipy = 0; else flipy = 1;
			mult = 16;
		}
		else
			mult = -16;

		// thedeep strongly suggests that this check goes here, otherwise the radar breaks
		if (!(spriteram[offs] & 0x8000))
		{
			offs += 4;
			continue;
		}

		for (x = 0; x < w; x++)
		{
			code = (spriteram[offs + 1] & 0x1fff) & ~(h - 1);

			if (flipy)
				incy = -1;
			else
			{
				code += h - 1;
				incy = 1;
			}

			for (y = 0; y < h; y++)
			{
				int draw = 0;

				if (!flash || (nCurrentFrame & 1))
				{
					draw = 1;
				}

				if (draw)
				{
					INT32 code2 = (code - y * incy) & 0x7ff;

					if (flipy) {
						if (flipx) {
							Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code2, sx + (mult * x), sy + (mult * y), color+8, 4, 0, 0, DrvGfxROM0);
						} else {
							Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code2, sx + (mult * x), sy + (mult * y), color+8, 4, 0, 0, DrvGfxROM0);
						}
					} else {
						if (flipx) {
							Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code2, sx + (mult * x), sy + (mult * y), color+8, 4, 0, 0, DrvGfxROM0);
						} else {
							Render16x16Tile_Mask_Clip(pTransDraw, code2, sx + (mult * x), sy + (mult * y), color+8, 4, 0, 0, DrvGfxROM0);
						}
					}
				}
			}

			offs += 4;
			if (offs >= (0x400 / 2))
				return;
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
		pTransDraw[i] = 0x200;
	}

	draw_background();
	draw_sprites();
	draw_foreground();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void interrupt_handler(INT32 param)
{
	INT32 scanline = param;

	if (scanline == 124) // TODO: clean this
	{
		if (protection_command != 0x59)
		{
			int coins = DrvInput4;
			if      (coins & 1) protection_data = 1;
			else if (coins & 2) protection_data = 2;
			else if (coins & 4) protection_data = 3;
			else                protection_data = 0;

			if (protection_data)
				protection_irq = 1;
		}
		if (protection_irq)
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	}
	else if(scanline == 0)
	{
		if (nmi_enable)
		{
			ZetNmi();
		}
	}
}

static INT32 previous_coin = 0;

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();
	M6502NewFrame();

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;
		DrvInputs[4] = 0; // real inputs #4
		DrvInput4 = 0;    // hold coin for 1 single frame-processed.

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy3[i] & 1) << i;
		}

		for (INT32 i = 0; i < 4; i++) {
			if ((previous_coin != (DrvInputs[4]&7)) && DrvJoy3[i] && !hold_coin[i]) {
				hold_coin[i] = 2; // frames to hold coin + 1
			}

			if (hold_coin[i]) {
				hold_coin[i]--;
				DrvInput4 |= 1<<i;
			}
			if (!hold_coin[i]) {
				if (DrvInput4 & 1<<i)
					DrvInput4 ^= 1<<i;
			}
		}

		previous_coin = DrvInputs[4] & 7;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 1500000 / 60, -1 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	ZetOpen(0);
	M6502Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = nCyclesTotal[0] / nInterleave;

		nCyclesDone[0] += ZetRun(nSegment);
		interrupt_handler(i);

		BurnTimerUpdate(ZetTotalCycles() / 4);
	}

	BurnTimerEndFrame(nCyclesTotal[1]);
	
	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
	}

	M6502Close();
	ZetClose();

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
		M6502Scan(nAction);

		BurnYM2203Scan(nAction, pnMin);

		SCAN_VAR(rom_bank);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(protection_data);
		SCAN_VAR(protection_command);
		SCAN_VAR(protection_index);
		SCAN_VAR(protection_irq);
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

	{ "dp-14",	0x01000, 0x0b886dad, 3 | BRF_PRG | BRF_ESS }, //  3 i8751 Code

	{ "dp-08.rom",	0x10000, 0xc5624f6b, 4 | BRF_GRA },           //  4 Sprites
	{ "dp-07.rom",	0x10000, 0xc76768c1, 4 | BRF_GRA },           //  5
	{ "dp-06.rom",	0x10000, 0x98adea78, 4 | BRF_GRA },           //  6
	{ "dp-05.rom",	0x10000, 0x76ea7dd1, 4 | BRF_GRA },           //  7

	{ "dp-03.rom",	0x10000, 0x6bf5d819, 5 | BRF_GRA },           //  8 Background Tiles
	{ "dp-01.rom",	0x10000, 0xe56be2fe, 5 | BRF_GRA },           //  9
	{ "dp-04.rom",	0x10000, 0x4db02c3c, 5 | BRF_GRA },           // 10
	{ "dp-02.rom",	0x10000, 0x1add423b, 5 | BRF_GRA },           // 11

	{ "dp-11.rom",	0x04000, 0x196e23d1, 6 | BRF_GRA },           // 12 Foreground Tiles

	{ "fi-1",	0x00200, 0xf31efe09, 7 | BRF_OPT },           // 13 Color Data
	{ "fi-2",	0x00200, 0xf305c8d5, 7 | BRF_GRA },           // 14
	{ "fi-3",	0x00200, 0xf61a9686, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(thedeep)
STD_ROM_FN(thedeep)

struct BurnDriver BurnDrvThedeep = {
	"thedeep", NULL, NULL, NULL, "1987",
	"The Deep (Japan)\0", NULL, "Wood Place Inc.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, thedeepRomInfo, thedeepRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x201,
	248, 256, 3, 4
};


// Run Deep

static struct BurnRomInfo rundeepRomDesc[] = {
	{ "3",		0x08000, 0xc9c9e194, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "9",		0x10000, 0x931f4e67, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "dp-12.rom",	0x08000, 0xc4e848c4, 2 | BRF_PRG | BRF_ESS }, //  2 M65c02 Code

	{ "dp-14",	0x01000, 0x0b886dad, 3 | BRF_PRG | BRF_ESS }, //  3 i8751 Code

	{ "dp-08.rom",	0x10000, 0xc5624f6b, 4 | BRF_GRA },           //  4 sprites
	{ "dp-07.rom",	0x10000, 0xc76768c1, 4 | BRF_GRA },           //  5
	{ "dp-06.rom",	0x10000, 0x98adea78, 4 | BRF_GRA },           //  6
	{ "dp-05.rom",	0x10000, 0x76ea7dd1, 4 | BRF_GRA },           //  7

	{ "dp-03.rom",	0x10000, 0x6bf5d819, 5 | BRF_GRA },           //  8 Background Tiles
	{ "dp-01.rom",	0x10000, 0xe56be2fe, 5 | BRF_GRA },           //  9
	{ "dp-04.rom",	0x10000, 0x4db02c3c, 5 | BRF_GRA },           // 10
	{ "dp-02.rom",	0x10000, 0x1add423b, 5 | BRF_GRA },           // 11

	{ "11",		0x04000, 0x5d29e4b9, 6 | BRF_GRA },           // 12 Foreground Tiles

	{ "fi-1",	0x00200, 0xf31efe09, 7 | BRF_OPT },           // 13 Color Data
	{ "fi-2",	0x00200, 0xf305c8d5, 7 | BRF_GRA },           // 14
	{ "fi-3",	0x00200, 0xf61a9686, 7 | BRF_GRA },           // 15
};

STD_ROM_PICK(rundeep)
STD_ROM_FN(rundeep)

struct BurnDriver BurnDrvRundeep = {
	"rundeep", "thedeep", NULL, NULL, "1988",
	"Run Deep\0", NULL, "bootleg (Cream)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, rundeepRomInfo, rundeepRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x201,
	248, 256, 3, 4
};
