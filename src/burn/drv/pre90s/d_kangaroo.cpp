// FB Alpha Kangaroo / Funky Fish driver module
// Based on MAME driver by Ville Laitinen and Aaron Giles

#include "tiles_generic.h"
#include "z80_intf.h"
#include "bitswap.h"
#include "ay8910.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvGfxROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT32 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 mcu_value;
static UINT8 nDrvBank;
static UINT8 soundlatch;
static UINT8 *video_control;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo FnkyfishInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Fnkyfish)

static struct BurnInputInfo KangarooInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 0,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Kangaroo)

static struct BurnDIPInfo KangarooDIPList[]=
{
	{0x10, 0xff, 0xff, 0x00, NULL				},
	{0x11, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x10, 0x01, 0x01, 0x00, "3"				},
	{0x10, 0x01, 0x01, 0x01, "5"				},

	{0   , 0xfe, 0   ,    2, "Difficulty"			},
	{0x10, 0x01, 0x02, 0x00, "Easy"				},
	{0x10, 0x01, 0x02, 0x02, "Hard"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x10, 0x01, 0x0c, 0x08, "10000 30000"			},
	{0x10, 0x01, 0x0c, 0x0c, "20000 40000"			},
	{0x10, 0x01, 0x0c, 0x04, "10000"			},
	{0x10, 0x01, 0x0c, 0x00, "None"				},

	{0   , 0xfe, 0   ,    16, "Coinage"			},
	{0x10, 0x01, 0xf0, 0x10, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0xf0, 0x20, "A 2C/1C B 1C/3C"		},
	{0x10, 0x01, 0xf0, 0x00, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0xf0, 0x30, "A 1C/1C B 1C/2C"		},
	{0x10, 0x01, 0xf0, 0x40, "A 1C/1C B 1C/3C"		},
	{0x10, 0x01, 0xf0, 0x50, "A 1C/1C B 1C/4C"		},
	{0x10, 0x01, 0xf0, 0x60, "A 1C/1C B 1C/5C"		},
	{0x10, 0x01, 0xf0, 0x70, "A 1C/1C B 1C/6C"		},
	{0x10, 0x01, 0xf0, 0x80, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0xf0, 0x90, "A 1C/2C B 1C/4C"		},
	{0x10, 0x01, 0xf0, 0xa0, "A 1C/2C B 1C/5C"		},
	{0x10, 0x01, 0xf0, 0xe0, "A 1C/2C B 1C/6C"		},
	{0x10, 0x01, 0xf0, 0xb0, "A 1C/2C B 1C/10C"		},
	{0x10, 0x01, 0xf0, 0xc0, "A 1C/2C B 1C/11C"		},
	{0x10, 0x01, 0xf0, 0xd0, "A 1C/2C B 1C/12C"		},
	{0x10, 0x01, 0xf0, 0xf0, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Music"			},
	{0x11, 0x01, 0x20, 0x20, "Off"				},
	{0x11, 0x01, 0x20, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x11, 0x01, 0x40, 0x00, "Upright"			},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x11, 0x01, 0x80, 0x00, "Off"				},
	{0x11, 0x01, 0x80, 0x80, "On"				},
};

STDDIPINFO(Kangaroo)

static struct BurnDIPInfo FnkyfishDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x02, NULL				},
	{0x10, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "Lives"			},
	{0x0f, 0x01, 0x01, 0x00, "3"				},
	{0x0f, 0x01, 0x01, 0x01, "5"				},

	{0   , 0xfe, 0   ,    2, "Cabinet"			},
	{0x0f, 0x01, 0x02, 0x02, "Upright"			},
	{0x0f, 0x01, 0x02, 0x00, "Cocktail"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x04, 0x00, "Off"				},
	{0x0f, 0x01, 0x04, 0x04, "On"				},
};

STDDIPINFO(Fnkyfish)

static void videoram_write(UINT16 offset, UINT8 data, UINT8 mask)
{
	UINT32 expdata = 0;
	if (data & 0x01) expdata |= 0x00000055;
	if (data & 0x10) expdata |= 0x000000aa;
	if (data & 0x02) expdata |= 0x00005500;
	if (data & 0x20) expdata |= 0x0000aa00;
	if (data & 0x04) expdata |= 0x00550000;
	if (data & 0x40) expdata |= 0x00aa0000;
	if (data & 0x08) expdata |= 0x55000000;
	if (data & 0x80) expdata |= 0xaa000000;

	UINT32 layermask = 0;
	if (mask & 0x08) layermask |= 0x30303030;
	if (mask & 0x04) layermask |= 0xc0c0c0c0;
	if (mask & 0x02) layermask |= 0x03030303;
	if (mask & 0x01) layermask |= 0x0c0c0c0c;

	DrvVidRAM[offset] = (DrvVidRAM[offset] & ~layermask) | (expdata & layermask);
}

