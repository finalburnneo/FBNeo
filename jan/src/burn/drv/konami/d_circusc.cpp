// FB Alpha Circus Charlie driver module
// Based on MAME driver by Chris Hardy

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "dac.h"

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
static UINT8 *DrvTransTable;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;

static UINT32 *DrvPalette;
static UINT8  DrvRecalc;

static UINT8 snlatch;
static UINT8 soundlatch;
static UINT8 irqmask;
static UINT8 spritebank;
static UINT8 scrolldata;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog;

static struct BurnInputInfo CircuscInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Circusc)

static struct BurnDIPInfo CircuscDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL			},
	{0x0d, 0xff, 0xff, 0x4b, NULL			},

	{0   , 0xfe, 0   ,    16, "Coin A"		},
	{0x0c, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x0c, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x0c, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x0c, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x0c, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x0c, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    16, "Coin B"		},
	{0x0c, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x0c, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x0c, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x0c, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x0c, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x0c, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x0c, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x0c, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x0c, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x0c, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x0c, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x0c, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x0c, 0x01, 0xf0, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x03, 0x03, "3"			},
	{0x0d, 0x01, 0x03, 0x02, "4"			},
	{0x0d, 0x01, 0x03, 0x01, "5"			},
	{0x0d, 0x01, 0x03, 0x00, "7"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x0d, 0x01, 0x04, 0x00, "Upright"		},
	{0x0d, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x0d, 0x01, 0x08, 0x08, "20k 90k 70k+"		},
	{0x0d, 0x01, 0x08, 0x00, "30k 110k 80k+"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0d, 0x01, 0x60, 0x60, "Easy"			},
	{0x0d, 0x01, 0x60, 0x40, "Normal"		},
	{0x0d, 0x01, 0x60, 0x20, "Hard"			},
	{0x0d, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x80, 0x80, "Off"			},
	{0x0d, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Circusc)

static void circusc_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x0000:
	//		flipscreen = data & 0x01;
		return;

		case 0x0001:
			irqmask = data & 0x01;
		return;

		case 0x0003:
		case 0x0004:
	//		coin counter
		return;

		case 0x0005:
			spritebank = data & 0x01;
		return;

		case 0x0400:
			watchdog = 0;
		return;

		case 0x0800:
			soundlatch = data;
		return;

		case 0x0c00:
			ZetSetVector(0xff);
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x1c00:
			scrolldata = data;
		return;
	}
}

static UINT8 circusc_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x1000:
			return DrvInputs[0];

		case 0x1001:
			return DrvInputs[1];

		case 0x1002:
			return DrvInputs[2];

		case 0x1003:
			return 0;

		case 0x1400:
			return DrvDips[0];

		case 0x1800:
			return DrvDips[1];
	}

	return 0;
}

static void __fastcall circusc_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			snlatch = data;
		return;

		case 0xa001:
			SN76496Write(0, snlatch);
		return;

		case 0xa002:
			SN76496Write(1, snlatch);
		return;

		case 0xa003:
			DACWrite(0, data);
		return;

		case 0xa004:
		case 0xa07c:
			// discrete
		return;
	}
}

static UINT8 __fastcall circusc_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x6000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0x8000:
			return (ZetTotalCycles() / 0x200) & 0x1e;
	}

	return 0;
}

