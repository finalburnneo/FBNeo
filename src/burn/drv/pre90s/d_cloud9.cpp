// FB Alpha Cloud 9 / Firebeast driver module
// Based on MAME driver by Mike Balfour and Aaron Giles

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "pokey.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvVidPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT16 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 irq_state;
static UINT8 video_latch[8];
static UINT8 bitmode_addr[2];

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT32 TrackX;
static INT32 TrackY;

static INT32 is_firebeast = 0;

static struct BurnInputInfo Cloud9InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"	},

	{"P1 Up",		    BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Cloud9)

static struct BurnInputInfo FirebeasInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 fire 2"	},

	{"P1 Up",		    BIT_DIGITAL,	DrvJoy3 + 0,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 4,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Firebeas)

static struct BurnDIPInfo Cloud9DIPList[]=
{
	{0x0a, 0xff, 0xff, 0x05, NULL					},
	{0x0b, 0xff, 0xff, 0x08, NULL					},

	{0   , 0xfe, 0   ,    11, "Coinage"				},
	{0x0a, 0x01, 0xfe, 0x22, "9 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x1e, "8 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x1a, "7 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x16, "6 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x12, "5 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x0e, "4 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x0a, "3 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x06, "2 Coins 1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x04, "1 Coin  1 Credits"	},
	{0x0a, 0x01, 0xfe, 0x02, "1 Coin  2 Credits"	},
	{0x0a, 0x01, 0xfe, 0x00, "Free Play"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0b, 0x01, 0x08, 0x08, "Off"					},
	{0x0b, 0x01, 0x08, 0x00, "On"					},
};

STDDIPINFO(Cloud9)

static struct BurnDIPInfo FirebeasDIPList[]=
{
	{0x0a, 0xff, 0xff, 0xff, NULL					},
	{0x0b, 0xff, 0xff, 0x08, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0b, 0x01, 0x08, 0x08, "Off"					},
	{0x0b, 0x01, 0x08, 0x00, "On"					},
};

STDDIPINFO(Firebeas)

static void palette_write(UINT8 offset, INT32 data)
{
	DrvPalRAM[offset] = data;

	data ^= 0x1ff;

	INT32 bit0 = (data >> 6) & 0x01;
	INT32 bit1 = (data >> 7) & 0x01;
	INT32 bit2 = (data >> 8) & 0x01;
	INT32 r = (((bit0 * 4700) + (bit1 * 10000) + (bit2 * 22000)) * 255) / 36700;

	bit0 = (data >> 3) & 0x01;
	bit1 = (data >> 4) & 0x01;
	bit2 = (data >> 5) & 0x01;
	INT32 g = (((bit0 * 4700) + (bit1 * 10000) + (bit2 * 22000)) * 255) / 36700;

	bit0 = (data >> 0) & 0x01;
	bit1 = (data >> 1) & 0x01;
	bit2 = (data >> 2) & 0x01;
	INT32 b = (((bit0 * 4700) + (bit1 * 10000) + (bit2 * 22000)) * 255) / 36700;

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static inline void bitmode_autoinc()
{
	if (!video_latch[0])
		bitmode_addr[0]++;

	if (!video_latch[1])
		bitmode_addr[1]++;
}

static void write_vram(UINT16 addr, UINT8 data, UINT8 bitmd, UINT8 pixba )
{
	UINT8 *dest = &DrvVidRAM[0x0000 | (addr & 0x3fff)];
	UINT8 *dest2 = &DrvVidRAM[0x4000 | (addr & 0x3fff)];
	UINT8 promaddr = 0;
	UINT8 wpbits;

	promaddr |= bitmd << 7;
	promaddr |= video_latch[4] << 6;
	promaddr |= video_latch[6] << 5;
	promaddr |= ((addr & 0xf000) != 0x4000) << 4;
	promaddr |= ((addr & 0x3800) == 0x0000) << 3;
	promaddr |= ((addr & 0x0600) == 0x0600) << 2;
	promaddr |= (pixba << 0);

	wpbits = DrvVidPROM[0x200 + promaddr];

	if (!(wpbits & 1))
		dest2[0] = (dest2[0] & 0x0f) | (data & 0xf0);
	if (!(wpbits & 2))
		dest2[0] = (dest2[0] & 0xf0) | (data & 0x0f);
	if (!(wpbits & 4))
		dest[0] = (dest[0] & 0x0f) | (data & 0xf0);
	if (!(wpbits & 8))
		dest[0] = (dest[0] & 0xf0) | (data & 0x0f);
}

static UINT8 bitmode_read()
{
	UINT16 addr = (bitmode_addr[1] << 6) | (bitmode_addr[0] >> 2);

	UINT8 result = DrvVidRAM[((~bitmode_addr[0] & 2) << 13) | addr] << ((bitmode_addr[0] & 1) * 4);

	bitmode_autoinc();

	return (result >> 4) | 0xf0;
}

static void bitmode_write(UINT8 data)
{
	UINT16 addr = (bitmode_addr[1] << 6) | (bitmode_addr[0] >> 2);

	data = (data & 0x0f) | (data << 4);

	write_vram(addr, data, 1, bitmode_addr[0] & 3);

	bitmode_autoinc();
}

static void cloud9_write(UINT16 address, UINT8 data)
{
	if (address <= 0x0001) {
		write_vram(address, data, 0, 0);
		bitmode_addr[address] = data;
		// falls through to write_vram()!
	}

	if (address == 0x0002) {
		bitmode_write(data);
		return;
	}

	if (address <= 0x4fff) {
		write_vram(address, data, 0, 0);
		return;
	}

	if ((address & 0xff80) == 0x5400) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xff80) == 0x5480) {
		if (irq_state) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			irq_state = 0;
		}
		return;
	}

	if ((address & 0xff80) == 0x5500) {
		INT32 val = (data << 1) | ((address & 0x40) >> 6);
		palette_write(address & 0x3f, val);
		return;
	}

	if ((address & 0xff80) == 0x5580) {
		video_latch[address & 7] = data >> 7;
		return;
	}

	if ((address & 0xff80) == 0x5600) {
		data >>= 7;
		address &= 7;
		// coin coiners & leds
		return;
	}

	if ((address & 0xff80) == 0x5680) {
		// nvram store
		return;
	}

	if ((address & 0xff80) == 0x5700) {
		// nvram recall
		return;
	}

	if ((address & 0xfe00) == 0x5a00) {
		pokey_write((address >> 8) & 1, address & 0xf, data);
		return;
	}

	if ((address & 0xfc00) == 0x5c00) {
		DrvNVRAM[address & 0xff] = data & 0x0f;
		return;
	}
}

