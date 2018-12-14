// FB Alpha Atari Skull and Crossbones driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarijsa.h"

#define DEBUG_TXT 0
#define dprintf(...) do { if (DEBUG_TXT) { bprintf(__VA_ARGS__); }} while (0)

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM;
static UINT8 *DrvPfRAM0;
static UINT8 *DrvPfRAM1;
static UINT8 *DrvMobRAM;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvAlphaRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nExtraCycles[2];

static INT32 scanline_int_state;
static INT32 video_int_state;
static INT32 cpu_halted;
static INT32 playfield_latch;
static INT32 scrollx;
static INT32 scrollx_raw;
static INT32 scrolly;
static INT32 scrolly_raw;
static INT32 mobank;

static INT32 do_scanline_irq = -1;

static INT32 vblank, hblank, scanline;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static UINT32 lastline;

static void partial_update();

static struct BurnInputInfo SkullxboInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 2"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 15,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 13,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Skullxbo)

static struct BurnDIPInfo SkullxboDIPList[]=
{
	{0x10, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x10, 0x01, 0x80, 0x80, "Off"			},
	{0x10, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Skullxbo)

static void update_interrupts()
{
	INT32 state = 0;
	if (scanline_int_state) state = 1;
	if (video_int_state) state = 2;
	if (atarijsa_int_state) state = 4;

	if (state)
		SekSetIRQLine(state, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void __fastcall skullxbo_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfff000) == 0xffd000) {
		*((UINT16*)(DrvMobRAM + (address & 0x0ffe))) = data;
		AtariMoWrite(0, (address & 0xfff) >> 1, data);
		return;
	}

	if ((address & 0xffe000) == 0xff8000) {
		*((UINT16*)(DrvPfRAM0 + (address & 0x1ffe))) = data;
		*((UINT16*)(DrvPfRAM1 + (address & 0x1ffe))) = (*((UINT16*)(DrvPfRAM1 + (address & 0x1ffe))) & 0xff00) | (playfield_latch);
		return;
	}

	if ((address & 0xfff800) == 0xff0000) {
		dprintf(0, _T("--moset bank.w %X. (line %d)\n"), address, scanline);
		//partial_update();
		atarimo_set_bank(0, (address >> 10) & 1);
		mobank = (address >> 10) & 1;
		return;
	}

	if ((address & 0xfffc00) == 0xff0800) {
		cpu_halted = 1; // until hblank
		return;
	}