static void blitter_run()
{
	UINT16 src = video_control[0] + (256 * video_control[1]);
	UINT16 dst = video_control[2] + (256 * video_control[3]);
	UINT8 height = video_control[5];
	UINT8 width = video_control[4];
	UINT8 mask = video_control[8];

	if (mask & 0x0c) mask |= 0x0c;
	if (mask & 0x03) mask |= 0x03;

	for (INT32 y = 0; y <= height; y++, dst += 256)
	{
		for (INT32 x = 0; x <= width; x++)
		{
			UINT16 effdst = (dst + x) & 0x3fff;
			UINT16 effsrc = src++ & 0x1fff;
			videoram_write(effdst, DrvGfxROM[0x0000 + effsrc], mask & 0x05);
			videoram_write(effdst, DrvGfxROM[0x2000 + effsrc], mask & 0x0a);
		}
	}
}

static void bankswitch(INT32 data)
{
	nDrvBank = data;

	ZetMapMemory(DrvGfxROM + (data & 1) * 0x2000, 0xc000, 0xd000, MAP_ROM);
}

static UINT8 __fastcall kangaroo_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0xe400) {
		return DrvDips[0];
	}

	switch (address >> 8)
	{
		case 0xec:
			return DrvInputs[0];

		case 0xed:
			return DrvInputs[1];

		case 0xee:
			return DrvInputs[2];

		case 0xef:
			mcu_value++;
			return mcu_value & 0xf;
	}

	return 0;
}

static void __fastcall kangaroo_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xc000) == 0x8000) {
		videoram_write(address & 0x3fff, data, video_control[8]);
		return;
	}

	if ((address & 0xfc00) == 0xe800) {
		address &= 0xf; // 0-a
		video_control[address] = data;

		switch (address)
		{
			case 5:
				blitter_run();
			return;

			case 8:
				bankswitch((data & 5) ? 0 : 1);
			return;
		}

		return;
	}

	switch (address >> 8)
	{
		case 0xec:
			soundlatch = data;
		return;

		case 0xed:
			// coin counter
		return;
	}
}

static void __fastcall kangaroo_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x4000) {
		DrvZ80RAM1[address & 0x3ff] = data;
		return;
	}

	switch (address & ~0xfff)
	{
		case 0x7000:
			AY8910Write(0, 1, data);
		return;

		case 0x8000:
			AY8910Write(0, 0, data);
		return;
	}
}

static UINT8 __fastcall kangaroo_sound_read(UINT16 address)
{
	if (address < 0x1000) {
		return DrvZ80ROM1[address];
	}

	if ((address & 0xf000) == 0x4000) {
		return DrvZ80RAM1[address & 0x3ff];
	}
	
	switch (address & ~0xfff)
	{
		case 0x6000:
			return soundlatch;
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	bankswitch(0);
	ZetReset();
	ZetNmi(); // needs this to start correctly
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	AY8910Reset(0);

	HiscoreReset();

	soundlatch   = 0;
	mcu_value = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x006000;
	DrvZ80ROM1		= Next; Next += 0x001000;

	DrvGfxROM		= Next; Next += 0x006000;

	DrvPalette		= (UINT32*)Next; Next += 0x0008 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000400;
	DrvZ80RAM1		= Next; Next += 0x000400;
	DrvVidRAM		= (UINT32*)Next; Next += 256 * 64 * sizeof(UINT32);

	video_control		= Next; Next += 0x000010;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}
		
static INT32 DrvInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (game == 0) // Funky Fish
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x1000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x3000,  3, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  4, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM  + 0x0000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x1000,  6, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x2000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x3000,  8, 1)) return 1;
		}
		else if (game == 1) // Kangaroo
		{
			if (BurnLoadRom(DrvZ80ROM0 + 0x0000,  0, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x1000,  1, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x2000,  2, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x3000,  3, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x4000,  4, 1)) return 1;
			if (BurnLoadRom(DrvZ80ROM0 + 0x5000,  5, 1)) return 1;

			if (BurnLoadRom(DrvZ80ROM1 + 0x0000,  6, 1)) return 1;

			if (BurnLoadRom(DrvGfxROM  + 0x0000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x1000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x2000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM  + 0x3000, 10, 1)) return 1;
		}
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM0,	0xe000, 0xe3ff, MAP_RAM);
	ZetSetWriteHandler(kangaroo_main_write);
	ZetSetReadHandler(kangaroo_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x0fff, MAP_ROM);
	for (INT32 i = 0; i < 0x1000; i+= 0x400) {
		ZetMapMemory(DrvZ80RAM1,	0x4000 + i, 0x43ff + i, MAP_RAM);
	}
	ZetSetWriteHandler(kangaroo_sound_write);
	ZetSetReadHandler(kangaroo_sound_read);
	ZetSetOutHandler(kangaroo_sound_write);
	ZetSetInHandler(kangaroo_sound_read);
	ZetClose();

	AY8910Init(0, 1250000, 0);
	AY8910SetAllRoutes(0, 0.30, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	AY8910Exit(0);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 8; i++)
	{
		DrvPalette[i] = BurnHighCol((i&4)?0xff:0, (i&2)?0xff:0, (i&1)?0xff:0, 0);
	}
}

