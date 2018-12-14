// FB Alpha Rock'n Rope driver module
// Based on MAME driver by Chris Hardy

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "timeplt_snd.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvM6809ROMDec;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvColPROM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 irq_enable;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog;

static struct BurnInputInfo RocnropeInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Rocnrope)

static struct BurnDIPInfo RocnropeDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0x5b, NULL			},
	{0x14, 0xff, 0xff, 0x96, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x12, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x12, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x12, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x12, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x12, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x12, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x12, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x12, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x12, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x12, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x12, 0x01, 0xf0, 0x00, "No Coin B"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x13, 0x01, 0x03, 0x03, "3"			},
	{0x13, 0x01, 0x03, 0x02, "4"			},
	{0x13, 0x01, 0x03, 0x01, "5"			},
	{0x13, 0x01, 0x03, 0x00, "255 (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x00, "Upright"		},
	{0x13, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    16, "Difficulty"		},
	{0x13, 0x01, 0x78, 0x78, "1 (Easy)"		},
	{0x13, 0x01, 0x78, 0x70, "2"			},
	{0x13, 0x01, 0x78, 0x68, "3"			},
	{0x13, 0x01, 0x78, 0x60, "4"			},
	{0x13, 0x01, 0x78, 0x58, "5"			},
	{0x13, 0x01, 0x78, 0x50, "6"			},
	{0x13, 0x01, 0x78, 0x48, "7"			},
	{0x13, 0x01, 0x78, 0x40, "8"			},
	{0x13, 0x01, 0x78, 0x38, "9"			},
	{0x13, 0x01, 0x78, 0x30, "10"			},
	{0x13, 0x01, 0x78, 0x28, "11"			},
	{0x13, 0x01, 0x78, 0x20, "12"			},
	{0x13, 0x01, 0x78, 0x18, "13"			},
	{0x13, 0x01, 0x78, 0x10, "14"			},
	{0x13, 0x01, 0x78, 0x08, "15"			},
	{0x13, 0x01, 0x78, 0x00, "16 (Difficult)"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x80, 0x80, "Off"			},
	{0x13, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    7, "First Bonus"		},
	{0x14, 0x01, 0x07, 0x06, "20000"		},
	{0x14, 0x01, 0x07, 0x05, "30000"		},
	{0x14, 0x01, 0x07, 0x04, "40000"		},
	{0x14, 0x01, 0x07, 0x03, "50000"		},
	{0x14, 0x01, 0x07, 0x02, "60000"		},
	{0x14, 0x01, 0x07, 0x01, "70000"		},
	{0x14, 0x01, 0x07, 0x00, "80000"		},

	{0   , 0xfe, 0   ,    5, "Repeated Bonus"	},
	{0x14, 0x01, 0x38, 0x20, "40000"		},
	{0x14, 0x01, 0x38, 0x18, "50000"		},
	{0x14, 0x01, 0x38, 0x10, "60000"		},
	{0x14, 0x01, 0x38, 0x08, "70000"		},
	{0x14, 0x01, 0x38, 0x00, "80000"		},

	{0   , 0xfe, 0   ,    2, "Grant Repeated Bonus"	},
	{0x14, 0x01, 0x40, 0x40, "No"			},
	{0x14, 0x01, 0x40, 0x00, "Yes"			},
};

STDDIPINFO(Rocnrope)

static UINT8 rocnrope_read(UINT16 address)
{
	switch (address)
	{
		case 0x3080:
			return DrvInputs[0];

		case 0x3081:
			return DrvInputs[1];

		case 0x3082:
			return DrvInputs[2];

		case 0x3083:
			return DrvDips[0];

		case 0x3000:
			return DrvDips[1];

		case 0x3100:
			return DrvDips[2];
	}

	return 0;
}