static UINT8 cloud9_read(UINT16 address)
{
	if (address == 0x0002) {
		return bitmode_read();
	}

	if (address >= 0x0000 && address <= 0x4fff) {
		return DrvVidRAM[address];
	}

	if ((address & 0xff80) == 0x5800) {
		if (address & 1) {
			return DrvInputs[1];
		} else {
			return (DrvInputs[0] & ~0x80) | (vblank ? 0 : 0x80);
		}
	}

	if ((address & 0xfffc) == 0x5900) {
		return (address & 3) ? (TrackX & 0xff) : (TrackY & 0xff);
	}

	if ((address & 0xfe00) == 0x5a00) {
		return pokey_read((address >> 8) & 1, address & 0xf);
	}

	if ((address & 0xfc00) == 0x5c00) {
		return DrvNVRAM[address & 0xff] | 0xf0;
	}
	
	return 0;
}

static INT32 pokey_1_callback(INT32)
{
	return DrvDips[0];
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();

	PokeyReset();

	irq_state = 0;
	for (INT32 i = 0; i < 8; i++) {
		video_latch[i] = 0;
	}
	bitmode_addr[0] = 0;
	bitmode_addr[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM		= Next; Next += 0x010000;

	DrvVidPROM		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x0041 * sizeof(UINT32);

	DrvNVRAM		= Next; Next += 0x000100;

	AllRam			= Next;

	DrvSprRAM		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x008000;
	
	DrvPalRAM		= (UINT16*)Next; Next += 0x000040 * sizeof(UINT16);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4]  = { 0x2000*8*3, 0x2000*8*2, 0x2000*8*1, 0x2000*8*0 };
	INT32 XOffs[16] = { STEP16(0,1) };
	INT32 YOffs[16] = { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x8000);

	GfxDecode(0x0100, 4, 16, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

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
		INT32 k = 0;
		if (BurnLoadRom(DrvM6502ROM + 0x6000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x8000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xa000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xc000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0xe000, k++, 1)) return 1;

		// firebeast has a half-sized rom at the end (check for 0'd vectors)
		if (DrvM6502ROM[0xffff] == 0 && DrvM6502ROM[0xfffe] == 0) {
			memcpy (DrvM6502ROM + 0xf000, DrvM6502ROM + 0xe000, 0x1000);
			is_firebeast = 1;
		}

		if (BurnLoadRom(DrvGfxROM   + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x2000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x4000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x6000, k++, 1)) return 1;

		if (BurnLoadRom(DrvVidPROM  + 0x0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvVidPROM  + 0x0100, k++, 1)) return 1;
		if (BurnLoadRom(DrvVidPROM  + 0x0200, k++, 1)) return 1;
		if (BurnLoadRom(DrvVidPROM  + 0x0300, k++, 1)) return 1;

		DrvGfxDecode();
	}

	memset(DrvNVRAM, 0xff, 0x100);

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	//M6502MapMemory(DrvVidRAM,				0x0000, 0x4fff, MAP_ROM); // handler!!
	M6502MapMemory(DrvSprRAM,				0x5000, 0x53ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0x6000,	0x6000, 0xffff, MAP_RAM);
	M6502SetWriteHandler(cloud9_write);
	M6502SetReadHandler(cloud9_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(1250000, 2, 0.50, 0);
	PokeyAllPotCallback(1, pokey_1_callback);
	PokeySetTotalCyclesCB(M6502TotalCycles);

	GenericTilesInit();

	DrvDoReset(1);

#if 0
	// might come in handy to find values for other drivers that might uses similar sync prom?
	INT32 m_vblank_start = 0;
	INT32 m_vblank_end = 0;
	/* find the start of VBLANK in the SYNC PROM */
	for (m_vblank_start = 0; m_vblank_start < 256; m_vblank_start++)
		if ((DrvVidPROM[(m_vblank_start - 1) & 0xff] & 2) != 0 && (DrvVidPROM[m_vblank_start] & 2) == 0)
			break;
	if (m_vblank_start == 0)
		m_vblank_start = 256;

	/* find the end of VBLANK in the SYNC PROM */
	for (m_vblank_end = 0; m_vblank_end < 256; m_vblank_end++)
		if ((DrvVidPROM[(m_vblank_end - 1) & 0xff] & 2) == 0 && (DrvVidPROM[m_vblank_end] & 2) != 0)
			break;

	bprintf(0, _T("vblank begin: %d  vblank end: %d.\n"), m_vblank_start, m_vblank_end);
#endif

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();

	PokeyExit();

	BurnFree(AllMem);

	is_firebeast = 0;

	return 0;
}

#define SCREEN_START_OFFSET 24

static void draw_sprites()
{
	for (INT32 offs = 0; offs < 0x20; offs++)
	{
		if ((DrvSprRAM[offs + 0x00] & 0xfe) == 0) continue;
		INT32 sx    = DrvSprRAM[offs + 0x60];
		INT32 sy    = 241 - DrvSprRAM[offs] - SCREEN_START_OFFSET;
		INT32 flipx = DrvSprRAM[offs + 0x40] & 0x80;
		INT32 flipy = DrvSprRAM[offs + 0x40] & 0x40;
		INT32 code  = DrvSprRAM[offs + 0x20];

		Draw16x16MaskTile(pTransDraw, code, sx, sy, flipx, flipy, 0, 4, 0, 0x10, DrvGfxROM);

		if (sx >= 240) // wrap
			Draw16x16MaskTile(pTransDraw, code, sx - 256, sy, flipx, flipy, 0, 4, 0, 0x10, DrvGfxROM);
	}
}

static INT32 lastline;

static void draw_bitmap(INT32 line)
{
	INT32 flip = video_latch[5] ? 0xff : 0x00;

	UINT8 *src[2] = { DrvVidRAM + 0x4000, DrvVidRAM };

	for (INT32 y = lastline + SCREEN_START_OFFSET; y < line + SCREEN_START_OFFSET; y++)
	{
		if ((y - SCREEN_START_OFFSET) > nScreenHeight) break;
		UINT16 *dst = pTransDraw + (y - SCREEN_START_OFFSET) * nScreenWidth;
		INT32 effy = (y ^ flip) * 64;

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			INT32 effx = x ^ flip;

			dst[x] = ((src[(effx >> 1) & 1][effy + (effx / 4)] >> ((~effx & 1) * 4)) & 0x0f);
		}
	}
}