	if ((address & 0xfffc00) == 0xff0c00) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfffc00) == 0xff1000) {
		video_int_state = 0;
		update_interrupts();
		return;
	}

	if ((address & 0xfffc00) == 0xff1400) {
		AtariJSAWrite(data & 0xff);
		return;
	}

	if ((address & 0xfffc00) == 0xff1800) {
		AtariJSAResetWrite(0);
		return;
	}

	if ((address & 0xfffd80) == 0xff1c00) {
		playfield_latch = data & 0xff;
		return;
	}

	if ((address & 0xff1d80) == 0xff1c80) {
		scrollx = (data >> 6) & ~1;
		if (scrollx_raw != data)
			partial_update();
		GenericTilemapSetScrollX(0, scrollx);
		atarimo_set_xscroll(0, scrollx);
		scrollx_raw = data;
		dprintf(0, _T("scrollX.w %d   (line: %d)\n"), scrollx, scanline);
		return;
	}

	if ((address & 0xfffd80) == 0xff1d00) {
		scanline_int_state = 0;
		update_interrupts();
		return;
	}

	if ((address & 0xfffd80) == 0xff1d80) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xfff800) == 0xff4000) {
		INT32 line = (scanline > nScreenHeight-1) ? 0 : scanline;

		scrolly = ((data >> 7) - (line)) & 0x1ff;
		if (scrolly_raw != data)
			partial_update();
		GenericTilemapSetScrollY(0, scrolly);
		atarimo_set_yscroll(0, scrolly);
		scrolly_raw = data;
		dprintf(0, _T("scrollY.w %d   (line: %d)\n"), scrolly, line);
		return;
	}

	if ((address & 0xfff800) == 0xff4800) {
		// mobwr_w (unused)
		return;
	}

	if (address < 0x80000) return; // NOP (romspace writes..)

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall skullxbo_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0xffd000) {
		DrvMobRAM[(address & 0x0fff)^1] = data;
		if (address&1)
			AtariMoWrite(0, (address & 0xfff) >> 1, *((UINT16*)(DrvMobRAM + (address & 0x0ffe))));
		return;
	}

	if ((address & 0xffe000) == 0xff8000) {
		DrvPfRAM0[(address & 0x1fff)^1] = data;
		*((UINT16*)(DrvPfRAM1 + (address & 0x1ffe))) = (*((UINT16*)(DrvPfRAM1 + (address & 0x1ffe))) & 0xff00) | (playfield_latch);
		return;
	}

	if ((address & 0xfff800) == 0xff0000) {
		dprintf(0, _T("--moset bank.b %X. (line %d)\n"), address, scanline);
		//partial_update();
		atarimo_set_bank(0, (address >> 10) & 1);
		mobank = (address >> 10) & 1;
		return;
	}

	if ((address & 0xfffc00) == 0xff0800) {
		cpu_halted = 1; // until hblank
		return;
	}

	if ((address & 0xfffc00) == 0xff0c00) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfffc00) == 0xff1000) {
		video_int_state = 0;
		update_interrupts();
		return;
	}

	if ((address & 0xfffc00) == 0xff1400) {
		AtariJSAWrite(data);
		return;
	}

	if ((address & 0xfffc00) == 0xff1800) {
		AtariJSAResetWrite(0);
		return;
	}

	if ((address & 0xfffd80) == 0xff1c00) {
		playfield_latch = data & 0xff;
		return;
	}

	if ((address & 0xff1d80) == 0xff1c80) {
		dprintf(0, _T("wb xscrollx??? %X\n"), data);
		// scrollx
		return;
	}

	if ((address & 0xfffd80) == 0xff1d00) {
		scanline_int_state = 0;
		update_interrupts();
		return;
	}

	if ((address & 0xfffd80) == 0xff1d80) {
		BurnWatchdogWrite();
		return;
	}

	if ((address & 0xfff800) == 0xff4000) {
		dprintf(0, _T("wb yscrolly??? %X\n"), data);
		INT32 line = (scanline > nScreenHeight) ? 0 : scanline;
		scrolly = (data >> 7) - line;
		return;
	}

	if ((address & 0xfff800) == 0xff4800) {
		// mobwr_w (unused)
		return;
	}
	bprintf (0, _T("WB: %5.5x, %4.4x\n"), address, data);
}

static inline UINT16 special_read()
{
	UINT16 ret = (DrvInputs[1] & ~0xb0) | (DrvDips[0] & 0x80);

	if (hblank) ret ^= 0x10;
	if (vblank) ret ^= 0x20;
	if (!atarigen_cpu_to_sound_ready) ret ^= 0x40;

	return ret;
}

static UINT16 __fastcall skullxbo_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0xff5000:
		case 0xff5001:
			return AtariJSARead();

		case 0xff5800:
		case 0xff5801:
			return DrvInputs[0];

		case 0xff5802:
		case 0xff5803:
			return special_read();
	}

	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall skullxbo_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0xff5000:
		case 0xff5001:
			return AtariJSARead() >> ((~address & 1) * 8);

		case 0xff5800:
		case 0xff5801:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0xff5802:
		case 0xff5803:
			return special_read() >> ((~address & 1) * 8);

		case 0xff1c81: // NOP
			return 0;
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + (offs * 2)));

	TILE_SET_INFO(2, data ^ 0x400, data >> 11, (data >> 15) ? TILE_OPAQUE : 0);
}