static INT32 DrvDACSync()
{
	return (INT32)(float)(nBurnSoundLen * (ZetTotalCycles() / (3579545.0000 / (nBurnFPS / 100.0000))));
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6809Open(0);
	M6809Reset();
	M6809Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	DACReset();

	watchdog = 0;
	soundlatch = 0;
	irqmask = 0;
	spritebank = 0;
	scrolldata = 0;
	snlatch = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6809ROM		= Next; Next += 0x00a000;
	DrvM6809ROMDec		= Next; Next += 0x00a000;
	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x018000;
	DrvGfxROM1		= Next; Next += 0x028000;

	DrvColPROM		= Next; Next += 0x000220;

	DrvTransTable		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6809RAM		= Next; Next += 0x02000;
	DrvColRAM		= Next; Next += 0x00400;
	DrvVidRAM		= Next; Next += 0x00400;
	DrvSprRAM		= Next; Next += 0x00200;

	DrvZ80RAM		= Next; Next += 0x00400;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void M6809Decode()
{
	for (INT32 i = 0; i < 0xa000; i++) {
		DrvM6809ROMDec[i] = DrvM6809ROM[i] ^ (((i&2)?0x80:0x20)|((i&8)?0x08:0x02));
	}
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
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

		if (BurnLoadRom(DrvM6809ROM + 0x00000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x02000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x04000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x06000,  3, 1)) return 1;
		if (BurnLoadRom(DrvM6809ROM + 0x08000,  4, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM   + 0x00000,  5, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM   + 0x02000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0  + 0x00000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0  + 0x02000,  8, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1  + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x02000, 10, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x04000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x06000, 12, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x08000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1  + 0x0a000, 14, 1)) return 1;

		if (BurnLoadRom(DrvColPROM  + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00020, 16, 1)) return 1;
		if (BurnLoadRom(DrvColPROM  + 0x00120, 17, 1)) return 1;

		M6809Decode();
		DrvGfxExpand(DrvGfxROM0, 0x4000);
		DrvGfxExpand(DrvGfxROM1, 0xc000);
	}

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,		0x2000, 0x2fff, MAP_RAM);
	M6809MapMemory(DrvColRAM,		0x3000, 0x33ff, MAP_RAM);
	M6809MapMemory(DrvVidRAM,		0x3400, 0x37ff, MAP_RAM);
	M6809MapMemory(DrvSprRAM,		0x3800, 0x39ff, MAP_RAM);
	M6809MapMemory(DrvM6809RAM + 0x1a00,	0x3a00, 0x3fff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM,		0x6000, 0xffff, MAP_READ);
	M6809MapMemory(DrvM6809ROMDec,		0x6000, 0xffff, MAP_FETCH);
	M6809SetWriteHandler(circusc_main_write);
	M6809SetReadHandler(circusc_main_read);
	M6809Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x3fff, MAP_ROM);
	for (INT32 i = 0; i < 0x2000; i+=0x400) {
		ZetMapMemory(DrvZ80RAM,		0x4000+i, 0x43ff+i, MAP_RAM);
	}
	ZetSetWriteHandler(circusc_sound_write);
	ZetSetReadHandler(circusc_sound_read);
	ZetClose();

	SN76496Init(0, 1789772, 0);
	SN76496Init(1, 1789772, 1);
	SN76496SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvDACSync);
	DACSetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	ZetExit();

	SN76496Exit();
	DACExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[32];

	for (INT32 i = 0; i < 32; i++)
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

	for (INT32 i = 0; i < 0x200; i++) {
		DrvPalette[i] = pens[(DrvColPROM[i+0x20] & 0xf) + ((~i & 0x100)>>4)];
		DrvTransTable[i] = (DrvPalette[i] ? 0xff : 0);
	}
}

static void draw_layer(INT32 priority)
{
	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		if (sx >= 80) sy -= scrolldata;
		if (sy < -7) sy += 256;

		sy -= 16;
		if (sy < -7 || sy >= nScreenHeight) continue;

		INT32 attr  = DrvColRAM[offs];
		INT32 prio  = attr & 0x10; 
		if (priority != prio) continue;

		INT32 code  = DrvVidRAM[offs] + ((attr & 0x20) * 8);
		INT32 color = attr & 0x0f;
		INT32 flipy = attr & 0x80;
		INT32 flipx = attr & 0x40;

		if (flipy) {
			if (flipx) {
				Render8x8Tile_FlipXY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_FlipY_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		} else {
			if (flipx) {
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			} else {
				Render8x8Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM0);
			}
		}
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvSprRAM + (spritebank * 0x100);

	for (INT32 offs = 0; offs < 0x100; offs += 4)
	{
		INT32 attr   = spriteram[offs + 1];
		INT32 code   = spriteram[offs + 0] + ((attr & 0x20) * 8);
		INT32 sx     = spriteram[offs + 2];
		INT32 sy     = spriteram[offs + 3];
		INT32 color  = attr & 0x0f;
		INT32 flipx  = attr & 0x40;
		INT32 flipy  = attr & 0x80;

		RenderTileTranstab(pTransDraw, DrvGfxROM1, code, color*16+0x100, 0, sx, sy - 16, flipx, flipy, 16, 16, DrvTransTable);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	draw_layer(0x10);
	draw_sprites();
	draw_layer(0x00);

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
		if ((DrvInputs[1] & 0x03) == 0) DrvInputs[1] |= 0x03;
		if ((DrvInputs[2] & 0x03) == 0) DrvInputs[2] |= 0x03;
	}

	INT32 nInterleave = 32;
	INT32 nCyclesTotal[2] = { 2048000 / 60, 3579545 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	M6809Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += M6809Run(nCyclesTotal[0] / nInterleave);

		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (irqmask) M6809SetIRQLine(0, CPU_IRQSTATUS_AUTO);

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			SN76496Update(0, pSoundBuf, nSegmentLength);
			SN76496Update(1, pSoundBuf, nSegmentLength);
		}
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	M6809Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029705;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6809Scan(nAction);
		ZetScan(nAction);

		DACScan(nAction, pnMin);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(snlatch);
		SCAN_VAR(soundlatch);
		SCAN_VAR(irqmask);
		SCAN_VAR(spritebank);
		SCAN_VAR(scrolldata);
	}

	return 0;
}


