// FB Alpha Super Rider driver module
// Based on MAME driver by Aaron Giles
//
// Notes: Added a lowpass filter effect to get rid of the headache-inducing
// high pitched hissing noise.

#include "tiles_generic.h"
#include "z80_intf.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}
#include "lowpass2.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvBgRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvFgRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT16 *pAY8910Buffer[6];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static UINT8 tilemapflipx;
static UINT8 tilemapflipy;
static UINT8 nmi_enable;
static UINT8 soundlatch;
static UINT8 fgdisable;
static UINT8 fgscrolly;
static UINT8 bgscrolly;

static INT32 watchdog;

static class LowPass2 *LP1 = NULL, *LP2 = NULL;
#define SampleFreq 44100.0
#define CutFreq 1000.0
#define Q 0.4
#define Gain 1.0
#define CutFreq2 1000.0
#define Q2 0.3
#define Gain2 1.475

static struct BurnInputInfo DrvInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy3 + 2,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 coin"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Service",		BIT_DIGITAL,	DrvJoy3 + 1,    "service"       },
	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dips",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Drv)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x11, NULL                     },

	{0,    0xfe,    0,    8, "Coinage"	          },
	{0x0d, 0x01, 0x07, 0x00, "2 Coins 1 Credit"	  },
	{0x0d, 0x01, 0x07, 0x01, "1 Coin  1 Credit"	  },
	{0x0d, 0x01, 0x07, 0x02, "1 Coin  2 Credits"	  },
	{0x0d, 0x01, 0x07, 0x03, "1 Coin  3 Credits"	  },
	{0x0d, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	  },
	{0x0d, 0x01, 0x07, 0x05, "1 Coin  5 Credits"	  },
	{0x0d, 0x01, 0x07, 0x06, "1 Coin  6 Credits"	  },
	{0x0d, 0x01, 0x07, 0x07, "1 Coin  7 Credits"	  },

	{0   , 0xfe, 0   , 4   , "Timer Speed"            },
	{0x0d, 0x01, 0x18, 0x18, "Slow"     		  },
	{0x0d, 0x01, 0x18, 0x10, "Medium"    		  },
	{0x0d, 0x01, 0x18, 0x08, "Fast"     		  },
	{0x0d, 0x01, 0x18, 0x00, "Fastest"    		  },

	{0   , 0xfe, 0   , 2   , "Bonus"                  },
	{0x0d, 0x01, 0x20, 0x00, "200k"       		  },
	{0x0d, 0x01, 0x20, 0x20, "400k"       		  },

	{0   , 0xfe, 0   , 2   , "Cabinet"                },
	{0x0d, 0x01, 0x40, 0x00, "Upright"     		  },
	{0x0d, 0x01, 0x40, 0x40, "Cocktail"    		  },

	{0   , 0xfe, 0   , 2   , "Invulnerability?"       },
	{0x0d, 0x01, 0x80, 0x00, "Off"     		  },
	{0x0d, 0x01, 0x80, 0x80, "On"    		  },
};

STDDIPINFO(Drv)

static void __fastcall supridr_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xb000:
			nmi_enable = data;
		return;

		case 0xb002:
		case 0xb003:
		//	coin_lockout = ~data & 0x01;
		return;

		case 0xb006:
			tilemapflipx = data & 1;
		return;

		case 0xb007:
			tilemapflipy = data & 1;
		return;

		case 0xb800:
			soundlatch = data;
			ZetClose();
			ZetOpen(1);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xc801:
			fgdisable = ~data & 0x01;
		return;

		case 0xc802:
			fgscrolly = data;
		return;

		case 0xc804:
			bgscrolly = data;
		return;
	}
}

static UINT8 __fastcall supridr_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return DrvInputs[tilemapflipx ? 1 : 0]; // p2 / p1

		case 0xa800:
			return DrvInputs[2]; // system

		case 0xb000:
			return DrvDips[0];
	}

	return 0;
}

static UINT8 __fastcall supridr_main_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			watchdog = 0;
			return 0;
	}

	return 0;
}

static void __fastcall supridr_sound_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x8c:
		case 0x8d:
		case 0x8e:
		case 0x8f:
			AY8910Write((port & 2)/2, port & 1, data);
		return;
	}
}

static UINT8 __fastcall supridr_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x8d:
		case 0x8f:
			return AY8910Read((port & 2)/2);
	}

	return 0;
}