static void draw_screen()
{
	UINT8 scrolly = video_control[6];
	UINT8 scrollx = video_control[7];
	UINT8 maska  = (video_control[10] & 0x28) >> 3;
	UINT8 maskb   = video_control[10] & 0x07;
	UINT8 xora   = (video_control[9] & 0x20) ? 0xff : 0x00;
	UINT8 xorb   = (video_control[9] & 0x10) ? 0xff : 0x00;
	UINT8 enaa   = (video_control[9] & 0x08);
	UINT8 enab   = (video_control[9] & 0x04);
	UINT8 pria  = (~video_control[9] & 0x02);
	UINT8 prib  = (~video_control[9] & 0x01);

	for (INT32 y = 8; y < 248; y++)
	{
		UINT16 *dest = pTransDraw + (y - 8) * nScreenWidth;

		for (INT32 x = 0; x < 512; x += 2)
		{
			UINT8 effxa = scrollx + ((x / 2) ^ xora);
			UINT8 effya = scrolly + (y ^ xora);
			UINT8 effxb = (x / 2) ^ xorb;
			UINT8 effyb = y ^ xorb;
			UINT8 pixa = (DrvVidRAM[effya + 256 * (effxa / 4)] >> (8 * (effxa & 3) + 0)) & 0x0f;
			UINT8 pixb = (DrvVidRAM[effyb + 256 * (effxb / 4)] >> (8 * (effxb & 3) + 4)) & 0x0f;
			UINT8 finalpens;

			finalpens = 0;
			if (enaa && (pria || pixb == 0)) finalpens |= pixa;
			if (enab && (prib || pixa == 0)) finalpens |= pixb;

			dest[x + 0] = finalpens & 7;

			finalpens = 0;
			if (enaa && (pria || pixb == 0))
			{
				if (!(pixa & 0x08)) pixa &= maska;
				finalpens |= pixa;
			}
			if (enab && (prib || pixa == 0))
			{
				if (!(pixb & 0x08)) pixb &= maskb;
				finalpens |= pixb;
			}

			dest[x + 1] = finalpens & 7;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_screen();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = DrvDips[1];
		DrvInputs[1] = 0;
		DrvInputs[2] = 0;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal[2] = { 2500000 / 60, 1250000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if (i == 31) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		if (i == 31) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(pSoundBuf, nSegmentLength);
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

		SCAN_VAR(soundlatch);
		SCAN_VAR(nDrvBank);
		SCAN_VAR(mcu_value);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(nDrvBank);
		ZetClose();
	}

	return 0;
}


// Funky Fish

static struct BurnRomInfo fnkyfishRomDesc[] = {
	{ "tvg_64.0",		0x1000, 0xaf728803, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg_65.1",		0x1000, 0x71959e6b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg_66.2",		0x1000, 0x5ccf68d4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tvg_67.3",		0x1000, 0x938ff36f, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "tvg_68.8",		0x1000, 0xd36bb2be, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 #1 Code

	{ "tvg_69.v0",		0x1000, 0xcd532d0b, 3 | BRF_GRA },           //  5 Graphics
	{ "tvg_71.v2",		0x1000, 0xa59c9713, 3 | BRF_GRA },           //  6
	{ "tvg_70.v1",		0x1000, 0xfd308ef1, 3 | BRF_GRA },           //  7
	{ "tvg_72.v3",		0x1000, 0x6ae9b584, 3 | BRF_GRA },           //  8
};

STD_ROM_PICK(fnkyfish)
STD_ROM_FN(fnkyfish)

static INT32 fnkyfishInit()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvFnkyfish = {
	"fnkyfish", NULL, NULL, NULL, "1981",
	"Funky Fish\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_HORSHOOT, 0,
	NULL, fnkyfishRomInfo, fnkyfishRomName, NULL, NULL, NULL, NULL, FnkyfishInputInfo, FnkyfishDIPInfo,
	fnkyfishInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	240, 512, 3, 4
};


// Kangaroo

static struct BurnRomInfo kangarooRomDesc[] = {
	{ "tvg_75.0",		0x1000, 0x0d18c581, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "tvg_76.1",		0x1000, 0x5978d37a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "tvg_77.2",		0x1000, 0x522d1097, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "tvg_78.3",		0x1000, 0x063da970, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "tvg_79.4",		0x1000, 0x9e5cf8ca, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "tvg_80.5",		0x1000, 0x2fc18049, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "tvg_81.8",		0x1000, 0xfb449bfd, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code

	{ "tvg_83.v0",		0x1000, 0xc0446ca6, 3 | BRF_GRA },           //  7 Graphics
	{ "tvg_85.v2",		0x1000, 0x72c52695, 3 | BRF_GRA },           //  8
	{ "tvg_84.v1",		0x1000, 0xe4cb26c2, 3 | BRF_GRA },           //  9
	{ "tvg_86.v3",		0x1000, 0x9e6a599f, 3 | BRF_GRA },           // 10

	{ "mb8841.ic29",	0x0800, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 11 MCU Internal ROM
	{ "tvg_82.12",		0x0800, 0x57766f69, 4 | BRF_PRG | BRF_ESS }, // 12 MCU External ROM
};

STD_ROM_PICK(kangaroo)
STD_ROM_FN(kangaroo)

static INT32 kangarooInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvKangaroo = {
	"kangaroo", NULL, NULL, NULL, "1982",
	"Kangaroo\0", NULL, "Sun Electronics", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, kangarooRomInfo, kangarooRomName, NULL, NULL, NULL, NULL, KangarooInputInfo, KangarooDIPInfo,
	kangarooInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	240, 512, 3, 4
};


// Kangaroo (Atari)

static struct BurnRomInfo kangarooaRomDesc[] = {
	{ "136008-101.ic7",	0x1000, 0x0d18c581, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "136008-102.ic8",	0x1000, 0x5978d37a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136008-103.ic9",	0x1000, 0x522d1097, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136008-104.ic10",	0x1000, 0x063da970, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136008-105.ic16",	0x1000, 0x82a26c7d, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136008-106.ic17",	0x1000, 0x3dead542, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136008-107.ic24",	0x1000, 0xfb449bfd, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code

	{ "136008-108.ic76",	0x1000, 0xc0446ca6, 3 | BRF_GRA },           //  7 Graphics
	{ "136008-110.ic77",	0x1000, 0x72c52695, 3 | BRF_GRA },           //  8
	{ "136008-109.ic52",	0x1000, 0xe4cb26c2, 3 | BRF_GRA },           //  9
	{ "136008-111.ic53",	0x1000, 0x9e6a599f, 3 | BRF_GRA },           // 10

	{ "mb8841.ic29",	0x0800, 0x00000000, 4 | BRF_NODUMP | BRF_OPT }, // 11 MCU Internal ROM
	{ "136008-112.ic28",	0x0800, 0x57766f69, 4 | BRF_PRG | BRF_ESS }, // 12 MCU External ROM
};

STD_ROM_PICK(kangarooa)
STD_ROM_FN(kangarooa)

struct BurnDriver BurnDrvKangarooa = {
	"kangarooa", "kangaroo", NULL, NULL, "1982",
	"Kangaroo (Atari)\0", NULL, "Sun Electronics (Atari license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, kangarooaRomInfo, kangarooaRomName, NULL, NULL, NULL, NULL, KangarooInputInfo, KangarooDIPInfo,
	kangarooInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	240, 512, 3, 4
};


// Kangaroo (bootleg)

static struct BurnRomInfo kangaroobRomDesc[] = {
	{ "k1.ic7",		0x1000, 0x0d18c581, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "k2.ic8",		0x1000, 0x5978d37a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "k3.ic9",		0x1000, 0x522d1097, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "k4.ic10",		0x1000, 0x063da970, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "k5.ic16",		0x1000, 0x9e5cf8ca, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "k6.ic17",		0x1000, 0x7644504a, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "k7.ic24",		0x1000, 0xfb449bfd, 2 | BRF_PRG | BRF_ESS }, //  6 Z80 #1 Code

	{ "k10.ic76",		0x1000, 0xc0446ca6, 3 | BRF_GRA },           //  7 Graphics
	{ "k11.ic77",		0x1000, 0x72c52695, 3 | BRF_GRA },           //  8
	{ "k8.ic52",		0x1000, 0xe4cb26c2, 3 | BRF_GRA },           //  9
	{ "k9.ic53",		0x1000, 0x9e6a599f, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(kangaroob)
STD_ROM_FN(kangaroob)

struct BurnDriver BurnDrvKangaroob = {
	"kangaroob", "kangaroo", NULL, NULL, "1982",
	"Kangaroo (bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM | GBF_ACTION, 0,
	NULL, kangaroobRomInfo, kangaroobRomName, NULL, NULL, NULL, NULL, KangarooInputInfo, KangarooDIPInfo,
	kangarooInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 8,
	240, 512, 3, 4
};