// Circus Charlie (level select, set 1)

static struct BurnRomInfo circuscRomDesc[] = {
	{ "380_s05.3h",		0x2000, 0x48feafcf, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Encrypted)
	{ "380_q04.4h",		0x2000, 0xc283b887, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "380_q03.5h",		0x2000, 0xe90c0e86, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "380_q02.6h",		0x2000, 0x4d847dc6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "380_q01.7h",		0x2000, 0x18c20adf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "380_l14.5c",		0x2000, 0x607df0fb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "380_l15.7c",		0x2000, 0xa6ad30e1, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "380_j12.4a",		0x2000, 0x56e5b408, 3 | BRF_GRA },           //  7 Tiles
	{ "380_j13.5a",		0x2000, 0x5aca0193, 3 | BRF_GRA },           //  8

	{ "380_j06.11e",	0x2000, 0xdf0405c6, 4 | BRF_GRA },           //  9 Sprites
	{ "380_j07.12e",	0x2000, 0x23dfe3a6, 4 | BRF_GRA },           // 10
	{ "380_j08.13e",	0x2000, 0x3ba95390, 4 | BRF_GRA },           // 11
	{ "380_j09.14e",	0x2000, 0xa9fba85a, 4 | BRF_GRA },           // 12
	{ "380_j10.15e",	0x2000, 0x0532347e, 4 | BRF_GRA },           // 13
	{ "380_j11.16e",	0x2000, 0xe1725d24, 4 | BRF_GRA },           // 14