static tilemap_callback( bg )
{
	UINT16 data1 = *((UINT16*)(DrvPfRAM0 + (offs * 2)));
	UINT16 data2 = *((UINT16*)(DrvPfRAM1 + (offs * 2)));

	TILE_SET_INFO(1, data1, data2, TILE_FLIPYX(data1 >> 15));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	BurnWatchdogReset();
	AtariEEPROMReset();
	AtariJSAReset();

	cpu_halted = 0;
	video_int_state = 0;
	scanline_int_state = 0;
	playfield_latch = 0;
	do_scanline_irq = -1;
	scrollx = 0;
	scrolly = 0;

	nExtraCycles[0] = nExtraCycles[1] = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x080000;
	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROM2		= Next; Next += 0x040000;

	DrvSndROM		= Next; Next += 0x080000;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	DrvPalRAM		= Next; Next += 0x0010000;
	DrvPfRAM0		= Next; Next += 0x0020000;
	DrvPfRAM1		= Next; Next += 0x0020000;
	DrvMobRAM		= Next; Next += 0x0010000;
	DrvAlphaRAM		= Next; Next += 0x0020000;
	Drv68KRAM		= Next; Next += 0x0030000;

	atarimo_0_slipram	 = (UINT16*)(DrvAlphaRAM + 0xf80);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 200, 300
{
	INT32 Plane0[5]  = { 0x50000*8*4, 0x50000*8*3, 0x50000*8*2, 0x50000*8*1, 0x50000*8*0 };
	INT32 XOffs0[16] = { STEP16(0,1) };
	INT32 YOffs0[8]  = { STEP8(0,16) };

	INT32 Plane1[4]  = { STEP4(0,1) };
	INT32 XOffs1[16] = { 0x50000*8+0, 0x50000*8+0, 0x50000*8+ 4, 0x50000*8+ 4, 0, 0,  4,  4,
			    0x50000*8+8, 0x50000*8+8, 0x50000*8+12, 0x50000*8+12, 8, 8, 12, 12 };
	INT32 YOffs1[8]  = { STEP8(0,16) };

	INT32 Plane2[2]  = { 0, 1 };
	INT32 XOffs2[16] = { 0, 0, 2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 12, 12, 14, 14 };
	INT32 YOffs2[8]  = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x190000);

	GfxDecode(0x5000, 5, 16, 8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x0a0000; i++) tmp[i] = DrvGfxROM1[i] ^ 0xff;

	GfxDecode(0x5000, 4, 16, 8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x008000);

	GfxDecode(0x0800, 2, 16, 8, Plane2, XOffs2, YOffs2, 0x080, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	atarimo_desc modesc =
	{
		0,                  /* index to which gfx system */
		2,                  /* number of motion object banks */
		1,                  /* are the entries linked? */
		0,                  /* are the entries split? */
		0,                  /* render in reverse order? */
		0,                  /* render in swapped X/Y order? */
		0,                  /* does the neighbor bit affect the next object? */
		8,                  /* pixels per SLIP entry (0 for no-slip) */
		0,                  /* pixel offset for SLIPs */
		0,                  /* maximum number of links to visit/scanline (0=all) */

		0x000,              /* base palette entry */
		0x200,              /* maximum number of colors */
		0,                  /* transparent pen index */

		{{ 0x00ff,0,0,0 }}, /* mask for the link */
		{{ 0,0,0,0 }},            /* mask for the graphics bank */
		{{ 0,0x7fff,0,0 }}, /* mask for the code index */
		{{ 0,0,0,0 }},            /* mask for the upper code index */
		{{ 0,0,0x000f,0 }}, /* mask for the color */
		{{ 0,0,0xffc0,0 }}, /* mask for the X position */
		{{ 0,0,0,0xff80 }}, /* mask for the Y position */
		{{ 0,0,0,0x0070 }}, /* mask for the width, in tiles*/
		{{ 0,0,0,0x000f }}, /* mask for the height, in tiles */
		{{ 0,0x8000,0,0 }}, /* mask for the horizontal flip */
		{{ 0,0,0,0 }},            /* mask for the vertical flip */
		{{ 0,0,0x0030,0 }}, /* mask for the priority */
		{{ 0,0,0,0 }},            /* mask for the neighbor */
		{{ 0,0,0,0 }},            /* mask for absolute coordinates */

		{{ 0,0,0,0 }},            /* mask for the special value */
		0,                  /* resulting value to indicate "special" */
		0,                  /* callback routine for special entries */
	};

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x070001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x070000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x070000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x090000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0b0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0d0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0f0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x110000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x120000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x130000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x140000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x150000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x160000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x070000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x090000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x070000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0xff2000, 0xff2fff, MAP_RAM);
	SekMapMemory(DrvPfRAM0,			0xff8000, 0xff9fff, MAP_ROM);
	SekMapMemory(DrvPfRAM1,			0xffa000, 0xffbfff, MAP_RAM);
	SekMapMemory(DrvAlphaRAM,		0xffc000, 0xffcfff, MAP_RAM);
	SekMapMemory(DrvMobRAM,			0xffd000, 0xffdfff, MAP_ROM); // handler
	SekMapMemory(Drv68KRAM,			0xffe000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		skullxbo_main_write_word);
	SekSetWriteByteHandler(0,		skullxbo_main_write_byte);
	SekSetReadWordHandler(0,		skullxbo_main_read_word);
	SekSetReadByteHandler(0,		skullxbo_main_read_byte);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1,		0xff6000, 0xff6fff);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, DrvSndROM, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback,    16, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, alpha_map_callback, 16, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 5, 16, 8, 0x400000, 0x000, 0x1f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 8, 0x400000, 0x200, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM2, 2, 16, 8, 0x040000, 0x300, 0x0f);
	GenericTilemapSetTransparent(1, 0);

	AtariMoInit(0, &modesc);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	AtariJSAExit();
	AtariMoExit();
	AtariEEPROMExit();

	BurnFree(AllMem);

	return 0;
}