static void rocnrope_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
			watchdog = 0;
		return;

		case 0x8080:
	//		flipscreen = ~data & 0x01;
		return;

		case 0x8081:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x8082:
		case 0x8083:
		case 0x8084:
		return;

		case 0x8087:
			irq_enable = data & 0x01;
			if (!irq_enable) {
				M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0x8100:
			TimepltSndSoundlatch(data);
		return;

		case 0x8182:	// write irq vectors!
		case 0x8183:
		case 0x8184:
		case 0x8185:
		case 0x8186:
		case 0x8187:
		case 0x8188:
		case 0x8189:
		case 0x818a:
		case 0x818b:
		case 0x818c:
		case 0x818d:
			DrvM6809ROM[0xfff0 + (address & 0xf)] = data;
		return;
	}
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	TimepltSndReset();

	irq_enable = 0;
	watchdog = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x010000;
	DrvM6809ROMDec		= Next; Next += 0x010000;
	DrvZ80ROM		= Next; Next += 0x003000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x008000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x001010;
	DrvSprRAM		= Next; Next += 0x000800;
	DrvColRAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvZ80RAM		= Next; Next += 0x000400;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { 0x10004, 0x10000, 4, 0 };
	INT32 Plane1[4] = { 0x20004, 0x20000, 4, 0 };
	INT32 XOffs[16] = { STEP4(0,1), STEP4(64,1), STEP4(128,1), STEP4(192,1) };
	INT32 YOffs[16] = { STEP8(0,8), STEP8(256,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x8000);

	GfxDecode(0x100, 4, 16, 16, Plane1, XOffs, YOffs, 0x200, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x4000);

	GfxDecode(0x200, 4,  8,  8, Plane0, XOffs, YOffs, 0x080, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static void M6809Decode()
{
	for (INT32 i = 0x6000; i < 0x10000; i++) {
		DrvM6809ROMDec[i] = DrvM6809ROM[i] ^ (((i&2)?0x80:0x20)|((i&8)?0x08:0x02));
	}
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
		if (BurnLoadRom(DrvM6809ROM + 0x06000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0a000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0c000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x0e000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x01000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x04000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x06000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x02000, 12, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 13, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 14, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00120, 15, 1)) return 1;

		M6809Decode();
		DrvGfxDecode();

		DrvM6809ROMDec[0x703d] = 0x98; // Patch bad opcode

		// allow us to use Color Prom data as transparency table for sprites
		for (INT32 i = 0; i < 0x200; i++) {
			DrvColPROM[0x20 + i] &= 0xf;
		}
	}

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvSprRAM,		0x4000, 0x47ff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x4800, 0x4bff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x4c00, 0x4fff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM,		0x5000, 0x5fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM + 0x6000,	0x6000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809ROMDec + 0x6000,	0x6000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(rocnrope_write);
	M6809SetReadHandler(rocnrope_read);
	M6809Close();

	TimepltSndInit(DrvZ80ROM, DrvZ80RAM, 0);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();

	TimepltSndExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[16];

	for (INT32 i = 0; i < 16; i++)
	{
		INT32 bit0 = (DrvColPROM[i] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i] >> 2) & 0x01;
		INT32 r = bit2 * 151 + bit1 * 71 + bit0 * 33;

		bit0 = (DrvColPROM[i] >> 3) & 0x01;
		bit1 = (DrvColPROM[i] >> 4) & 0x01;
		bit2 = (DrvColPROM[i] >> 5) & 0x01;
		INT32 g = bit2 * 151 + bit1 * 71 + bit0 * 33;

		bit0 = (DrvColPROM[i] >> 6) & 0x01;
		bit1 = (DrvColPROM[i] >> 7) & 0x01;
		INT32 b = bit1 * 174 + bit0 * 81;

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x200; i++)
	{
		DrvPalette[i] = pens[DrvColPROM[0x20 + i]];
	}
}