	{ "380_j18.2a",		0x0020, 0x10dd4eaa, 5 | BRF_GRA },           // 15 Color PROMs
	{ "380_j17.7b",		0x0100, 0x13989357, 5 | BRF_GRA },           // 16
	{ "380_j16.10c",	0x0100, 0xc244f2aa, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(circusc)
STD_ROM_FN(circusc)

struct BurnDriver BurnDrvCircusc = {
	"circusc", NULL, NULL, NULL, "1984",
	"Circus Charlie (level select, set 1)\0", NULL, "Konami", "GX380",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, circuscRomInfo, circuscRomName, NULL, NULL, CircuscInputInfo, CircuscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Circus Charlie (level select, set 2)

static struct BurnRomInfo circusc2RomDesc[] = {
	{ "380_w05.3h",		0x2000, 0x87df9f5e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Encrypted)
	{ "380_q04.4h",		0x2000, 0xc283b887, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "380_q03.5h",		0x2000, 0xe90c0e86, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "380_q02.6h",		0x2000, 0x4d847dc6, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "380_q01.7h",		0x2000, 0x18c20adf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "380_l14.5c",		0x2000, 0x607df0fb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "380_l15.7c",		0x2000, 0xa6ad30e1, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "380_j12.4a",		0x2000, 0x56e5b408, 3 | BRF_GRA },           //  7 Tiles
	{ "380_k13.5a",		0x2000, 0x5aca0193, 3 | BRF_GRA },           //  8

	{ "380_j06.11e",	0x2000, 0xdf0405c6, 4 | BRF_GRA },           //  9 Sprites
	{ "380_j07.12e",	0x2000, 0x23dfe3a6, 4 | BRF_GRA },           // 10
	{ "380_j08.13e",	0x2000, 0x3ba95390, 4 | BRF_GRA },           // 11
	{ "380_j09.14e",	0x2000, 0xa9fba85a, 4 | BRF_GRA },           // 12
	{ "380_j10.15e",	0x2000, 0x0532347e, 4 | BRF_GRA },           // 13
	{ "380_j11.16e",	0x2000, 0xe1725d24, 4 | BRF_GRA },           // 14

	{ "380_j18.2a",		0x0020, 0x10dd4eaa, 5 | BRF_GRA },           // 15 Color PROMs
	{ "380_j17.7b",		0x0100, 0x13989357, 5 | BRF_GRA },           // 16
	{ "380_j16.10c",	0x0100, 0xc244f2aa, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(circusc2)
STD_ROM_FN(circusc2)

struct BurnDriver BurnDrvCircusc2 = {
	"circusc2", "circusc", NULL, NULL, "1984",
	"Circus Charlie (level select, set 2)\0", NULL, "Konami", "GX380",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, circusc2RomInfo, circusc2RomName, NULL, NULL, CircuscInputInfo, CircuscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Circus Charlie (level select, set 3)

static struct BurnRomInfo circusc3RomDesc[] = {
	{ "380_w05.3h",		0x2000, 0x87df9f5e, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Encrypted)
	{ "380_r04.4h",		0x2000, 0xc283b887, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "380_r03.5h",		0x2000, 0xe90c0e86, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "380_r02.6h",		0x2000, 0x2d434c6f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "380_q01.7h",		0x2000, 0x18c20adf, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "380_l14.5c",		0x2000, 0x607df0fb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "380_l15.7c",		0x2000, 0xa6ad30e1, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "380_j12.4a",		0x2000, 0x56e5b408, 3 | BRF_GRA },           //  7 Tiles
	{ "380_j13.5a",		0x2000, 0x5aca0193, 3 | BRF_GRA },           //  8

	{ "380_j06.11e",	0x2000, 0xdf0405c6, 4 | BRF_GRA },           //  9 Sprites
	{ "380_j07.12e",	0x2000, 0x23dfe3a6, 4 | BRF_GRA },           // 10
	{ "380_j08.13e",	0x2000, 0x3ba95390, 4 | BRF_GRA },           // 11
	{ "380_j09.14e",	0x2000, 0xa9fba85a, 4 | BRF_GRA },           // 12
	{ "380_j10.15e",	0x2000, 0x0532347e, 4 | BRF_GRA },           // 13
	{ "380_j11.16e",	0x2000, 0xe1725d24, 4 | BRF_GRA },           // 14

	{ "380_j18.2a",		0x0020, 0x10dd4eaa, 5 | BRF_GRA },           // 15 Color PROMs
	{ "380_j17.7b",		0x0100, 0x13989357, 5 | BRF_GRA },           // 16
	{ "380_j16.10c",	0x0100, 0xc244f2aa, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(circusc3)
STD_ROM_FN(circusc3)

struct BurnDriver BurnDrvCircusc3 = {
	"circusc3", "circusc", NULL, NULL, "1984",
	"Circus Charlie (level select, set 3)\0", NULL, "Konami", "GX380",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, circusc3RomInfo, circusc3RomName, NULL, NULL, CircuscInputInfo, CircuscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Circus Charlie (no level select)

static struct BurnRomInfo circusc4RomDesc[] = {
	{ "380_r05.3h",		0x2000, 0xed52c60f, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Encrypted)
	{ "380_n04.4h",		0x2000, 0xfcc99e33, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "380_n03.5h",		0x2000, 0x5ef5b3b5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "380_n02.6h",		0x2000, 0xa5a5e796, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "380_n01.7h",		0x2000, 0x70d26721, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "380_l14.5c",		0x2000, 0x607df0fb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "380_l15.7c",		0x2000, 0xa6ad30e1, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "380_j12.4a",		0x2000, 0x56e5b408, 3 | BRF_GRA },           //  7 Tiles
	{ "380_j13.5a",		0x2000, 0x5aca0193, 3 | BRF_GRA },           //  8

	{ "380_j06.11e",	0x2000, 0xdf0405c6, 4 | BRF_GRA },           //  9 Sprites
	{ "380_j07.12e",	0x2000, 0x23dfe3a6, 4 | BRF_GRA },           // 10
	{ "380_j08.13e",	0x2000, 0x3ba95390, 4 | BRF_GRA },           // 11
	{ "380_j09.14e",	0x2000, 0xa9fba85a, 4 | BRF_GRA },           // 12
	{ "380_j10.15e",	0x2000, 0x0532347e, 4 | BRF_GRA },           // 13
	{ "380_j11.16e",	0x2000, 0xe1725d24, 4 | BRF_GRA },           // 14

	{ "380_j18.2a",		0x0020, 0x10dd4eaa, 5 | BRF_GRA },           // 15 Color PROMs
	{ "380_j17.7b",		0x0100, 0x13989357, 5 | BRF_GRA },           // 16
	{ "380_j16.10c",	0x0100, 0xc244f2aa, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(circusc4)
STD_ROM_FN(circusc4)

struct BurnDriver BurnDrvCircusc4 = {
	"circusc4", "circusc", NULL, NULL, "1984",
	"Circus Charlie (no level select)\0", NULL, "Konami", "GX380",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, circusc4RomInfo, circusc4RomName, NULL, NULL, CircuscInputInfo, CircuscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Circus Charlie (Centuri)

static struct BurnRomInfo circusccRomDesc[] = {
	{ "380_u05.3h",		0x2000, 0x964c035a, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Encrypted)
	{ "380_p04.4h",		0x2000, 0xdd0c0ee7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "380_p03.5h",		0x2000, 0x190247af, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "380_p02.6h",		0x2000, 0x7e63725e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "380_p01.7h",		0x2000, 0xeedaa5b2, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "380_l14.5c",		0x2000, 0x607df0fb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "380_l15.7c",		0x2000, 0xa6ad30e1, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "380_j12.4a",		0x2000, 0x56e5b408, 3 | BRF_GRA },           //  7 Tiles
	{ "380_j13.5a",		0x2000, 0x5aca0193, 3 | BRF_GRA },           //  8

	{ "380_j06.11e",	0x2000, 0xdf0405c6, 4 | BRF_GRA },           //  9 Sprites
	{ "380_j07.12e",	0x2000, 0x23dfe3a6, 4 | BRF_GRA },           // 10
	{ "380_j08.13e",	0x2000, 0x3ba95390, 4 | BRF_GRA },           // 11
	{ "380_j09.14e",	0x2000, 0xa9fba85a, 4 | BRF_GRA },           // 12
	{ "380_j10.15e",	0x2000, 0x0532347e, 4 | BRF_GRA },           // 13
	{ "380_j11.16e",	0x2000, 0xe1725d24, 4 | BRF_GRA },           // 14

	{ "380_j18.2a",		0x0020, 0x10dd4eaa, 5 | BRF_GRA },           // 15 Color PROMs
	{ "380_j17.7b",		0x0100, 0x13989357, 5 | BRF_GRA },           // 16
	{ "380_j16.10c",	0x0100, 0xc244f2aa, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(circuscc)
STD_ROM_FN(circuscc)

struct BurnDriver BurnDrvCircuscc = {
	"circuscc", "circusc", NULL, NULL, "1984",
	"Circus Charlie (Centuri)\0", NULL, "Konami (Centuri license)", "GX380",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, circusccRomInfo, circusccRomName, NULL, NULL, CircuscInputInfo, CircuscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};


// Circus Charlie (Centuri, earlier)

static struct BurnRomInfo circusceRomDesc[] = {
	{ "380_p05.3h",		0x2000, 0x7ca74494, 1 | BRF_PRG | BRF_ESS }, //  0 M6809 Code (Encrypted)
	{ "380_p04.4h",		0x2000, 0xdd0c0ee7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "380_p03.5h",		0x2000, 0x190247af, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "380_p02.6h",		0x2000, 0x7e63725e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "380_p01.7h",		0x2000, 0xeedaa5b2, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "380_l14.5c",		0x2000, 0x607df0fb, 2 | BRF_PRG | BRF_ESS }, //  5 Z80 Code
	{ "380_l15.7c",		0x2000, 0xa6ad30e1, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "380_j12.4a",		0x2000, 0x56e5b408, 3 | BRF_GRA },           //  7 Tiles
	{ "380_j13.5a",		0x2000, 0x5aca0193, 3 | BRF_GRA },           //  8

	{ "380_j06.11e",	0x2000, 0xdf0405c6, 4 | BRF_GRA },           //  9 Sprites
	{ "380_j07.12e",	0x2000, 0x23dfe3a6, 4 | BRF_GRA },           // 10
	{ "380_j08.13e",	0x2000, 0x3ba95390, 4 | BRF_GRA },           // 11
	{ "380_j09.14e",	0x2000, 0xa9fba85a, 4 | BRF_GRA },           // 12
	{ "380_j10.15e",	0x2000, 0x0532347e, 4 | BRF_GRA },           // 13
	{ "380_j11.16e",	0x2000, 0xe1725d24, 4 | BRF_GRA },           // 14

	{ "380_j18.2a",		0x0020, 0x10dd4eaa, 5 | BRF_GRA },           // 15 Color PROMs
	{ "380_j17.7b",		0x0100, 0x13989357, 5 | BRF_GRA },           // 16
	{ "380_j16.10c",	0x0100, 0xc244f2aa, 5 | BRF_GRA },           // 17
};

STD_ROM_PICK(circusce)
STD_ROM_FN(circusce)

struct BurnDriver BurnDrvCircusce = {
	"circusce", "circusc", NULL, NULL, "1984",
	"Circus Charlie (Centuri, earlier)\0", NULL, "Konami (Centuri license)", "GX380",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_PLATFORM, 0,
	NULL, circusceRomInfo, circusceRomName, NULL, NULL, CircuscInputInfo, CircuscDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	224, 256, 3, 4
};