static struct atarimo_rect_list rectlist;

static void copy_sprites()
{
	//INT32 minx, maxx, miny, maxy;
	//GenericTilesGetClip(&minx, &maxx, &miny, &maxy);
	//dprintf(0, _T("numrecs: %d\n"), rectlist.numrects);

	for (INT32 r = 0; r < rectlist.numrects; r++, rectlist.rect++)
	{
		//bprintf(0, _T("miny: %d   maxy: %d\n"), rectlist.rect->min_y, rectlist.rect->max_y);
	    for (INT32 y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
		//for (INT32 y = miny; y < maxy; y++)
		{
			UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
			UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

			for (INT32 x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
			//for (INT32 x = minx; x < maxx; x++)
			{
				if (mo[x] != 0xffff)
				{
					int mopriority = mo[x] >> 12;
					int mopix = mo[x] & 0x1f;
					int pfcolor = (pf[x] >> 4) & 0x0f;
					int pfpix = pf[x] & 0x0f;
					int o17 = ((pf[x] & 0xc8) == 0xc8);

					if ((mopriority == 0 && !o17 && mopix >= 2) ||
						(mopriority == 1 && mopix >= 2 && !(pfcolor & 0x08)) ||
						((mopriority & 2) && mopix >= 2 && !(pfcolor & 0x0c)) ||
						(!(pfpix & 8) && mopix >= 2))
						pf[x] = mo[x] & ATARIMO_DATA_MASK;

					if ((mopriority == 0 && !o17 && mopix == 1) ||
						(mopriority == 1 && mopix == 1 && !(pfcolor & 0x08)) ||
						((mopriority & 2) && mopix == 1 && !(pfcolor & 0x0c)) ||
						(!(pfpix & 8) && mopix == 1))
						pf[x] |= 0x400;

					mo[x] = 0xffff;
				}
			}
		}
	}
}

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		AtariPaletteUpdateIRGB(DrvPalRAM, DrvPalette, 0x1000);
		DrvRecalc = 1; // force!!
	}

	dprintf(2, _T("             --  new frame  --\n"));

	lastline = 0;

	if (pBurnDraw) BurnTransferClear();
}

static void partial_update()
{
	if (!pBurnDraw) return;

	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;

	dprintf(0, _T("%07d: partial %d - %d.    scrollx %d   scrolly %d\n"), nCurrentFrame, lastline, scanline, scrollx, scrolly);

	GenericTilesSetClip(0, nScreenWidth, lastline, scanline);
	AtariMoRender(0, &rectlist);
	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nSpriteEnable & 1) copy_sprites();
	GenericTilesClearClip();

	GenericTilemapSetScrollY(0, scrolly);
	GenericTilemapSetScrollX(0, scrollx);
	atarimo_set_yscroll(0, scrolly&0x1ff);
	atarimo_set_xscroll(0, scrollx);

	lastline = scanline;
}