static void draw_layer()
{
	for (INT32 offs = (32 * 2); offs < (32 * 32) - (2 * 32); offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 attr  = DrvColRAM[offs];
		INT32 code  = DrvVidRAM[offs] + ((attr & 0x80) << 1);
		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x40;
		INT32 flipy = attr & 0x20;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile_FlipY(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			} else {
				Render8x8Tile(pTransDraw, code, sx, sy - 16, color, 4, 0x100, DrvGfxROM1);
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x30 - 2; offs >= 0; offs -= 2)
	{
		INT32 attr  = DrvSprRAM[0x000 + offs + 0];
		INT32 code  = DrvSprRAM[0x400 + offs + 1];
		INT32 sx    = DrvSprRAM[0x400 + offs + 0];
		INT32 sy    = DrvSprRAM[0x000 + offs + 1];
		INT32 color = attr & 0x0f;
		INT32 flipx = attr & 0x40;
		INT32 flipy =~attr & 0x80;

		RenderTileTranstab(pTransDraw, DrvGfxROM0, code, color * 16, 0, 240 - sx, sy - 16, flipx, flipy, 16, 16, DrvColPROM + 0x20);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer();
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

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		// Clear opposites
		if ((DrvInputs[1] & 0x0c) == 0) DrvInputs[1] |= 0x0c;
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[2] & 0x0c) == 0) DrvInputs[2] |= 0x0c;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 1536000 / 60, 1789772 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		INT32 nSegment = (nCyclesTotal[0] * (i + 1)) / nInterleave;
		nCyclesDone[0] += M6809Run(nSegment - nCyclesDone[0]);
		if (i == (nInterleave - 1) && irq_enable) M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);

		nSegment = (nCyclesTotal[1] * i) / nInterleave;
		nCyclesDone[1] += ZetRun(nSegment - nCyclesDone[1]);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			TimepltSndUpdate(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	ZetClose();
	M6809Close();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		TimepltSndUpdate(pSoundBuf, nSegmentLength);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029521;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		ZetScan(nAction);

		TimepltSndScan(nAction, pnMin);

		SCAN_VAR(irq_enable);
	}

	// Read and write irq vectors
	if (nAction & ACB_READ) {
		memcpy (DrvM6809RAM + 0x1000, DrvM6809ROM + 0xfff2, 0x0c);
	}

	if (nAction & ACB_WRITE) {
		memcpy (DrvM6809ROM + 0xfff2, DrvM6809RAM + 0x1000, 0x0c);
	}

	return 0;
}


// Roc'n Rope

static struct BurnRomInfo rocnropeRomDesc[] = {
	{ "rr1.1h",		0x2000, 0x83093134, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "rr2.2h",		0x2000, 0x75af8697, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rr3.3h",		0x2000, 0xb21372b1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rr4.4h",		0x2000, 0x7acb2a05, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rnr_h5.vid",		0x2000, 0x150a6264, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "rnr_7a.snd",		0x1000, 0x75d2c4e2, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "rnr_8a.snd",		0x1000, 0xca4325ae, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "rnr_a11.vid",	0x2000, 0xafdaba5e, 3 | BRF_GRA },           //  7 Sprites
	{ "rnr_a12.vid",	0x2000, 0x054cafeb, 3 | BRF_GRA },           //  8
	{ "rnr_a9.vid",		0x2000, 0x9d2166b2, 3 | BRF_GRA },           //  9
	{ "rnr_a10.vid",	0x2000, 0xaff6e22f, 3 | BRF_GRA },           // 10

	{ "rnr_h12.vid",	0x2000, 0xe2114539, 4 | BRF_GRA },           // 11 Characters
	{ "rnr_h11.vid",	0x2000, 0x169a8f3f, 4 | BRF_GRA },           // 12

	{ "a17_prom.bin",	0x0020, 0x22ad2c3e, 5 | BRF_GRA },           // 13 Color PROMs
	{ "b16_prom.bin",	0x0100, 0x750a9677, 5 | BRF_GRA },           // 14
	{ "rocnrope.pr3",	0x0100, 0xb5c75a27, 5 | BRF_GRA },           // 15

	{ "h100.6g",		0x0001, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 16 PALs
};

STD_ROM_PICK(rocnrope)
STD_ROM_FN(rocnrope)

struct BurnDriver BurnDrvRocnrope = {
	"rocnrope", NULL, NULL, NULL, "1983",
	"Roc'n Rope\0", NULL, "Konami", "GX364",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, rocnropeRomInfo, rocnropeRomName, NULL, NULL, NULL, NULL, RocnropeInputInfo, RocnropeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Roc'n Rope (Kosuka)

static struct BurnRomInfo rocnropekRomDesc[] = {
	{ "rnr_h1.vid",		0x2000, 0x0fddc1f6, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "rnr_h2.vid",		0x2000, 0xce9db49a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rnr_h3.vid",		0x2000, 0x6d278459, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rnr_h4.vid",		0x2000, 0x9b2e5f2a, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "rnr_h5.vid",		0x2000, 0x150a6264, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "rnr_7a.snd",		0x1000, 0x75d2c4e2, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "rnr_8a.snd",		0x1000, 0xca4325ae, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "rnr_a11.vid",	0x2000, 0xafdaba5e, 3 | BRF_GRA },           //  7 Sprites
	{ "rnr_a12.vid",	0x2000, 0x054cafeb, 3 | BRF_GRA },           //  8
	{ "rnr_a9.vid",		0x2000, 0x9d2166b2, 3 | BRF_GRA },           //  9
	{ "rnr_a10.vid",	0x2000, 0xaff6e22f, 3 | BRF_GRA },           // 10

