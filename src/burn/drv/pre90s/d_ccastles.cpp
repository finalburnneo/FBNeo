// FB Alpha Crystal Castles driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "burn_gun.h"
#include "watchdog.h"
#include "pokey.h"
#include "x2212.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvGfxROM;
static UINT8 *DrvVidPROM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvVidRAM;
static UINT16 *DrvPalRAM;

static UINT16 *DrvTempDraw;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 bank_latch;
static INT32 irq_state;
static UINT8 video_latch[8];
static UINT8 bitmode_addr[2];
static INT32 hscroll;
static INT32 vscroll;
static INT32 vblank;
static INT32 nvram_storelatch[2];

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4f[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static INT16 DrvAnalogPortX = 0;
static INT16 DrvAnalogPortY = 0;

static INT32 is_joyver = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo CcastlesInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4f+ 0,	"p1 start"	},
	{"P1 Up",		    BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",     BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",     BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	A("P1 Trackball X", BIT_ANALOG_REL, &DrvAnalogPortX,"p1 x-axis"),
	A("P1 Trackball Y", BIT_ANALOG_REL, &DrvAnalogPortY,"p1 y-axis"),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4f+ 1,	"p2 start"	},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		    BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Tilt",		    BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};
#undef A

STDINPUTINFO(Ccastles)

static struct BurnInputInfo CcastlesjInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy4f+ 0,	"p1 start"	},
	{"P1 Up",		    BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	DrvJoy3 + 1,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 right"	},
	{"P1 Button 1",     BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 1"	},
	{"P1 Button 2",     BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"	},

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy4f+ 1,	"p2 start"	},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Service",		    BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Tilt",		    BIT_DIGITAL,	DrvJoy1 + 3,	"tilt"		},
	{"Dip A",		    BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		    BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Ccastlesj)

static struct BurnDIPInfo CcastlesDIPList[]=
{
	{0x0f, 0xff, 0xff, 0xc7, NULL				},
	{0x10, 0xff, 0xff, 0x10, NULL				},

	{0   , 0xfe, 0   ,    1, "Cabinet"			},
	{0x0f, 0x01, 0x20, 0x00, "Upright"			},
//	{0x0f, 0x01, 0x20, 0x20, "Cocktail"			}, // not impl.!

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x10, 0x01, 0x10, 0x10, "Off"				},
	{0x10, 0x01, 0x10, 0x00, "On"				},
};

STDDIPINFO(Ccastles)

static struct BurnDIPInfo CcastlesjDIPList[]=
{
	{0x0d, 0xff, 0xff, 0xc7, NULL				},
	{0x0e, 0xff, 0xff, 0x10, NULL				},

	{0   , 0xfe, 0   ,    1, "Cabinet"			},
	{0x0d, 0x01, 0x20, 0x00, "Upright"			},
//	{0x0d, 0x01, 0x20, 0x20, "Cocktail"			}, // not impl.!

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0e, 0x01, 0x10, 0x10, "Off"				},
	{0x0e, 0x01, 0x10, 0x00, "On"				},
};

STDDIPINFO(Ccastlesj)

static void palette_write(UINT8 offset)
{
	UINT16 data = DrvPalRAM[offset & 0x1f];

	INT32 r = ((data & 0xc0) >> 6) | ((data & 0x200) >> (3+4));
	INT32 b = (data & 0x38) >> 3;
	INT32 g = (data & 0x07);

	INT32 bit0 = (~r >> 0) & 0x01;
	INT32 bit1 = (~r >> 1) & 0x01;
	INT32 bit2 = (~r >> 2) & 0x01;
	r = (((bit0 * 4700) + (bit1 * 10000) + (bit2 * 22000)) * 255) / 36700;

	bit0 = (~g >> 0) & 0x01;
	bit1 = (~g >> 1) & 0x01;
	bit2 = (~g >> 2) & 0x01;
	g = (((bit0 * 4700) + (bit1 * 10000) + (bit2 * 22000)) * 255) / 36700;

	bit0 = (~b >> 0) & 0x01;
	bit1 = (~b >> 1) & 0x01;
	bit2 = (~b >> 2) & 0x01;
	b = (((bit0 * 4700) + (bit1 * 10000) + (bit2 * 22000)) * 255) / 36700;

	DrvPalette[offset & 0x1f] = BurnHighCol(r,g,b,0);
}