static INT32 DrvDraw()
{
	//scanline = 240; // end partial updates.
	partial_update();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static void skullxbo_scanline_update()
{
	INT32 offs = (scanline / 8) * 64 + 42;
	UINT16 *base = (UINT16*)(DrvAlphaRAM + offs);

	if (offs >= 0x7c0)
		return;

	if (scanline == 0)
	{
		dprintf(0, _T("scrollY_set.A at line 0: %d\n"), scrolly);

		GenericTilemapSetScrollY(0, scrolly);
		atarimo_set_yscroll(0, scrolly);
	}

	for (INT32 x = 42; x < 64; x++)
	{
		UINT16 data = *base++;
		UINT16 command = data & 0x000f;

		if (command == 0x0d)
		{
			if (scanline > 0) {
				//partial_update(); // causes trouble in cutscene-transitions
			}
			scrolly = ((data >> 7) - (scanline - 8)) & 0x1ff;
			dprintf(0, _T("scrollY_set.B at line %d: %d\n"), scanline, scrolly);
			scrolly_raw = data;

			if (scanline == 0 || scanline == 8) {
				GenericTilemapSetScrollY(0, scrolly);
				atarimo_set_yscroll(0, scrolly);
			}
			break;  // limit 1-scrolly per line. fixes weirdness in cutscene transition
		}
	}
}

static void scanline_callback()
{
	INT32 offset = (scanline / 8) * 64 + 42;
	UINT16 check = *((UINT16*)(DrvAlphaRAM + offset * 2));

	if (offset < 0x7c0 && (check & 0x8000))
	{
		do_scanline_irq = scanline + 6;
	}

	skullxbo_scanline_update();
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	M6502NewFrame();

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xff2f;
		DrvInputs[2] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2];
		atarijsa_test_mask = 0x80;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(7159090 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[2] = { nExtraCycles[0], nExtraCycles[1] };

	SekOpen(0);
	M6502Open(0);

	DrvDrawBegin();
	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;
		hblank = 0;

		if (scanline >= 0 && (scanline%8) == 0) {
			scanline_callback();
		}

		INT32 sek_line = ((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0];

		if (cpu_halted)
			nCyclesDone[0] += SekIdle((INT32)((double)sek_line * 0.9));//0.7368));
		else
			nCyclesDone[0] += SekRun((INT32)((double)sek_line * 0.9));//0.7368));

		nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

		hblank = 1;
		cpu_halted = 0;

		if (scanline >= 0 && scanline == do_scanline_irq) { // 6th
			dprintf(0, _T("-->scanline irq @ line %d.\n"), scanline);
			scanline+=2;
			partial_update();

			scanline_int_state = 1;
			update_interrupts();
			do_scanline_irq = -1;
		}
		// finish line
		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if (i == 240) {
			vblank = 1;

			video_int_state = 1;
			update_interrupts();

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && i&1) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 2);
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	SekClose();
	M6502Close();

	nExtraCycles[0] = nCyclesDone[0] - nCyclesTotal[0];
	nExtraCycles[1] = nCyclesDone[1] - nCyclesTotal[1];

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

		SekScan(nAction);

		AtariJSAScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(video_int_state);
		SCAN_VAR(scanline_int_state);
		SCAN_VAR(cpu_halted);

		SCAN_VAR(nExtraCycles);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Skull & Crossbones (rev 5)

static struct BurnRomInfo skullxboRomDesc[] = {
	{ "136072-5150.228a",	0x10000, 0x9546d88b, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "136072-5151.228c",	0x10000, 0xb9ed8bd4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136072-5152.213a",	0x10000, 0xc07e44fc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136072-5153.213c",	0x10000, 0xfef8297f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136072-1154.200a",	0x10000, 0xde4101a3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136072-1155.200c",	0x10000, 0x78c0f6ad, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136072-1156.185a",	0x08000, 0xcde16b55, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136072-1157.185c",	0x08000, 0x31c77376, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136072-1149.1b",		0x10000, 0x8d730e7a, 2 | BRF_PRG | BRF_ESS }, //  8 M6502 Code

	{ "136072-1102.13r",	0x10000, 0x90becdfa, 3 | BRF_GRA },           //  9 Sprites
	{ "136072-1104.28r",	0x10000, 0x33609071, 3 | BRF_GRA },           // 10
	{ "136072-1106.41r",	0x10000, 0x71962e9f, 3 | BRF_GRA },           // 11
	{ "136072-1101.13p",	0x10000, 0x4d41701e, 3 | BRF_GRA },           // 12
	{ "136072-1103.28p",	0x10000, 0x3011da3b, 3 | BRF_GRA },           // 13
	{ "136072-1108.53r",	0x10000, 0x386c7edc, 3 | BRF_GRA },           // 14
	{ "136072-1110.67r",	0x10000, 0xa54d16e6, 3 | BRF_GRA },           // 15
	{ "136072-1112.81r",	0x10000, 0x669411f6, 3 | BRF_GRA },           // 16
	{ "136072-1107.53p",	0x10000, 0xcaaeb57a, 3 | BRF_GRA },           // 17
	{ "136072-1109.67p",	0x10000, 0x61cb4e28, 3 | BRF_GRA },           // 18
	{ "136072-1114.95r",	0x10000, 0xe340d5a1, 3 | BRF_GRA },           // 19
	{ "136072-1116.109r",	0x10000, 0xf25b8aca, 3 | BRF_GRA },           // 20
	{ "136072-1118.123r",	0x10000, 0x8cf73585, 3 | BRF_GRA },           // 21
	{ "136072-1113.95p",	0x10000, 0x899b59af, 3 | BRF_GRA },           // 22
	{ "136072-1115.109p",	0x10000, 0xcf4fd19a, 3 | BRF_GRA },           // 23
	{ "136072-1120.137r",	0x10000, 0xfde7c03d, 3 | BRF_GRA },           // 24
	{ "136072-1122.151r",	0x10000, 0x6ff6a9f2, 3 | BRF_GRA },           // 25
	{ "136072-1124.165r",	0x10000, 0xf11909f1, 3 | BRF_GRA },           // 26
	{ "136072-1119.137p",	0x10000, 0x6f8003a1, 3 | BRF_GRA },           // 27
	{ "136072-1121.151p",	0x10000, 0x8ff0a1ec, 3 | BRF_GRA },           // 28
	{ "136072-1125.123n",	0x10000, 0x3aa7c756, 3 | BRF_GRA },           // 29
	{ "136072-1126.137n",	0x10000, 0xcb82c9aa, 3 | BRF_GRA },           // 30
	{ "136072-1128.151n",	0x10000, 0xdce32863, 3 | BRF_GRA },           // 31

	{ "136072-2129.180p",	0x10000, 0x36b1a578, 4 | BRF_GRA },           // 32 Background Tiles
	{ "136072-2131.193p",	0x10000, 0x7b7c04a1, 4 | BRF_GRA },           // 33
	{ "136072-2133.208p",	0x10000, 0xe03fe4d9, 4 | BRF_GRA },           // 34
	{ "136072-2135.221p",	0x10000, 0x7d497110, 4 | BRF_GRA },           // 35
	{ "136072-2137.235p",	0x10000, 0xf91e7872, 4 | BRF_GRA },           // 36
	{ "136072-2130.180r",	0x10000, 0xb25368cc, 4 | BRF_GRA },           // 37
	{ "136072-2132.193r",	0x10000, 0x112f2d20, 4 | BRF_GRA },           // 38
	{ "136072-2134.208r",	0x10000, 0x84884ed6, 4 | BRF_GRA },           // 39
	{ "136072-2136.221r",	0x10000, 0xbc028690, 4 | BRF_GRA },           // 40
	{ "136072-2138.235r",	0x10000, 0x60cec955, 4 | BRF_GRA },           // 41

	{ "136072-2141.250k",	0x08000, 0x60d6d6df, 5 | BRF_GRA },           // 42 Alpha Tiles

	{ "136072-1145.7k",		0x10000, 0xd9475d58, 6 | BRF_GRA },           // 43 Samples
	{ "136072-1146.7j",		0x10000, 0x133e6aef, 6 | BRF_GRA },           // 44
	{ "136072-1147.7e",		0x10000, 0xba4d556e, 6 | BRF_GRA },           // 45
	{ "136072-1148.7d",		0x10000, 0xc48df49a, 6 | BRF_GRA },           // 46
};

STD_ROM_PICK(skullxbo)
STD_ROM_FN(skullxbo)

struct BurnDriver BurnDrvSkullxbo = {
	"skullxbo", NULL, NULL, NULL, "1989",
	"Skull & Crossbones (rev 5)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, skullxboRomInfo, skullxboRomName, NULL, NULL, NULL, NULL, SkullxboInputInfo, SkullxboDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	672, 240, 4, 3
};
