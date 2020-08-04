// FB ALpha Rock'n Rage driver module
// Based on MAME driver by Manuel Abadia

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "hd6309_intf.h"
#include "burn_ym2151.h"
#include "vlm5030.h"
#include "k007342_k007420.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309ROM;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvVLMROM;
static UINT8 *DrvLutPROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvHD6309RAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvPalRAM;

static UINT8 HD6309Bank;
static UINT8 soundlatch;
static UINT8 videoregs;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[3];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT32 watchdog;

static struct BurnInputInfo RockrageInputList[] = {
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
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"}	,
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

STDINPUTINFO(Rockrage)

static struct BurnDIPInfo RockrageDIPList[]=
{
	{0x12, 0xff, 0xff, 0xe0, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x5d, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x20, 0x20, "Off"			},
	{0x12, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x80, 0x80, "Off"			},
	{0x12, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x13, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},
	{0x13, 0x01, 0x0f, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
	{0x13, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x13, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x13, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x13, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x13, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x13, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x13, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x13, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x13, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x13, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x13, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x13, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},
	{0x13, 0x01, 0xf0, 0x00, "Invalid"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x14, 0x01, 0x03, 0x03, "1"			},
	{0x14, 0x01, 0x03, 0x02, "2"			},
	{0x14, 0x01, 0x03, 0x01, "3"			},
	{0x14, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x14, 0x01, 0x08, 0x08, "30k & Every 70k"	},
	{0x14, 0x01, 0x08, 0x00, "40k & Every 80k"	},

	{0   , 0xfe, 0   ,    2, "Freeze Screen"	},
	{0x14, 0x01, 0x10, 0x10, "Off"			},
	{0x14, 0x01, 0x10, 0x00, "On"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x60, 0x60, "Easy"			},
	{0x14, 0x01, 0x60, 0x40, "Medium"		},
	{0x14, 0x01, 0x60, 0x20, "Hard"			},
	{0x14, 0x01, 0x60, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x80, 0x80, "Off"			},
	{0x14, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Rockrage)

static void bankswitch(INT32 bank)
{
	HD6309Bank = bank;
	HD6309MapMemory(DrvHD6309ROM + 0x10000 + (((bank & 0x70) >> 4) * 0x2000), 0x6000, 0x7fff, MAP_ROM);
}

static void rockrage_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x2600) {
		K007342Regs[0][address & 7] = data;
		return;
	}

	switch (address)
	{
		case 0x2e80:
			soundlatch = data;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x2ec0:
			watchdog = 0;
		return;

		case 0x2f00:
			videoregs = data;
		return;

		case 0x2f40:
			bankswitch(data);
		return;
	}
}

static UINT8 rockrage_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x2e00:
		case 0x2e01:
		case 0x2e02:
			return DrvInputs[address & 3];

		case 0x2e03:
			return DrvDips[2];

		case 0x2e40:
			return DrvDips[1];
	}

	return 0;
}

static void rockrage_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x2000:
			vlm5030_data_write(0, data);
		return;

		case 0x4000:
			vlm5030_rst(0, (data >> 1) & 1);
			vlm5030_st(0, (data >> 0) & 1);
		return;

		case 0x6000:
			BurnYM2151SelectRegister(data);
		return;

		case 0x6001:
			BurnYM2151WriteRegister(data);
		return;
	}
}

static UINT8 rockrage_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x3000:
			return (vlm5030_bsy(0) ? 1 : 0);

		case 0x5000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0x6000:
		case 0x6001:
			return BurnYM2151Read();
	}

	return 0;
}

static UINT32 DrvVLM5030Sync(INT32 samples_rate)
{
	return (samples_rate * M6809TotalCycles()) / 25000;
}

static void rockrage_tile_callback(INT32 layer, INT32 bank, INT32 *code, INT32 *color, INT32 */*flags*/)
{
	if (layer == 1) {
		*code |= ((*color & 0x40) << 2) | ((bank & 0x01) << 9);
	} else {
		*code |= ((*color & 0x40) << 2) | ((bank & 0x03) << 10) | ((videoregs & 0x04) << 7) | ((videoregs & 0x08) << 9);
	}

	*color = (layer * 0x10) + (*color & 0xf);
}