	{ "rnr_h12.vid",	0x2000, 0xe2114539, 4 | BRF_GRA },           // 11 Characters
	{ "rnr_h11.vid",	0x2000, 0x169a8f3f, 4 | BRF_GRA },           // 12

	{ "a17_prom.bin",	0x0020, 0x22ad2c3e, 5 | BRF_GRA },           // 13 Color PROMs
	{ "b16_prom.bin",	0x0100, 0x750a9677, 5 | BRF_GRA },           // 14
	{ "rocnrope.pr3",	0x0100, 0xb5c75a27, 5 | BRF_GRA },           // 15

	{ "h100.6g",		0x0001, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 16 PALs
};

STD_ROM_PICK(rocnropek)
STD_ROM_FN(rocnropek)

struct BurnDriver BurnDrvRocnropek = {
	"rocnropek", "rocnrope", NULL, NULL, "1983",
	"Roc'n Rope (Kosuka)\0", NULL, "Konami / Kosuka", "GX364",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, rocnropekRomInfo, rocnropekRomName, NULL, NULL, NULL, NULL, RocnropeInputInfo, RocnropeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Ropeman (bootleg of Roc'n Rope)

static struct BurnRomInfo ropemanRomDesc[] = {
	{ "r1.1j",		0x2000, 0x6310a1fe, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code
	{ "r2.2j",		0x2000, 0x75af8697, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "r3.3j",		0x2000, 0xb21372b1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "r4.4j",		0x2000, 0x7acb2a05, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "r5.5j",		0x2000, 0x150a6264, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "r12.7a",		0x1000, 0x75d2c4e2, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "r13.8a",		0x1000, 0xca4325ae, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "r10.11a",		0x2000, 0xafdaba5e, 3 | BRF_GRA },           //  7 Sprites
	{ "r11.12a",		0x2000, 0x054cafeb, 3 | BRF_GRA },           //  8
	{ "r8.9a",		0x2000, 0x9d2166b2, 3 | BRF_GRA },           //  9
	{ "r9.10a",		0x2000, 0xaff6e22f, 3 | BRF_GRA },           // 10

	{ "r7.12j",		0x2000, 0xcd8ac4bf, 4 | BRF_GRA },           // 11 Characters
	{ "r6.11j",		0x2000, 0x35891835, 4 | BRF_GRA },           // 12

	{ "1.17a",		0x0020, 0x22ad2c3e, 5 | BRF_GRA },           // 13 Color PROMs
	{ "2.16b",		0x0100, 0x750a9677, 5 | BRF_GRA },           // 14
	{ "3.16g",		0x0100, 0xb5c75a27, 5 | BRF_GRA },           // 15

	{ "pal10l8.6g",		0x0001, 0x00000000, 6 | BRF_OPT | BRF_NODUMP }, // 16 PALs
	{ "n82s153.pal1.bin",	0x00eb, 0xbaebe804, 6 | BRF_OPT },           // 17
	{ "n82s153.pal2.bin",	0x00eb, 0xa0e1b7a0, 6 | BRF_OPT },           // 18
};

STD_ROM_PICK(ropeman)
STD_ROM_FN(ropeman)

struct BurnDriver BurnDrvRopeman = {
	"ropeman", "rocnrope", NULL, NULL, "1983",
	"Ropeman (bootleg of Roc'n Rope)\0", NULL, "bootleg", "GX364",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, ropemanRomInfo, ropemanRomName, NULL, NULL, NULL, NULL, RocnropeInputInfo, RocnropeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