static void DrvDrawBegin()
{
	lastline = 0;

	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x40; i++) {
			palette_write(i, DrvPalRAM[i]);
		}
		DrvRecalc = 0;
	}

	if ((nBurnLayer & 1) == 0)
		BurnTransferClear();
}

static void DrvDrawLine(INT32 line)
{
	if ((nBurnLayer & 1) != 0)
		draw_bitmap(line);

	lastline = line;
}

static void DrvDrawEnd()
{
	DrvDrawLine(256);

	if ((nSpriteEnable & 1) != 0)
		draw_sprites();

	INT32 palette_bank = video_latch[7] ? 0x20 : 0x00;

	BurnTransferCopy(DrvPalette + palette_bank);
}

static INT32 DrvReRedraw()
{
	DrvDrawBegin();
	DrvDrawLine(256);
	DrvDrawEnd();

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & ~0x08) | (DrvDips[1] & 0x08);

		{
			// UDLR fake trackball
			if (DrvJoy3[0]) TrackY+=3;
			if (DrvJoy3[1]) TrackY-=3;
			if (is_firebeast == 0) {
				if (DrvJoy3[2]) TrackX-=3;
				if (DrvJoy3[3]) TrackX+=3;
			} else {
				if (DrvJoy3[2]) TrackX+=3;
				if (DrvJoy3[3]) TrackX-=3;
			}
		}
	}

	INT32 nInterleave = 262;
	INT32 nTotalCycles = 1250000 / 60;
	INT32 nCyclesDone = 0;

	M6502Open(0);

	if (pBurnDraw)
		DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++) {
		vblank = (~DrvVidPROM[i & 0xff] >> 1) & 1;

		nCyclesDone += M6502Run(((i + 1) * nTotalCycles / nInterleave) - nCyclesDone);
		if ((i%64) == 63) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			irq_state = 1;
			DrvDrawLine(i);
		}
	}

	M6502Close();

	if (pBurnSoundOut) {
		pokey_update(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDrawEnd();
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
		pokey_scan(nAction, pnMin);

		SCAN_VAR(irq_state);
		SCAN_VAR(video_latch);
		SCAN_VAR(bitmode_addr);
		SCAN_VAR(TrackX);
		SCAN_VAR(TrackY);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00100;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}


// Cloud 9 (prototype)

static struct BurnRomInfo cloud9RomDesc[] = {
	{ "c9_6000.bin",	0x2000, 0xb5d95d98, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "c9_8000.bin",	0x2000, 0x49af8f22, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "c9_a000.bin",	0x2000, 0x7cf404a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "c9_c000.bin",	0x2000, 0x26a4d7df, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "c9_e000.bin",	0x2000, 0x6e663bce, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "c9_gfx0.bin",	0x1000, 0xd01a8019, 2 | BRF_GRA },           //  5 Sprites
	{ "c9_gfx1.bin",	0x1000, 0x514ac009, 2 | BRF_GRA },           //  6
	{ "c9_gfx2.bin",	0x1000, 0x930c1ade, 2 | BRF_GRA },           //  7
	{ "c9_gfx3.bin",	0x1000, 0x27e9b88d, 2 | BRF_GRA },           //  8

	{ "63s141.e10",		0x0100, 0x8e98083f, 3 | BRF_GRA },           //  9 PROMs
	{ "63s141.m10",		0x0100, 0xb0b039c0, 3 | BRF_GRA },           // 10
	{ "82s129.p3",		0x0100, 0x615d784d, 3 | BRF_GRA },           // 11
	{ "63s141.m8",		0x0100, 0x6d7479ec, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(cloud9)
STD_ROM_FN(cloud9)

struct BurnDriver BurnDrvCloud9 = {
	"cloud9", NULL, NULL, NULL, "1983",
	"Cloud 9 (prototype)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT | GBF_ACTION, 0,
	NULL, cloud9RomInfo, cloud9RomName, NULL, NULL, NULL, NULL, Cloud9InputInfo, Cloud9DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Firebeast (prototype)

static struct BurnRomInfo firebeasRomDesc[] = {
	{ "6000.j2",		0x2000, 0xdbd04782, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 Code
	{ "8000.kl2",		0x2000, 0x96231ca4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "a000.lm2",		0x2000, 0xf7e0bf25, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "c000.n2",		0x2000, 0x43a91b74, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "e000.p2",		0x1000, 0x8e5045ab, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "mo0000.r12",		0x2000, 0x5246c979, 2 | BRF_GRA },           //  5 Sprites
	{ "mo2000.p12",		0x2000, 0x1a3b6d57, 2 | BRF_GRA },           //  6
	{ "mo4000.n12",		0x2000, 0x69e18319, 2 | BRF_GRA },           //  7
	{ "mo6000.m12",		0x2000, 0xb722997f, 2 | BRF_GRA },           //  8

	{ "63s141.e10",		0x0100, 0x8e98083f, 3 | BRF_GRA },           //  9 PROMs
	{ "63s141.m10",		0x0100, 0xb0b039c0, 3 | BRF_GRA },           // 10
	{ "82s129.p3",		0x0100, 0x615d784d, 3 | BRF_GRA },           // 11
	{ "63s141.m8",		0x0100, 0x6d7479ec, 3 | BRF_GRA },           // 12
};

STD_ROM_PICK(firebeas)
STD_ROM_FN(firebeas)

struct BurnDriver BurnDrvFirebeas = {
	"firebeas", NULL, NULL, NULL, "1983",
	"Firebeast (prototype)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, firebeasRomInfo, firebeasRomName, NULL, NULL, NULL, NULL, FirebeasInputInfo, FirebeasDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};