static UINT8 ay8910_1_portA(UINT32)
{
	return soundlatch;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);
	AY8910Reset(1);

	tilemapflipx = 0;
	tilemapflipy = 0;
	nmi_enable = 0;
	soundlatch = 0;
	watchdog = 0;
	fgdisable = 0;
	fgscrolly = 0;
	bgscrolly = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0	= Next; Next += 0x010000;
	DrvZ80ROM1	= Next; Next += 0x011000;

	DrvGfxROM0	= Next; Next += 0x014000;
	DrvGfxROM1	= Next; Next += 0x014000;
	DrvGfxROM2	= Next; Next += 0x018000;

	DrvColPROM	= Next; Next += 0x000060;

	DrvPalette	= (UINT32*)Next; Next += 0x0060 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ80RAM0	= Next; Next += 0x010800;
	DrvZ80RAM1	= Next; Next += 0x010400;
	DrvBgRAM	= Next; Next += 0x010400;
	DrvSprRAM	= Next; Next += 0x010400;
	DrvFgRAM	= Next; Next += 0x010800;

	RamEnd		= Next;

	pAY8910Buffer[0] = (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1] = (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2] = (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3] = (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4] = (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5] = (INT16 *)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd		= Next;

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x60; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = 0x4f * bit0 + 0xa8 * bit1;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void DrvGfxDecode()
{
	INT32 Plane0[4]  = { 0x1000 * 8, 0x1000 * 8 + 4, 0 , 4 };
	INT32 XOffs0[8]  = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };

	INT32 Plane1[3]  = { 0x1000 * 8 * 0, 0x1000 * 8 * 1, 0x1000 * 8 * 2 };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(8*8,1) };
	INT32 YOffs1[16] = { STEP8(0,8), STEP8(8*8*2,8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0x3000);
	if (tmp == NULL) {
		return;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0100, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0100, 4,  8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x3000);

	GfxDecode(0x0080, 3, 16, 16, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM2);

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
		if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x1000,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x3000,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x5000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x6000,  6, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x7000,  7, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0xc000,  8, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0xd000,  9, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0xe000, 10, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x0000, 11, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1000, 13, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000, 14, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x1000, 15, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x0000, 16, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x1000, 17, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x2000, 18, 1)) return 1;

		if (BurnLoadRom(DrvColPROM + 0x0000, 19, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0020, 20, 1)) return 1;
		if (BurnLoadRom(DrvColPROM + 0x0040, 21, 1)) return 1;

		DrvGfxDecode();
		DrvPaletteInit();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,		0x8000, 0x87ff, MAP_RAM);
	ZetMapMemory(DrvBgRAM,			0x8800, 0x8bff, MAP_RAM);
	ZetMapMemory(DrvFgRAM,			0x9000, 0x97ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,			0x9800, 0x9bff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM0 + 0xc000,	0xc000, 0xefff, MAP_ROM);
	ZetSetWriteHandler(supridr_main_write);
	ZetSetReadHandler(supridr_main_read);
	ZetSetInHandler(supridr_main_read_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x0fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM1,		0x3800, 0x3bff, MAP_RAM);
	ZetSetOutHandler(supridr_sound_write_port);
	ZetSetInHandler(supridr_sound_read_port);
	ZetClose();

	AY8910Init(0, 1536000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910Init(1, 1536000, nBurnSoundRate, &ay8910_1_portA, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.05, BURN_SND_ROUTE_BOTH);
	AY8910SetAllRoutes(1, 0.05, BURN_SND_ROUTE_BOTH);
	GenericTilesInit();

	LP1 = new LowPass2(CutFreq, SampleFreq, Q, Gain,
					   CutFreq2, Q2, Gain2);
	LP2 = new LowPass2(CutFreq, SampleFreq, Q, Gain,
					   CutFreq2, Q2, Gain2);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);
	AY8910Exit(1);

	BurnFree (AllMem);

	delete LP1;
	delete LP2;
	LP1 = NULL;
	LP2 = NULL;

	return 0;
}

static void draw_bg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		if (sx > 4*8 && sx < (nScreenWidth - 1*8)) // don't scroll the player's status lines
			sy -= bgscrolly;

		if (sy < -7) sy += 256;

		sy -= 16; // offsets

		INT32 code = DrvBgRAM[offs];

		Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 4, 0x00, DrvGfxROM0);
	}
}

static void draw_fg_layer()
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		if (sx >= 32 && sx < 248) sy -= fgscrolly;
		if (sy < -7) sy += 256;
		sy -= 16; // offset
		INT32 code = DrvFgRAM[offs];

		Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 4, 0, 0x20, DrvGfxROM1);
	}
}