static void rockrage_sprite_callback(INT32 *code, INT32 *color)
{
	*code |= ((*color & 0x40) << 2) | ((*color & 0x80) << 1) * ((videoregs & 0x03) << 1);
	*code = (*code << 2) | ((*color & 0x30) >> 4);
	*color = 0x20 + (*color & 0x0f);
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	HD6309Open(0);
	HD6309Reset();
	HD6309Close();

	M6809Open(0);
	M6809Reset();
	M6809Close();

	BurnYM2151Reset();
	vlm5030Reset(0);

	K007342Reset();

	HD6309Bank = 0;
	soundlatch = 0;
	videoregs = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM		= Next; Next += 0x020000;
	DrvM6809ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x080000;
	DrvGfxROM1		= Next; Next += 0x080000;

	DrvVLMROM		= Next; Next += 0x008000;

	DrvLutPROM		= Next; Next += 0x000300;
;
	DrvPalette		= (UINT32*)Next; Next += 0x300 * sizeof(UINT32);

	AllRam			= Next;

	DrvHD6309RAM		= Next; Next += 0x002000;
	DrvM6809RAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000100;

	K007342VidRAM[0]	= Next; Next += 0x002000;
	K007342ScrRAM[0]	= Next; Next += 0x000200;

	K007420RAM[0]		= Next; Next += 0x000200;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len) // 64, 32
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
		if (BurnLoadRom(DrvHD6309ROM + 0x08000,  0, 1)) return 1;
		if (BurnLoadRom(DrvHD6309ROM + 0x10000,  1, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM  + 0x08000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00001,  3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  4, 2)) return 1;

		if (BurnDrvGetFlags() & BDF_PROTOTYPE)
		{
			if (BurnLoadRom(DrvGfxROM0   + 0x20001,  5, 2)) return 1;
			if (BurnLoadRom(DrvGfxROM0   + 0x20000,  6, 2)) return 1;

			if (BurnLoadRom(DrvGfxROM1   + 0x00000,  7, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x10000,  8, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x20000,  9, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x30000, 10, 1)) return 1;
	
			if (BurnLoadRom(DrvLutPROM   + 0x00000, 11, 1)) return 1;
			if (BurnLoadRom(DrvLutPROM   + 0x00100, 12, 1)) return 1;
			if (BurnLoadRom(DrvLutPROM   + 0x00200, 13, 1)) return 1;
	
			if (BurnLoadRom(DrvVLMROM    + 0x00000, 14, 1)) return 1;
		}
		else
		{
			if (BurnLoadRom(DrvGfxROM1   + 0x00000,  5, 1)) return 1;
			if (BurnLoadRom(DrvGfxROM1   + 0x20000,  6, 1)) return 1;
	
			if (BurnLoadRom(DrvLutPROM   + 0x00000,  7, 1)) return 1;
			if (BurnLoadRom(DrvLutPROM   + 0x00100,  8, 1)) return 1;
			if (BurnLoadRom(DrvLutPROM   + 0x00200,  9, 1)) return 1;
	
			if (BurnLoadRom(DrvVLMROM    + 0x00000, 10, 1)) return 1;
		}

		DrvGfxExpand(DrvGfxROM0, 0x40000);
		DrvGfxExpand(DrvGfxROM1, 0x40000);
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(K007342VidRAM[0],	0x0000, 0x1fff, MAP_RAM);
	HD6309MapMemory(K007420RAM[0], 		0x2000, 0x21ff, MAP_RAM);
	HD6309MapMemory(K007342ScrRAM[0],	0x2200, 0x23ff, MAP_RAM);
	HD6309MapMemory(DrvPalRAM,		0x2400, 0x24ff, MAP_RAM);
	HD6309MapMemory(DrvHD6309RAM,		0x4000, 0x5fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM + 0x08000, 0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(rockrage_main_write);
	HD6309SetReadHandler(rockrage_main_read);
	HD6309Close();

	M6809Init(0);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,		0x7000, 0x77ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM  + 0x08000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(rockrage_sound_write);
	M6809SetReadHandler(rockrage_sound_read);
	M6809Close();

	BurnYM2151Init(3579545);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.60, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.60, BURN_SND_ROUTE_RIGHT);

	vlm5030Init(0, 3579545, DrvVLM5030Sync, DrvVLMROM, 0x8000, 1);
	vlm5030SetAllRoutes(0, 1.20, BURN_SND_ROUTE_BOTH);

	K007342Init(DrvGfxROM0, rockrage_tile_callback);
	K007342SetOffsets(0, 16);

	K007420Init(0x3ff, rockrage_sprite_callback);
	K007420SetOffsets(0, 16);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	HD6309Exit();

	vlm5030Exit();
	BurnYM2151Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[0x40];
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x80 / 2; i++) {
		INT32 r = (BURN_ENDIAN_SWAP_INT16(p[i]) & 0x1f);
		INT32 g = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 5) & 0x1f;
		INT32 b = (BURN_ENDIAN_SWAP_INT16(p[i]) >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x300; i++) {
		DrvPalette[i] = pens[((i&0x300)/0x10)+(DrvLutPROM[i]&0xf)];
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 1;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) K007342DrawLayer(0, 0 | K007342_OPAQUE, 0);
	if (nSpriteEnable & 1) K007420DrawSprites(DrvGfxROM1);
	if (nBurnLayer & 2) K007342DrawLayer(0, 1 | K007342_OPAQUE, 0);
	if (nBurnLayer & 4) K007342DrawLayer(1, 0, 0);
	if (nBurnLayer & 8) K007342DrawLayer(1, 1, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (watchdog >= 120) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	HD6309NewFrame();
	M6809NewFrame();

	{
		memset (DrvInputs, 0, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & 0x1f) | (DrvDips[0] & 0xe0);
	}

	INT32 nInterleave = ((nBurnSoundLen) ? nBurnSoundLen : 1);
	INT32 nCyclesTotal[2] = { 3000000 / 60, 1500000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	HD6309Open(0);
	M6809Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += HD6309Run(nCyclesTotal[0] / nInterleave);

		nCyclesDone[1] += M6809Run(nCyclesTotal[1] / nInterleave);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (K007342_irq_enabled()) HD6309SetIRQLine(0, CPU_IRQSTATUS_AUTO);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		if (nSegmentLength) {
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
		vlm5030Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	HD6309Close();
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
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		HD6309Scan(nAction);
		M6809Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		vlm5030Scan(nAction, pnMin);

		K007342Scan(nAction);

		SCAN_VAR(HD6309Bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(videoregs);
	}

	if (nAction & ACB_WRITE) {
		HD6309Open(0);
		bankswitch(HD6309Bank);
		HD6309Close();
	}

	return 0;
}


// Rock'n Rage (World)

static struct BurnRomInfo rockrageRomDesc[] = {
	{ "620q01.16c",		0x08000, 0x0ddb5ef5, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "620q02.15c",		0x10000, 0xb4f6e346, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "620k03.11c",		0x08000, 0x9fbefe82, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "620k05.rom",		0x20000, 0x145d387c, 3 | BRF_GRA },           //  3 Tiles
	{ "620k06.rom",		0x20000, 0x7fa2c57c, 3 | BRF_GRA },           //  4

	{ "620k11.rom",		0x20000, 0x70449239, 4 | BRF_GRA },           //  5 Sprites
	{ "620l10.8g",		0x20000, 0x06d108e0, 4 | BRF_GRA },           //  6

	{ "620k07.13g",		0x00100, 0xb6135ee0, 5 | BRF_GRA },           //  7 Color Lookup Tables
	{ "620k08.12g",		0x00100, 0xb499800c, 5 | BRF_GRA },           //  8
	{ "620k09.11g",		0x00100, 0x9f0e0608, 5 | BRF_GRA },           //  9

	{ "620k04.6e",		0x08000, 0x8be969f3, 6 | BRF_GRA },           // 10 VLM5030 Samples
};

STD_ROM_PICK(rockrage)
STD_ROM_FN(rockrage)

struct BurnDriver BurnDrvRockrage = {
	"rockrage", NULL, NULL, NULL, "1986",
	"Rock'n Rage (World)\0", NULL, "Konami", "GX620",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, rockrageRomInfo, rockrageRomName, NULL, NULL, NULL, NULL, RockrageInputInfo, RockrageDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// Rock'n Rage (prototype?)

static struct BurnRomInfo rockrageaRomDesc[] = {
	{ "620n01.16c",		0x10000, 0xf89f56ea, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "620n02.15c",		0x10000, 0x5bc1f1cf, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "620k03.11c",		0x08000, 0x9fbefe82, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "620d05a.16g",	0x10000, 0x4d53fde9, 3 | BRF_GRA },           //  3 Tiles
	{ "620d06a.15g",	0x10000, 0x8cc05d4b, 3 | BRF_GRA },           //  4
	{ "620d05b.16f",	0x10000, 0x69f4599f, 3 | BRF_GRA },           //  5
	{ "620d06b.15f",	0x10000, 0x3892d41d, 3 | BRF_GRA },           //  6

	{ "620g11a.7g",		0x10000, 0x0ef40c2c, 4 | BRF_GRA },           //  7 Sprites
	{ "620d11b.7f",		0x10000, 0x8f116cbf, 4 | BRF_GRA },           //  8
	{ "620d10a.8g",		0x10000, 0x4789ae7b, 4 | BRF_GRA },           //  9
	{ "620g10b.8f",		0x10000, 0x1618854a, 4 | BRF_GRA },           // 10

	{ "620k07.13g",		0x00100, 0xb6135ee0, 5 | BRF_GRA },           // 11 Color Lookup Tables
	{ "620k08.12g",		0x00100, 0xb499800c, 5 | BRF_GRA },           // 12
	{ "620k09.11g",		0x00100, 0x9f0e0608, 5 | BRF_GRA },           // 13

	{ "620k04.6e",		0x08000, 0x8be969f3, 6 | BRF_GRA },           // 14 VLM5030 Samples
};

STD_ROM_PICK(rockragea)
STD_ROM_FN(rockragea)

struct BurnDriver BurnDrvRockragea = {
	"rockragea", "rockrage", NULL, NULL, "1986",
	"Rock'n Rage (prototype?)\0", NULL, "Konami", "GX620",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, rockrageaRomInfo, rockrageaRomName, NULL, NULL, NULL, NULL, RockrageInputInfo, RockrageDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};


// Koi no Hotrock (Japan)

static struct BurnRomInfo rockragejRomDesc[] = {
	{ "620k01.16c",		0x08000, 0x4f5171f7, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code
	{ "620k02.15c",		0x10000, 0x04c4d8f7, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "620k03.11c",		0x08000, 0x9fbefe82, 2 | BRF_PRG | BRF_ESS }, //  2 M6809 Code

	{ "620k05.16g",		0x20000, 0xca9d9346, 3 | BRF_GRA },           //  3 Tiles
	{ "620k06.15g",		0x20000, 0xc0e2b35c, 3 | BRF_GRA },           //  4

	{ "620k11.7g",		0x20000, 0x7430f6e9, 4 | BRF_GRA },           //  5 Sprites
	{ "620k10.8g",		0x20000, 0x0d1a95ab, 4 | BRF_GRA },           //  6

	{ "620k07.13g",		0x00100, 0xb6135ee0, 5 | BRF_GRA },           //  7 Color Lookup Tables
	{ "620k08.12g",		0x00100, 0xb499800c, 5 | BRF_GRA },           //  8
	{ "620k09.11g",		0x00100, 0x9f0e0608, 5 | BRF_GRA },           //  9

	{ "620k04.6e",		0x08000, 0x8be969f3, 6 | BRF_GRA },           // 10 VLM5030 Samples
};

STD_ROM_PICK(rockragej)
STD_ROM_FN(rockragej)

struct BurnDriver BurnDrvRockragej = {
	"rockragej", "rockrage", NULL, NULL, "1986",
	"Koi no Hotrock (Japan)\0", NULL, "Konami", "GX620",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, rockragejRomInfo, rockragejRomName, NULL, NULL, NULL, NULL, RockrageInputInfo, RockrageDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x300,
	256, 224, 4, 3
};