static inline void bitmode_autoinc()
{
	if (!video_latch[0])
	{
		if (!video_latch[2])
			bitmode_addr[0]++;
		else
			bitmode_addr[0]--;
	}

	if (!video_latch[1])
	{
		if (!video_latch[3])
			bitmode_addr[1]++;
		else
			bitmode_addr[1]--;
	}
}

static void write_vram(UINT16 addr, UINT8 data, UINT8 bitmd, UINT8 pixba)
{
	UINT8 *dest = &DrvVidRAM[addr & 0x7ffe];
	UINT8 promaddr = 0;
	UINT8 wpbits;

	promaddr |= ((addr & 0xf000) == 0) << 7;
	promaddr |= (addr & 0x0c00) >> 5;
	promaddr |= (!bitmd) << 4;
	promaddr |= (addr & 0x0001) << 2;
	promaddr |= (pixba << 0);

	wpbits = DrvVidPROM[0x200 + promaddr];

	if (!(wpbits & 1))
		dest[0] = (dest[0] & 0xf0) | (data & 0x0f);
	if (!(wpbits & 2))
		dest[0] = (dest[0] & 0x0f) | (data & 0xf0);
	if (!(wpbits & 4))
		dest[1] = (dest[1] & 0xf0) | (data & 0x0f);
	if (!(wpbits & 8))
		dest[1] = (dest[1] & 0x0f) | (data & 0xf0);
}

static UINT8 bitmode_read()
{
	UINT16 addr = (bitmode_addr[1] << 7) | (bitmode_addr[0] >> 1);
	UINT8 result = DrvVidRAM[addr] << ((~bitmode_addr[0] & 1) * 4);
	bitmode_autoinc();

	return result | 0x0f;
}

static void bitmode_write(UINT8 data)
{
	UINT16 addr = (bitmode_addr[1] << 7) | (bitmode_addr[0] >> 1);
	data = (data & 0xf0) | (data >> 4);
	write_vram(addr, data, 1, bitmode_addr[0] & 3);
	bitmode_autoinc();
}

static void bankswitch(INT32 bank)
{
	bank_latch = bank;

	M6502MapMemory(DrvM6502ROM + 0xa000 + (bank * 0x6000), 0xa000, 0xdfff, MAP_ROM);
}

static void ccastles_write(UINT16 address, UINT8 data)
{
	if (address >= 0x9f80 && address <= 0x9fbf) {
		DrvPalRAM[address & 0x1f] = data | ((address & 0x20) << 4);
		palette_write(address & 0x1f);
		return;
	}

	if (address <= 0x0001) {
		write_vram(address, data, 0, 0);
		bitmode_addr[address] = data;
		// falls through to write_vram()!
		return;
	}

	if (address == 0x0002) {
		bitmode_write(data);
		return;
	}

	if (address <= 0x7fff) {
		write_vram(address, data, 0, 0);
		return;
	}

	if (address == 0x9e84) {
		// NOP
		return;
	}

	if (address == 0x9e87) {
		bankswitch(data & 1);
		return;
	}

	if ((address & 0xfff8) == 0x9f00) {
		video_latch[address & 7] = (data >> 3) & 1;
		return;
	}

	if ((address & 0xff80) == 0x9c00) {
		x2212_recall(0, 0);
		x2212_recall(0, 1);
		x2212_recall(0, 0);
		x2212_recall(1, 0);
		x2212_recall(1, 1);
		x2212_recall(1, 0);
		return;
	}

	if ((address & 0xff80) == 0x9c80) {
		hscroll = data;
		return;
	}

	if ((address & 0xff80) == 0x9d00) {
		vscroll = data;
		return;
	}

	if ((address & 0xff80) == 0x9e00) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xfffe) == 0x9e80) {
		// leds
		return;
	}

	if (address >= 0x9e82 && address <= 0x9e83) {
		nvram_storelatch[address & 1] = data & 1;
		x2212_store(0, ~nvram_storelatch[0] & nvram_storelatch[1]);
		x2212_store(1, ~nvram_storelatch[0] & nvram_storelatch[1]);
		return;
	}

	if (address >= 0x9e85 && address <= 0x9e86) {
		// ccounter
		return;
	}

	if ((address & 0xff80) == 0x9d80) {
		if (irq_state) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			irq_state = 0;
		}
		return;
	}

	if ((address & 0xfff0) == 0x9800) {
		pokey_write(0, address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x9a00) {
		pokey_write(1, address & 0xf, data);
		return;
	}

	if ((address & 0xfc00) == 0x9000) {
		x2212_write(0, address & 0xff, data >> 4);
		x2212_write(1, address & 0xff, data & 0xf);
		return;
	}
}