static void draw_sprites()
{
	UINT8 *spr = DrvSprRAM + 0x40;

	for (INT32 i = 0; i < 48; i++)
	{
		INT32 code = (spr[i*4+1] & 0x3f) | ((spr[i*4+2] >> 1) & 0x40);
		INT32 color = spr[i*4+2] & 0x03; // iq_132... right? or 0x7f...
		INT32 flipx = spr[i*4+1] & 0x40;
		INT32 flipy = spr[i*4+1] & 0x80;
		INT32 sx = spr[i*4+3];
		INT32 sy = (240 - spr[i*4+0]) - 16;

		if (tilemapflipx)
		{
			flipx = !flipx;
			sx = 240 - sx;
		}
		if (tilemapflipy)
		{
			flipy = !flipy;
			sy = 240 - sy;
		}

		if (flipy) {
			if (flipx) {
				Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
			}
		} else {
			if (flipx) {
				Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
			} else {
				Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3, 0, 0x40, DrvGfxROM2);
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_bg_layer();
	if (fgdisable) draw_fg_layer();

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nCyclesTotal[2] = { 3072000 / 60, 2500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nInterleave = 100;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == (nInterleave - 1) && nmi_enable) ZetNmi();
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		if (LP1 && LP2) {
			LP1->Filter(pBurnSoundOut, nBurnSoundLen);  // Left
			LP2->Filter(pBurnSoundOut+1, nBurnSoundLen); // Right
		}
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

		SCAN_VAR(tilemapflipx);
		SCAN_VAR(tilemapflipy);
		SCAN_VAR(soundlatch);
		SCAN_VAR(nmi_enable);
		SCAN_VAR(fgscrolly);
		SCAN_VAR(bgscrolly);
		SCAN_VAR(fgdisable);
	}

	return 0;
}


// Super Rider

static struct BurnRomInfo suprridrRomDesc[] = {
	{ "sr8",    0x1000, 0x4a1f0a6c, 0 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "sr7",    0x1000, 0x523ee717, 0 | BRF_ESS | BRF_PRG }, //  1
	{ "sr4",    0x1000, 0x300370ae, 0 | BRF_ESS | BRF_PRG }, //  2
	{ "sr5",    0x1000, 0xc5bca683, 0 | BRF_ESS | BRF_PRG }, //  3
	{ "sr6",    0x1000, 0x563bab28, 0 | BRF_ESS | BRF_PRG }, //  4
	{ "sr3",    0x1000, 0x4b9d2ec5, 0 | BRF_ESS | BRF_PRG }, //  5
	{ "sr2",    0x1000, 0x6fe18e1d, 0 | BRF_ESS | BRF_PRG }, //  6
	{ "sr1",    0x1000, 0xf2ae64b3, 0 | BRF_ESS | BRF_PRG }, //  7
	{ "1",      0x1000, 0xcaf12fa2, 0 | BRF_ESS | BRF_PRG }, //  8
	{ "2",      0x1000, 0x2b3c638e, 0 | BRF_ESS | BRF_PRG }, //  9
	{ "3",      0x1000, 0x2abdb5f4, 0 | BRF_ESS | BRF_PRG }, // 10

	{ "sr9",    0x1000, 0x1c5dba78, 1 | BRF_ESS | BRF_PRG }, // 11 Z80 #1 Code

	{ "sr10",   0x1000, 0xa57ac8d0, 2 | BRF_GRA },           // 12 Background Tiles
	{ "sr11",   0x1000, 0xaa7ec7b2, 2 | BRF_GRA },           // 13

	{ "sr15",   0x1000, 0x744f3405, 2 | BRF_GRA },           // 14 Foreground Tiles
	{ "sr16",   0x1000, 0x3e1a876b, 2 | BRF_GRA },           // 15

	{ "sr12",   0x1000, 0x81494fe8, 2 | BRF_GRA },           // 16 Sprite Tiles
	{ "sr13",   0x1000, 0x63e94648, 2 | BRF_GRA },           // 17
	{ "sr14",   0x1000, 0x277a70af, 2 | BRF_GRA },           // 18

	{ "clr.1b", 0x0020, 0x87a79fe8, 2 | BRF_GRA },           // 19 Color PROMs
	{ "clr.9c", 0x0020, 0x10d63240, 2 | BRF_GRA },           // 20
	{ "clr.8a", 0x0020, 0x917eabcd, 2 | BRF_GRA },           // 21
};

STD_ROM_PICK(suprridr)
STD_ROM_FN(suprridr)

struct BurnDriver BurnDrvSuprridr = {
	"suprridr", NULL, NULL, NULL, "1983",
	"Super Rider\0", NULL, "Taito Corporation (Venture Line license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_TAITO, GBF_MISC, 0,
	NULL, suprridrRomInfo, suprridrRomName, NULL, NULL, DrvInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x60,
	224, 256, 3, 4
};