static UINT8 ccastles_read(UINT16 address)
{
	if (address == 0x0002) {
		return bitmode_read();
	}

	if (address <= 0x7fff) {
		return DrvVidRAM[address];
	}

	if (((address &~0x1f0) & 0xfff0) == 0x9800) {
		return pokey_read(0, address & 0xf);
	}

	if (((address &~0x1f0) & 0xfff0) == 0x9a00) {
		return pokey_read(1, address & 0xf);
	}

	if ((address & 0xfc00) == 0x9000) {
		return (x2212_read(1, address & 0xff) & 0x0f) | (x2212_read(0, address & 0xff) << 4);
	}

	if ((address & 0xfe00) == 0x9400) {
		switch (address & 1) {
			case 0: return (is_joyver) ? DrvInputs[2] : (BurnTrackballRead(0, 1)&0xff);
			case 1: return BurnTrackballRead(0, 0)&0xff;
		}
		return 0;
	}

	if ((address & 0xfe00) == 0x9600) {
		return (DrvInputs[0] & ~0x30) | (vblank << 5) | (DrvDips[1] & 0x10);
	}

	if (address >= 0x9f80 && address <= 0x9fbf) {
		return 0; // really, unmapped. (not pal)
	}

	return 0;
}

static INT32 pokey_1_callback(INT32)
{
	return (DrvDips[0] & ~0x18) | (DrvInputs[1] & 0x18);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	DrvPalRAM[0x10] = 0x2ff; // black background @ boot

	M6502Open(0);
	bankswitch(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();

	PokeyReset();

	x2212_reset();

	bank_latch = 0;
	irq_state = 0;
	for (INT32 i = 0; i < 8; i++) {
		video_latch[i] = 0;
	}
	bitmode_addr[0] = bitmode_addr[1] = 0;
	hscroll = vscroll = 0;

	nvram_storelatch[0] = nvram_storelatch[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM		= Next; Next += 0x014000;

	DrvGfxROM		= Next; Next += 0x010000;
	DrvVidPROM		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x0020 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM		= Next; Next += 0x000e00;
	DrvSprRAM		= Next; Next += 0x000200;
	DrvVidRAM		= Next; Next += 0x008000;

	DrvPalRAM		= (UINT16*)Next; Next += 0x000020 * sizeof(UINT16);

	RamEnd			= Next;

	DrvTempDraw     = (UINT16*)Next; Next += 400 * 400 * sizeof(UINT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { 4, RGN_FRAC(0x4000, 1, 2) + 0, RGN_FRAC(0x4000, 1, 2) + 4 };
	INT32 XOffs[8] = { 0, 1, 2, 3, 8+0, 8+1, 8+2, 8+3 };
	INT32 YOffs[16] = { 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16,	8*16, 9*16, 10*16, 11*16, 12*16, 13*16, 14*16, 15*16 };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x8000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM, 0x8000);

	GfxDecode(0x0100, 3, 8, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM);

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
		if (BurnLoadRom(DrvM6502ROM + 0x0a000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0c000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x0e000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x10000, k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM + 0x12000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM   + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM   + 0x02000, k++, 1)) return 1;

		if (BurnLoadRom(DrvVidPROM  + 0x00000, k++, 1)) return 1;
		if (BurnLoadRom(DrvVidPROM  + 0x00100, k++, 1)) return 1;
		if (BurnLoadRom(DrvVidPROM  + 0x00200, k++, 1)) return 1;
		if (BurnLoadRom(DrvVidPROM  + 0x00300, k++, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM,				0x8000, 0x8dff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,				0x8e00, 0x8fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM + 0xe000,	0xe000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(ccastles_write);
	M6502SetReadHandler(ccastles_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(1250000, 2, 0.50, 0);
	PokeyAllPotCallback(1, pokey_1_callback);
	PokeySetTotalCyclesCB(M6502TotalCycles);

	x2212_init_autostore(2);

	BurnTrackballInit(2);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	M6502Exit();

	PokeyExit();

	x2212_exit();

	BurnTrackballExit();

	BurnFree(AllMem);

	is_joyver = 0;

	return 0;
}

#define SCREEN_START_OFFSET 24

static void draw_sprites()
{
	UINT8 *sprite = &DrvSprRAM[video_latch[7] << 8];

	for (INT32 i = 0; i < (320 * 256); i++)
	{
		DrvTempDraw[i] = 0x0f;
	}

	for (INT32 offs = 0; offs < 0xa0; offs += 4)
	{
		INT32 sx    = sprite[offs + 3];
		INT32 sy    = 241 - sprite[offs + 1] - SCREEN_START_OFFSET;
		INT32 flipx = 0;
		INT32 flipy = 0;
		INT32 code  = sprite[offs];
		INT32 color = sprite[offs + 2] >> 7;

		DrawCustomMaskTile(DrvTempDraw, 8, 16, code, sx, sy, flipx, flipy, color, 3, 7, 0, DrvGfxROM);

		if (sx >= 240) // wrap
			DrawCustomMaskTile(DrvTempDraw, 8, 16, code, sx - 256, sy, flipx, flipy, color, 3, 7, 0, DrvGfxROM);
	}
}

static INT32 lastline;

static void draw_bitmap(INT32 line)
{
	INT32 flip = video_latch[4] ? 0xff : 0x00;

	for (INT32 y = lastline + SCREEN_START_OFFSET; y < line + SCREEN_START_OFFSET; y++)
	{
		if ((y - SCREEN_START_OFFSET) > nScreenHeight) continue;
		if ((y - SCREEN_START_OFFSET) < 0) continue;
		UINT16 *dst = pTransDraw + (y - SCREEN_START_OFFSET) * nScreenWidth;
		UINT16 *mosrc = DrvTempDraw + (y - SCREEN_START_OFFSET) * nScreenWidth;

		INT32 effy = (((y - 24) + (flip ? 0 : vscroll)) ^ flip) & 0xff;
		if (effy < 24)
			effy = 24;
		UINT8 *src = &DrvVidRAM[effy * 128];

		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			INT32 effx = (hscroll + (x ^ flip)) & 0xff;
			UINT8 pix = (src[effx / 2] >> ((effx & 1) * 4)) & 0x0f;
			UINT8 mopix = mosrc[x];
			UINT8 prindex, prvalue;

			prindex = 0x40;
			prindex |= (mopix & 7) << 2;
			prindex |= (mopix & 8) >> 2;
			prindex |= (pix & 8) >> 3;
			prvalue = DrvVidPROM[0x0300 + prindex];

			if (prvalue & 2)
				pix = mopix;

			pix |= (prvalue & 1) << 4;

			dst[x] = pix;
		}
	}
}

static void DrvDrawBegin()
{
	lastline = 0;

	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x20; i++) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferClear();

	if (nSpriteEnable & 1)
		draw_sprites();

}

static void DrvDrawLine(INT32 line)
{
	if (nBurnLayer & 1)
		draw_bitmap(line);

	lastline = line;
}

static void DrvDrawEnd()
{
	DrvDrawLine(256);

	BurnTransferCopy(DrvPalette);
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
		DrvInputs[2] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		{
			// fake start buttons for convenience
			if (DrvJoy4f[0]) DrvInputs[0] &= ~0x40;
			if (DrvJoy4f[1]) DrvInputs[0] &= ~0x80;
		}

		{
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_REVERSED);
			BurnTrackballFrame(0, DrvAnalogPortX, DrvAnalogPortY, 6, 0);
			BurnTrackballUDLR(0, DrvJoy3[2], DrvJoy3[1], DrvJoy3[0], DrvJoy3[3]);
			BurnTrackballUpdate(0);
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 1250000 / 60 };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSoundBufferPos = 0;

	M6502Open(0);

	if (pBurnDraw)
		DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++) {
		vblank = DrvVidPROM[i & 0xff] & 1;

		CPU_RUN(0, M6502);

		if ((i%64) == 63 && !irq_state) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			irq_state = 1;
			DrvDrawLine(i);
		}

		// Render Sound Segment
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	M6502Close();

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
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

		BurnTrackballScan();

		SCAN_VAR(bank_latch);
		SCAN_VAR(irq_state);
		SCAN_VAR(video_latch);
		SCAN_VAR(bitmode_addr);
		SCAN_VAR(hscroll);
		SCAN_VAR(vscroll);
		SCAN_VAR(nvram_storelatch);
	}

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bankswitch(bank_latch);
		M6502Close();
	}

	x2212_scan(nAction, pnMin); // <- yes, here!

	return 0;
}

// Crystal Castles (version 4)

static struct BurnRomInfo ccastlesRomDesc[] = {
	{ "136022-403.1k",	0x2000, 0x81471ae5, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136022-404.1l",	0x2000, 0x820daf29, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136022-405.1n",	0x2000, 0x4befc296, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastles)
STD_ROM_FN(ccastles)

struct BurnDriver BurnDrvCcastles = {
	"ccastles", NULL, NULL, NULL, "1983",
	"Crystal Castles (version 4)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastlesRomInfo, ccastlesRomName, NULL, NULL, NULL, NULL, CcastlesInputInfo, CcastlesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};

// Crystal Castles (joystick version)

static struct BurnRomInfo ccastlesjRomDesc[] = {
	{ "a000.12m",		0x2000, 0x0d911ef4, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "c000.13m",		0x2000, 0x246079de, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "e000.14m",		0x2000, 0x3beec4f3, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastlesj)
STD_ROM_FN(ccastlesj)

static INT32 CcastlesjInit()
{
	is_joyver = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvCcastlesj = {
	"ccastlesj", "ccastles", NULL, NULL, "1983",
	"Crystal Castles (joystick version)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastlesjRomInfo, ccastlesjRomName, NULL, NULL, NULL, NULL, CcastlesjInputInfo, CcastlesjDIPInfo,
	CcastlesjInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};

// Crystal Castles (version 3, German)

static struct BurnRomInfo ccastlesgRomDesc[] = {
	{ "136022-303.1k",	0x2000, 0x10e39fce, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136022-304.1l",	0x2000, 0x74510f72, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136022-112.1n",	0x2000, 0x69b8d906, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastlesg)
STD_ROM_FN(ccastlesg)

struct BurnDriver BurnDrvCcastlesg = {
	"ccastlesg", "ccastles", NULL, NULL, "1983",
	"Crystal Castles (version 3, German)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastlesgRomInfo, ccastlesgRomName, NULL, NULL, NULL, NULL, CcastlesInputInfo, CcastlesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};


// Crystal Castles (version 3, Spanish)

static struct BurnRomInfo ccastlespRomDesc[] = {
	{ "136022-303.1k",	0x2000, 0x10e39fce, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136022-304.1l",	0x2000, 0x74510f72, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136022-113.1n",	0x2000, 0xb833936e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastlesp)
STD_ROM_FN(ccastlesp)

struct BurnDriver BurnDrvCcastlesp = {
	"ccastlesp", "ccastles", NULL, NULL, "1983",
	"Crystal Castles (version 3, Spanish)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastlespRomInfo, ccastlespRomName, NULL, NULL, NULL, NULL, CcastlesInputInfo, CcastlesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};


// Crystal Castles (version 3, French)

static struct BurnRomInfo ccastlesfRomDesc[] = {
	{ "136022-303.1k",	0x2000, 0x10e39fce, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136022-304.1l",	0x2000, 0x74510f72, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136022-114.1n",	0x2000, 0x8585b4d1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastlesf)
STD_ROM_FN(ccastlesf)

struct BurnDriver BurnDrvCcastlesf = {
	"ccastlesf", "ccastles", NULL, NULL, "1983",
	"Crystal Castles (version 3, French)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastlesfRomInfo, ccastlesfRomName, NULL, NULL, NULL, NULL, CcastlesInputInfo, CcastlesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};


// Crystal Castles (version 3)

static struct BurnRomInfo ccastles3RomDesc[] = {
	{ "136022-303.1k",	0x2000, 0x10e39fce, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136022-304.1l",	0x2000, 0x74510f72, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136022-305.1n",	0x2000, 0x9418cf8a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastles3)
STD_ROM_FN(ccastles3)

struct BurnDriver BurnDrvCcastles3 = {
	"ccastles3", "ccastles", NULL, NULL, "1983",
	"Crystal Castles (version 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastles3RomInfo, ccastles3RomName, NULL, NULL, NULL, NULL, CcastlesInputInfo, CcastlesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};


// Crystal Castles (version 2)

static struct BurnRomInfo ccastles2RomDesc[] = {
	{ "136022-203.1k",	0x2000, 0x348a96f0, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136022-204.1l",	0x2000, 0xd48d8c1f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136022-205.1n",	0x2000, 0x0e4883cc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastles2)
STD_ROM_FN(ccastles2)

struct BurnDriver BurnDrvCcastles2 = {
	"ccastles2", "ccastles", NULL, NULL, "1983",
	"Crystal Castles (version 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastles2RomInfo, ccastles2RomName, NULL, NULL, NULL, NULL, CcastlesInputInfo, CcastlesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};


// Crystal Castles (version 1)

static struct BurnRomInfo ccastles1RomDesc[] = {
	{ "136022-103.1k",	0x2000, 0x9d10e314, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136022-104.1l",	0x2000, 0xfe2647a4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136022-105.1n",	0x2000, 0x5a13af07, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136022-102.1h",	0x2000, 0xf6ccfbd4, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136022-101.1f",	0x2000, 0xe2e17236, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136022-106.8d",	0x2000, 0x9d1d89fc, 2 | BRF_GRA },           //  5 gfx1
	{ "136022-107.8b",	0x2000, 0x39960b7d, 2 | BRF_GRA },           //  6

	{ "82s129-136022-108.7k",	0x0100, 0x6ed31e3b, 3 | BRF_GRA },           //  7 proms
	{ "82s129-136022-109.6l",	0x0100, 0xb3515f1a, 3 | BRF_GRA },           //  8
	{ "82s129-136022-110.11l",	0x0100, 0x068bdc7e, 3 | BRF_GRA },           //  9
	{ "82s129-136022-111.10k",	0x0100, 0xc29c18d9, 3 | BRF_GRA },           // 10
};

STD_ROM_PICK(ccastles1)
STD_ROM_FN(ccastles1)

struct BurnDriver BurnDrvCcastles1 = {
	"ccastles1", "ccastles", NULL, NULL, "1983",
	"Crystal Castles (version 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_ACTION, 0,
	NULL, ccastles1RomInfo, ccastles1RomName, NULL, NULL, NULL, NULL, CcastlesInputInfo, CcastlesDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvReRedraw, DrvScan, &DrvRecalc, 0x20,
	256, 232, 4, 3
};
