
/*
    version .00001a ;)


	no attempt at speed-ups
	no attempt at cleaning at all
	ROM sets might be out of date!!!

	working

	ARABIANM
	kaiserkn
	RECALH
	RIDINGF
	LIGHTBR
	RINGRAGE
	LANDMAKER & proto
	DARIUSG
	PUCHICAR
	PBOBBLE2 & clones
	pbobble3 
	pbobble4
	bublbob2
	bublblem
	gekiridn
	popnpop
	twinqix
	trstar
	spcinv95
	spcinvdj
	kirameki 	- has extra 68k rom that is banked for sound, not hooked up yet,
	qtheater
	quizhuhu	- missing text is normal.
	elavctrl
	cleopatr
	intcup94 / hthero94 ok
	pwrgoal
	gunlock
	GSEEKER
	arkretrn
	tcobra2
	cupfinal    - ok (clone of scfinals!)
	commandw

	broken

	scfinals    - coin inputs don't work, service coin ok.
*/


#include "tiles_generic.h"
#include "m68000_intf.h"
#include "taito.h"
#include "taitof3_snd.h"
#include "es5506.h"
#include "eeprom.h"

//#define DO_LOG

static UINT8 *DrvPfRAM;
static UINT8 *DrvVRAMRAM;
static UINT8 *DrvLineRAM;
static UINT8 *DrvPivotRAM;
static UINT8 *DrvCtrlRAM;
static UINT16 *DrvCoinWord;

static UINT32 *output_bitmap;

static UINT8 DrvRecalc;

static INT32 tileno_mask;

static INT32 watchdog;
static INT32 extended_layers;
#define SPRITE_KLUDGE	1
static INT32 f3_kludge=0;
static void (*pPaletteUpdateCallback)(UINT16) = NULL;

static UINT16 *bitmap_layer[10];
static UINT8 *bitmap_flags[10];
static INT32 bitmap_width[8];

static UINT8 *m_tile_opaque_pf[8];
static UINT8 *dirty_tiles;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvSrv[1];
static UINT8 DrvReset;
static UINT16 DrvInputs[5];
static UINT8 previous_coin;

static UINT32 sprite_code_mask; // set in init
static INT32 sprite_lag; // set in init

static UINT32 m_sprite_pen_mask;
static UINT32 m_sprite_pri_usage;
static INT32 m_flipscreen = 0;
static INT32 m_sprite_extra_planes = 0;
static INT32 sound_cpu_in_reset = 0;

enum {
	/* Early F3 class games, these are not cartridge games and system features may be different */
	RINGRAGE=0, /* D21 */
	ARABIANM,   /* D29 */
	RIDINGF,    /* D34 */
	GSEEKER,    /* D40 */
	TRSTAR,     /* D53 */
	GUNLOCK,    /* D66 */
	TWINQIX,
	UNDRFIRE,   /* D67 - Heavily modified F3 hardware (different memory map) */
	SCFINALS,
	LIGHTBR,    /* D69 */

	/* D77 - F3 motherboard proms, all following games are 'F3 package system' */
	/* D78 I CUP */
	KAISERKN,   /* D84 */
	DARIUSG,    /* D87 */
	BUBSYMPH,   /* D90 */
	SPCINVDX,   /* D93 */
	HTHERO95,   /* D94 */
	QTHEATER,   /* D95 */
	EACTION2,   /* E02 */
	SPCINV95,   /* E06 */
	QUIZHUHU,   /* E08 */
	PBOBBLE2,   /* E10 */
	GEKIRIDO,   /* E11 */
	KTIGER2,    /* E15 */
	BUBBLEM,    /* E21 */
	CLEOPATR,   /* E28 */
	PBOBBLE3,   /* E29 */
	ARKRETRN,   /* E36 */
	KIRAMEKI,   /* E44 */
	PUCHICAR,   /* E46 */
	PBOBBLE4,   /* E49 */
	POPNPOP,    /* E51 */
	LANDMAKR,   /* E61 */
	RECALH,     /* prototype */
	COMMANDW,   /* prototype */
	TMDRILL
};

static INT32 m_f3_game = 0;

struct tempsprite
{
	INT32 code, color;
	INT32 flipx, flipy;
	INT32 x, y;
	INT32 zoomx, zoomy;
	INT32 pri;
};

struct tempsprite *m_spritelist;
const struct tempsprite *m_sprite_end;

static INT32 min_x = 0;
static INT32 max_x = 512;
static INT32 min_y = 0;
static INT32 max_y = 256;

void TaitoF3VideoInit();

static struct BurnInputInfo F3InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 fire 4"},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy5 + 6,	"p3 coin"},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 14,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy4 + 0,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy4 + 1,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy4 + 2,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 3,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy3 + 9,	"p3 fire 2"},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy3 + 10,	"p3 fire 3"},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy3 + 11,	"p3 fire 4"},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy5 + 7,	"p4 coin"},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy4 + 4,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy4 + 5,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy4 + 6,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 7,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p4 fire 2"},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy3 + 14,	"p4 fire 3"},
	{"P4 Button 4",		BIT_DIGITAL,	DrvJoy3 + 15,	"p4 fire 4"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"service"},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"service"},
	{"Service 3",		BIT_DIGITAL,	DrvJoy1 + 11,	"service"},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 8,	"tilt"},
};

STDINPUTINFO(F3)

static struct BurnInputInfo KnInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy5 + 4,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 4"},
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 5"},
	{"P1 Button 6",		BIT_DIGITAL,	DrvJoy4 + 2,	"p1 fire 6"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy5 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 fire 3"},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 4"},
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 5"},
	{"P2 Button 6",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 fire 6"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"service"},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"service"},
	{"Service 3",		BIT_DIGITAL,	DrvJoy1 + 11,	"service"},
	{"Service Mode",	BIT_DIGITAL,    DrvSrv + 0,     "diag"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 8,	"tilt"},
};

STDINPUTINFO(Kn)

static void control_w(INT32 offset, UINT32 d, INT32 b)
{
	switch (offset & 0x1c)
	{
		case 0x00:
			watchdog = 0;
		return;

		case 0x04:
		{
	//		bprintf (0, _T("contrl %2.2x, %8.8x, %8.8x\n"), offset, d, b);
			if ((offset & 3) == 0) DrvCoinWord[0] = d << 0; // or 8?
		}
		return;

		case 0x10:
		{
			if ((offset & 3) == 3 || (offset == 0x4a0012 && b == 2)) {
				EEPROMSetClockLine((d & 0x08) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
				EEPROMWriteBit(d & 0x04);
				EEPROMSetCSLine((d & 0x10) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
			}
		}	
		return;

		case 0x14:
		{
	//		bprintf (0, _T("contrl %2.2x, %8.8x, %8.8x\n"), offset, d, b);
			if ((offset & 3) == 0) DrvCoinWord[1] = d << 0; // or 8?
		}

		return;
	}
}

static void __fastcall f3_main_write_long(UINT32 a, UINT32 d)
{
#ifdef DO_LOG
//	bprintf (0, _T("WL: %5.5x, %8.8x\n"), a, d);
#endif
	if ((a & 0xffff80) == 0x300000) {
		// sound_bankswitch_w
		return;
	}

	if ((a & 0xffffe0) == 0x4a0000) {
		control_w(a, d,4);
		return;
	}

	if ((a & 0xfffffc) == 0xc80000) {
		sound_cpu_in_reset = 0;
		// sound_reset_0_w
		return;
	}

	if ((a & 0xfffffc) == 0xc80100) {
		SekClose();
		SekOpen(1);
		SekReset();
		SekClose();
		SekOpen(0);
		sound_cpu_in_reset = 1;
		return;
	}
}

static void __fastcall f3_main_write_word(UINT32 a, UINT16 d)
{
#ifdef DO_LOG
//	if (a != 0x4a0000)bprintf (0, _T("WW: %5.5x, %4.4x\n"), a, d);
#endif
	if ((a & 0xffff80) == 0x300000) {
		// sound_bankswitch_w
		return;
	}

	if ((a & 0xffffe0) == 0x4a0000) {
		control_w(a, d,2);
		return;
	}

	if ((a & 0xfffffc) == 0x4c0000) {
		// unk
		return;
	}

	if ((a & 0xfffffc) == 0xc80000) {
		sound_cpu_in_reset = 0;
		return;
	}

	if ((a & 0xfffffc) == 0xc80100) {
		SekClose();
		SekOpen(1);
		SekReset();
		SekClose();
		SekOpen(0);
		sound_cpu_in_reset = 1;
		return;
	}
}

static void __fastcall f3_main_write_byte(UINT32 a, UINT8 d)
{
#ifdef DO_LOG
//	if (a != 0x4a0013 && a != 0x4a0000) bprintf (0, _T("WB: %5.5x, %2.2x\n"), a, d);
#endif
	if ((a & 0xffff80) == 0x300000) {
		// sound_bankswitch_w
		return;
	}

	if ((a & 0xffffe0) == 0x4a0000) {
		control_w(a, d,1);
		return;
	}

	if ((a & 0xfffffc) == 0xc80000) {
		sound_cpu_in_reset = 0;
		return;
	}

	if ((a & 0xfffffc) == 0xc80100) {
		SekClose();
		SekOpen(1);
		SekReset();
		SekClose();
		SekOpen(0);
		sound_cpu_in_reset = 1;
		return;
	}
}

static UINT32 control_r(INT32 offset, INT32 )
{
	offset &= 0x1c;

	UINT32 ret = 0xffffffff;

	switch (offset & 0x1c)
	{
		case 0x00:
			ret  = DrvInputs[0] & 0xffff;
			ret |= EEPROMRead() ? 0x01010000 : 0;
			ret |= (DrvInputs[4] & 0xfe) << 16;
			ret |= (DrvInputs[4] & 0xfe) << 24;
		break;

		case 0x04:
			ret = DrvInputs[1] | (DrvCoinWord[0] << 16);
		break;

		case 0x08:
		case 0x0c:
			ret = ~0; // and
		break;

		case 0x10:
			ret = DrvInputs[2] | 0xffff0000;
		break;

		case 0x14:
			ret = DrvInputs[3] | (DrvCoinWord[1] << 16);
		break;
	}

	return ret;
}

static UINT32 __fastcall f3_main_read_long(UINT32 a)
{

#ifdef DO_LOG
	bprintf (0, _T("RL: %5.5x pc: %5.5x\n"), a, SekGetPC(-1));
#endif

	if ((a & 0xffffe0) == 0x4a0000) {
		return control_r(a, 4);
	}

	return 0;
}

static UINT16 __fastcall f3_main_read_word(UINT32 a)
{
#ifdef DO_LOG
	bprintf (0, _T("RW: %5.5x pc: %5.5x\n"), a, SekGetPC(-1));
#endif

	if ((a & 0xffffe0) == 0x4a0000) {
		return control_r(a, 2) >> ((~a & 2) * 8);
	}

	return 0;
}

static UINT8 __fastcall f3_main_read_byte(UINT32 a)
{
#ifdef DO_LOG
	if (a != 0x4a0001) bprintf (0, _T("RB: %5.5x pc: %5.5x\n"), a, SekGetPC(-1));
#endif

	if ((a & 0xffffe0) == 0x4a0000) {
		return control_r(a, 1) >> (((a & 3)^3) * 8);
	}

	return 0;
}

static void __fastcall f3_palette_write_long(UINT32 a, UINT32 d)
{
	if ((a & 0xff8000) == 0x440000) {
		*((UINT32*)(TaitoPaletteRam + (a & 0x7ffc))) = (d << 16) | (d >> 16);
		pPaletteUpdateCallback(a);
		return;
	}
}

static void __fastcall f3_palette_write_word(UINT32 a, UINT16 d)
{
	if ((a & 0xff8000) == 0x440000) {
		*((UINT16*)(TaitoPaletteRam + (a & 0x7ffe))) = d;
		pPaletteUpdateCallback(a);
		return;
	}
}

static void __fastcall f3_palette_write_byte(UINT32 a, UINT8 d)
{
	if ((a & 0xff8000) == 0x440000) {
		TaitoPaletteRam[(a & 0x7fff) ^ 1] = d;
		pPaletteUpdateCallback(a);
		return;
	}
}

static void DrvVRAMExpand(UINT16 offset)
{
	offset &= 0x1ffc;

	TaitoCharsB[offset * 2 + 1] = DrvVRAMRAM[offset + 2] >> 4;
	TaitoCharsB[offset * 2 + 0] = DrvVRAMRAM[offset + 2] & 0x0f;
	TaitoCharsB[offset * 2 + 3] = DrvVRAMRAM[offset + 3] >> 4;
	TaitoCharsB[offset * 2 + 2] = DrvVRAMRAM[offset + 3] & 0x0f;
	TaitoCharsB[offset * 2 + 5] = DrvVRAMRAM[offset + 0] >> 4;
	TaitoCharsB[offset * 2 + 4] = DrvVRAMRAM[offset + 0] & 0x0f;
	TaitoCharsB[offset * 2 + 7] = DrvVRAMRAM[offset + 1] >> 4;
	TaitoCharsB[offset * 2 + 6] = DrvVRAMRAM[offset + 1] & 0x0f;
}

static void DrvPivotExpand(UINT16 offset)
{
	offset &= 0xfffc;

	TaitoCharsPivot[offset * 2 + 1] = DrvPivotRAM[offset + 2] >> 4;
	TaitoCharsPivot[offset * 2 + 0] = DrvPivotRAM[offset + 2] & 0x0f;
	TaitoCharsPivot[offset * 2 + 3] = DrvPivotRAM[offset + 3] >> 4;
	TaitoCharsPivot[offset * 2 + 2] = DrvPivotRAM[offset + 3] & 0x0f;
	TaitoCharsPivot[offset * 2 + 5] = DrvPivotRAM[offset + 0] >> 4;
	TaitoCharsPivot[offset * 2 + 4] = DrvPivotRAM[offset + 0] & 0x0f;
	TaitoCharsPivot[offset * 2 + 7] = DrvPivotRAM[offset + 1] >> 4;
	TaitoCharsPivot[offset * 2 + 6] = DrvPivotRAM[offset + 1] & 0x0f;
}

static void __fastcall f3_VRAM_write_long(UINT32 a, UINT32 d)
{
	if ((a & 0xffe000) == 0x61e000) {
		*((UINT32*)(DrvVRAMRAM + (a & 0x1ffc))) = (d << 16) | (d >> 16);
		DrvVRAMExpand(a);
		return;
	}
}

static void __fastcall f3_VRAM_write_word(UINT32 a, UINT16 d)
{
	if ((a & 0xffe000) == 0x61e000) {
		*((UINT16*)(DrvVRAMRAM + (a & 0x1ffe))) = d;
		DrvVRAMExpand(a);
		return;
	}
}

static void __fastcall f3_VRAM_write_byte(UINT32 a, UINT8 d)
{
	if ((a & 0xffe000) == 0x61e000) {
		DrvVRAMRAM[(a & 0x1fff) ^ 1] = d;
		DrvVRAMExpand(a);
		return;
	}
}

static void __fastcall f3_pivot_write_long(UINT32 a, UINT32 d)
{
	if ((a & 0xff0000) == 0x630000) {
		*((UINT32*)(DrvPivotRAM + (a & 0xfffc))) = (d << 16) | (d >> 16);
		DrvPivotExpand(a);
		return;
	}
}

static void __fastcall f3_pivot_write_word(UINT32 a, UINT16 d)
{
	if ((a & 0xff0000) == 0x630000) {
		*((UINT16*)(DrvPivotRAM + (a & 0xfffe))) = d;
		DrvPivotExpand(a);
		return;
	}
}

static void __fastcall f3_pivot_write_byte(UINT32 a, UINT8 d)
{
	if ((a & 0xff0000) == 0x630000) {
		DrvPivotRAM[(a & 0xffff) ^ 1] = d;
		DrvPivotExpand(a);
		return;
	}
}

static void __fastcall f3_playfield_write_long(UINT32 a, UINT32 d)
{
	if ((a & 0xff8000) == 0x610000) {
		UINT32 *ram = (UINT32*)(DrvPfRAM + (a & 0x7ffc));

		if (ram[0] != ((d << 16) | (d >> 16))) {
			ram[0] = (d << 16) | (d >> 16);
			dirty_tiles[(a & 0x7ffc)/4] = 1;
		}
		return;
	}
}

static void __fastcall f3_playfield_write_word(UINT32 a, UINT16 d)
{
	if ((a & 0xff8000) == 0x610000) {
		UINT16 *ram = (UINT16*)(DrvPfRAM + (a & 0x7ffe));

		if (ram[0] != d) {
			ram[0] = d;
			dirty_tiles[(a & 0x7ffc)/4] = 1;
		}
		return;
	}
}

static void __fastcall f3_playfield_write_byte(UINT32 a, UINT8 d)
{
	if ((a & 0xff8000) == 0x610000) {
		if (DrvPfRAM[(a & 0x7fff) ^ 1] != d) {
			DrvPfRAM[(a & 0x7fff) ^ 1] = d;
			dirty_tiles[(a & 0x7ffc)/4] = 1;
		}
		return;
	}
}

static void f3_reset_dirtybuffer()
{
	memset (dirty_tiles, 1, 0x8000/4);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (TaitoRamStart, 0, TaitoRamEnd - TaitoRamStart);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	TaitoF3SoundReset();

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		if (TaitoDefaultEEProm[0] != 0) {
			EEPROMFill((const UINT8*)TaitoDefaultEEProm, 0, 128);
		} else if (m_f3_game == RECALH || m_f3_game == GSEEKER ) {
			static const UINT8 recalh_eeprom[128] =	{
				0x85,0x54,0x00,0x00,0x30,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf3,0x35,
				0x00,0x01,0x86,0xa0,0x00,0x13,0x04,0x13,0x00,0x00,0xc3,0x50,0x00,0x19,0x00,0x0a,
				0x00,0x00,0x4e,0x20,0x00,0x03,0x18,0x0d,0x00,0x00,0x27,0x10,0x00,0x05,0x14,0x18,
				0x00,0x00,0x13,0x88,0x00,0x00,0x12,0x27,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
			};

			EEPROMFill(recalh_eeprom, 0, 128);
		} else if (m_f3_game == ARKRETRN) {
			static const UINT8 arkretrn_eeprom[128] = {
				0x54,0x41,0x49,0x54,0x4f,0x03,0x00,0x07,0xa1,0xe8,0xe0,0x01,0x11,0x12,0x30,0x00,
				0x00,0x00,0x02,0x02,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4c,0x63,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
			};
			static const UINT8 arkretrnj_eeprom[128] = {
				0x54,0x41,0x49,0x54,0x4f,0x01,0x00,0x07,0xa1,0xe8,0xe0,0x01,0x11,0x11,0x30,0x00,
				0x00,0x00,0x02,0x02,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4c,0x66,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
			};
			static const UINT8 arkretrnu_eeprom[128] = {
				0x54,0x41,0x49,0x54,0x4f,0x02,0x00,0x07,0xa1,0xe8,0xe0,0x01,0x11,0x11,0x30,0x00,
				0x00,0x00,0x02,0x02,0x02,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4c,0x65,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
			};

			if (strstr(BurnDrvGetTextA(DRV_NAME), "arkretrnu")) {
				EEPROMFill(arkretrnu_eeprom, 0, 128);
			} else if (strstr(BurnDrvGetTextA(DRV_NAME), "arkretrnj")) {
				EEPROMFill(arkretrnj_eeprom, 0, 128);
			} else {
				EEPROMFill(arkretrn_eeprom, 0, 128);
			}
		} else if (m_f3_game == PUCHICAR) {
			static const UINT8 puchicar_eeprom[128] = {
				0x54,0x41,0x49,0x54,0x4f,0x03,0x00,0x07,0xa1,0xf2,0xe0,0x01,0x11,0x12,0x30,0x00,
				0x00,0x00,0x02,0x02,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4d,0x59,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
			};
			static const UINT8 puchicarj_eeprom[128] = {
				0x54,0x41,0x49,0x54,0x4f,0x01,0x00,0x07,0xa1,0xf2,0xe0,0x01,0x11,0x11,0x30,0x00,
				0x00,0x00,0x02,0x02,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4d,0x5c,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
				0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
			};
			if (strstr(BurnDrvGetTextA(DRV_NAME), "puchicarj")) {
				EEPROMFill(puchicarj_eeprom, 0, 128);
			} else {
				EEPROMFill(puchicar_eeprom, 0, 128);
			}
		}
	}

	f3_reset_dirtybuffer();

	m_sprite_pen_mask = 0;
	m_sprite_pri_usage = 0;
	m_flipscreen = 0;
	m_sprite_extra_planes = 0;

	sound_cpu_in_reset = 1;
	watchdog = 0;
	previous_coin = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1		= Next; Next += 0x0200000;

	TaitoF3SoundRom		= Next;
	Taito68KRom2		= Next; Next += 0x0100000;
	if (m_f3_game == KIRAMEKI) Next += 0x200000;

	TaitoSpritesA		= Next; Next += TaitoSpriteARomSize;
	TaitoChars		= Next; Next += TaitoCharRomSize;

	m_tile_opaque_pf[0]	= Next; Next += TaitoCharRomSize / 0x100;
	m_tile_opaque_pf[1]	= Next; Next += TaitoCharRomSize / 0x100;
	m_tile_opaque_pf[2]	= Next; Next += TaitoCharRomSize / 0x100;
	m_tile_opaque_pf[3]	= Next; Next += TaitoCharRomSize / 0x100;
	m_tile_opaque_pf[4]	= Next; Next += TaitoCharRomSize / 0x100;
	m_tile_opaque_pf[5]	= Next; Next += TaitoCharRomSize / 0x100;
	m_tile_opaque_pf[6]	= Next; Next += TaitoCharRomSize / 0x100;
	m_tile_opaque_pf[7]	= Next; Next += TaitoCharRomSize / 0x100;

	TaitoF3ES5506Rom	= Next;
	TaitoES5505Rom		= Next; Next += TaitoF3ES5506RomSize;

	TaitoDefaultEEProm	= Next; Next += 0x0000080;

	pBurnDrvPalette		= (UINT32 *)Next;
	TaitoPalette		= (UINT32 *)Next; Next += 0x0002000 * sizeof (UINT32);

	TaitoCharsB		= Next; Next += 0x0004000;
	TaitoCharsPivot		= Next; Next += 0x0020000;

	TaitoRamStart		= Next;

	Taito68KRam1		= Next; Next += 0x0020000;
	TaitoPaletteRam		= Next; Next += 0x0008000;
	TaitoSpriteRam		= Next; Next += 0x0010000;
	TaitoSpriteRamDelayed	= Next; Next += 0x0010000;
	TaitoSpriteRamDelayed2	= Next; Next += 0x0010000;
	DrvPfRAM		= Next; Next += 0x000c000;
	TaitoVideoRam		= Next; Next += 0x0002000;
	DrvVRAMRAM		= Next; Next += 0x0002000;
	DrvLineRAM		= Next; Next += 0x0010000;
	DrvPivotRAM		= Next; Next += 0x0010000;
	DrvCtrlRAM		= Next; Next += 0x0000400;

	DrvCoinWord		= (UINT16*)Next; Next += 2 * sizeof(INT16);

	TaitoF3SoundRam		= Next; Next += 0x0010000;
	TaitoF3SharedRam	= Next; Next += 0x0000800;
	TaitoES5510DSPRam	= Next; Next += 0x0000200;

	TaitoES5510GPR		= (UINT32*)Next; Next += 0x0000c0 * sizeof(UINT32);
	TaitoES5510DRAM		= (UINT16*)Next; Next += 0x200000 * sizeof(UINT16);

	TaitoRamEnd		= Next;

	output_bitmap		= (UINT32*)Next; Next += 512 * 512 * sizeof(INT32);

	TaitoPriorityMap	= Next; Next += 1024 * 512;

	bitmap_layer[0] 	= (UINT16*)Next; Next += 1024 * 512 * sizeof(INT16);
	bitmap_layer[1] 	= (UINT16*)Next; Next += 1024 * 512 * sizeof(INT16);
	bitmap_layer[2] 	= (UINT16*)Next; Next += 1024 * 512 * sizeof(INT16);
	bitmap_layer[3] 	= (UINT16*)Next; Next += 1024 * 512 * sizeof(INT16);

	bitmap_layer[4] 	= (UINT16*)Next; Next += 512 * 512 * sizeof(INT16);
	bitmap_layer[5] 	= (UINT16*)Next; Next += 512 * 512 * sizeof(INT16);
	bitmap_layer[6] 	= (UINT16*)Next; Next += 512 * 512 * sizeof(INT16);
	bitmap_layer[7] 	= (UINT16*)Next; Next += 512 * 512 * sizeof(INT16);

	bitmap_layer[8] 	= (UINT16*)Next; Next += 512 * 512 * sizeof(INT16);
	bitmap_layer[9] 	= (UINT16*)Next; Next += 512 * 256 * sizeof(INT16);

	bitmap_flags[0]		= Next; Next += 1024 * 512;
	bitmap_flags[1]		= Next; Next += 1024 * 512;
	bitmap_flags[2]		= Next; Next += 1024 * 512;
	bitmap_flags[3]		= Next; Next += 1024 * 512;
	bitmap_flags[4]		= Next; Next += 512 * 512;
	bitmap_flags[5]		= Next; Next += 512 * 512;
	bitmap_flags[6]		= Next; Next += 512 * 512;
	bitmap_flags[7]		= Next; Next += 512 * 512;
	bitmap_flags[8]		= Next; Next += 512 * 512;
	bitmap_flags[9]		= Next; Next += 512 * 256;

	dirty_tiles		= Next; Next += 0x8000 / 4;

	TaitoMemEnd		= Next;

	return 0;
}

static void DrvCalculateTransTable(INT32 len)
{
	UINT8 *pf_gfx = TaitoChars;
	int c;

	for (c = 0;c < len/0x100;c++)
	{
		int x,y;
		int extra_planes; /* 0 = 4bpp, 1=5bpp, 2=?, 3=6bpp */

		for (extra_planes=0; extra_planes<4; extra_planes++)
		{
			int chk_trans_or_opa=0;
			UINT8 extra_mask = ((extra_planes << 4) | 0x0f);
			const UINT8 *dp = pf_gfx + c * 0x100; //pf_gfx->get_data(c);

			for (y = 0;y < 16;y++)
			{
				for (x = 0;x < 16;x++)
				{
					if(!(dp[x] & extra_mask))
						chk_trans_or_opa|=2;
					else
						chk_trans_or_opa|=1;
				}
				dp += 16;
			}

			m_tile_opaque_pf[extra_planes][c]=chk_trans_or_opa;
		}
	}
}

static void DrvGfxDecode(INT32 spr_len, INT32 tile_len)
{
	INT32 Plane0[6]  = { (spr_len * 4)+0, (spr_len * 4)+1, 0, 1, 2, 3 }; // sprite
	INT32 Plane1[6]  = { (tile_len * 4)+2, (tile_len * 4)+3, 0, 1, 2, 3 }; // tile
	INT32 XOffs0[16] = { 4, 0, 12, 8, 16+4, 16+0, 16+12, 16+8, 32+4, 32+0, 32+12, 32+8, 48+4, 48+0, 48+12, 48+8 };
	INT32 XOffs1[16] = { 4, 0, 16+4, 16+0, 8+4, 8+0, 24+4, 24+0, 32+4, 32+0, 48+4, 48+0, 40+4, 40+0, 56+4, 56+0 };
	INT32 YOffs[16] = { STEP16(0, 64) };

	UINT8 *tmp = (UINT8*)BurnMalloc((spr_len > tile_len) ? spr_len : tile_len);

	memcpy (tmp, TaitoSpritesA, spr_len);

	GfxDecode((spr_len / 0x100),  6, 16, 16, Plane0, XOffs0, YOffs, 0x400, tmp, TaitoSpritesA);

	memcpy (tmp, TaitoChars, tile_len);

	GfxDecode((tile_len / 0x100), 6, 16, 16, Plane1, XOffs1, YOffs, 0x400, tmp, TaitoChars);

	sprite_code_mask = (spr_len / 0x100);
	tileno_mask = (tile_len / 0x100);

	BurnFree (tmp);
}

static void tile_decode(INT32 spr_len, INT32 tile_len)
{
	UINT8 lsb,msb;
	UINT8 *gfx = TaitoChars;
	int size=tile_len;
	UINT32 offset = size/2;

	for (INT32 i = size/2+size/4; i<size; i+=2)
	{
		lsb = gfx[i+1];
		msb = gfx[i+0];

		gfx[offset+0]=((msb&0x02)<<3) | ((msb&0x01)>>0) | ((lsb&0x02)<<4) | ((lsb&0x01)<<1);
		gfx[offset+2]=((msb&0x08)<<1) | ((msb&0x04)>>2) | ((lsb&0x08)<<2) | ((lsb&0x04)>>1);
		gfx[offset+1]=((msb&0x20)>>1) | ((msb&0x10)>>4) | ((lsb&0x20)<<0) | ((lsb&0x10)>>3);
		gfx[offset+3]=((msb&0x80)>>3) | ((msb&0x40)>>6) | ((lsb&0x80)>>2) | ((lsb&0x40)>>5);

		offset+=4;
	}

	size = spr_len;
	offset = size/2;

	for (INT32 i = size/2+size/4; i<size; i++, offset+=2)
	{
		TaitoSpritesA[offset+0] = (((TaitoSpritesA[i] >> 0) & 3) << 2) | (((TaitoSpritesA[i] >> 2) & 3) << 6);
		TaitoSpritesA[offset+1] = (((TaitoSpritesA[i] >> 4) & 3) << 2) | (((TaitoSpritesA[i] >> 6) & 3) << 6);
	}

	DrvGfxDecode(spr_len, tile_len);

	DrvCalculateTransTable(tile_len);
}

static INT32 TaitoF3GetRoms(bool bLoad)
{
	if (!bLoad) TaitoF3ES5506RomSize = 0;

	char* pRomName;
	struct BurnRomInfo ri;
	struct BurnRomInfo pi;

	UINT8 *sprites = TaitoSpritesA;
	UINT8 *tiles = TaitoChars;
	UINT8 *samples = TaitoES5505Rom + ((TaitoF3ES5506RomSize == 0x1000000) ? 0x400000 : 0);

	INT32 prevsize = 0;
	INT32 prevtype = 0;
	INT32 tilecount = 0;
	INT32 spritecount = 0;
	INT32 ret = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		prevsize = ri.nLen;
		prevtype = ri.nType;

		BurnDrvGetRomInfo(&ri, i);
		BurnDrvGetRomInfo(&pi, i+1);

		if (ri.nType == TAITO_68KROM1_BYTESWAP32)
		{
	//		if (bLoad) bprintf (0, _T("000000 68k1\n"));

			if (bLoad) {
				ret  = BurnLoadRom(Taito68KRom1 + 1, i + 0, 4);
				ret |= BurnLoadRom(Taito68KRom1 + 0, i + 1, 4);
				ret |= BurnLoadRom(Taito68KRom1 + 3, i + 2, 4);
				ret |= BurnLoadRom(Taito68KRom1 + 2, i + 3, 4);
				if (ret) return 1;
			}
			i += 3;
			continue;
		}

		if (ri.nType == TAITO_SPRITESA_BYTESWAP)
		{
	//		if (bLoad) bprintf (0, _T("%6.6x sprite 2x\n"), sprites - TaitoSpritesA);

			if (m_f3_game == GSEEKER)
			{
				if (bLoad) {
					if (BurnLoadRom(sprites + 0x000000, i + 0, 2)) return 1;
					if (BurnLoadRom(sprites + 0x100001, i + 1, 2)) return 1;
					if (BurnLoadRom(sprites + 0x000000, i + 2, 2)) return 1;
					if (BurnLoadRom(sprites + 0x000001, i + 3, 2)) return 1;
					memset (sprites + 0x200000, 0, 0x100000);
				}
				sprites += 0x400000;
				i+=3;
			}
			else
			{
				if (bLoad) {
					BurnLoadRom(sprites + 0, i + 0, 2);
					BurnLoadRom(sprites + 1, i + 1, 2);
				}
				sprites += ri.nLen * 2;
				i++;
			}
			continue;
		}

		if (ri.nType == TAITO_SPRITESA)
		{
			spritecount = 1;
			if (prevtype == TAITO_SPRITESA_BYTESWAP) {
				sprites = TaitoSpritesA + ((sprites - TaitoSpritesA) / 2) * 3;
			}

	//		if (bLoad) bprintf (0, _T("%6.6x sprite 1x \n"), sprites - TaitoSpritesA);

			if (bLoad) {
				BurnLoadRom(sprites + 0, i + 0, 1);
			}
			sprites += ri.nLen;
			continue;
		}

		if (ri.nType == TAITO_CHARS_BYTESWAP32)
		{
	//		if (bLoad) bprintf (0, _T("%6.6x tiles x4\n"), tiles - TaitoChars);

			if (bLoad) {
				ret  = BurnLoadRom(tiles + 0, i + 0, 4);
				ret |= BurnLoadRom(tiles + 1, i + 1, 4);
				ret |= BurnLoadRom(tiles + 2, i + 2, 4);
				ret |= BurnLoadRom(tiles + 3, i + 3, 4);
				if (ret) return 1;
			}
			i+=3;
			tiles += ri.nLen * 4;
			continue;
		}

		if (ri.nType == TAITO_CHARS_BYTESWAP)
		{
			if (prevtype == TAITO_CHARS_BYTESWAP32) {
				tiles = TaitoChars + ((tiles - TaitoChars) / 2) * 3;
				tilecount = 1;
			}

	//		if (bLoad) bprintf (0, _T("%6.6x tiles x2\n"), tiles - TaitoChars);

			if (pi.nType != TAITO_CHARS_BYTESWAP) {
				if (bLoad) {
					BurnLoadRom(tiles + 0, i + 0, 2);
				}
			} else {
				if (bLoad) {
					BurnLoadRom(tiles + 0, i + 0, 2);
					BurnLoadRom(tiles + 1, i + 1, 2);
				}
				i++;
			}
			tiles += ri.nLen * 2;
			continue;
		}

		if (ri.nType == TAITO_CHARS)
		{
			tilecount = 1;
			if (prevtype == TAITO_CHARS_BYTESWAP) {
				tiles = TaitoChars + ((tiles - TaitoChars) / 2) * 3;
			}

	//		if (bLoad) bprintf (0, _T("%6.6x tiles x1 \n"), tiles - TaitoChars);

			if (bLoad) {
				BurnLoadRom(tiles + 0, i + 0, 1);
			}
			tiles += ri.nLen;
	//		if (bLoad) bprintf (0, _T("%6.6x tiles x1b \n"), tiles - TaitoChars);

			continue;
		}

		if (ri.nType == TAITO_68KROM2_BYTESWAP)
		{
	//		if (bLoad) bprintf (0, _T("000000 68k2 x2\n"));

			if (bLoad) {
				ret  = BurnLoadRom(Taito68KRom2 + 1, i + 0, 2);
				ret |= BurnLoadRom(Taito68KRom2 + 0, i + 1, 2);
				if (ret) return 1;
			}
			i++;
			continue;
		}

		if (ri.nType == TAITO_68KROM2) // kirameki
		{
	//		if (bLoad) bprintf (0, _T("100000, 68k1 x1\n"));
			if (bLoad) {
				BurnLoadRom(Taito68KRom2 + 0x100000, i, 1);
			}
			continue;
		}

		if (ri.nType == TAITO_ES5505_BYTESWAP)
		{
			INT32 size = samples - TaitoES5505Rom;

			if (prevtype == TAITO_ES5505_BYTESWAP && prevsize == 0x200000 && ri.nLen == 0x100000 && size == 0x400000) {
				samples += 0x200000;
			}

			if (size == 0xc00000 && ri.nLen == 0x100000) {
				samples += 0x200000;
			}

	//		if (bLoad) bprintf (0, _T("%6.6x, samples \n"), samples - TaitoES5505Rom);

			if (bLoad) {
				if (BurnLoadRom(samples + 1, i, 2)) return 1;
			}
			samples += ri.nLen * 2;
			continue;
		}

		if (ri.nType == TAITO_DEFAULT_EEPROM)
		{
			if (bLoad) {
				if (BurnLoadRom(TaitoDefaultEEProm, i, 1)) return 1;
			}
			continue;
		}
	}

	if (bLoad == false) {
		INT32 spritesize = sprites - TaitoSpritesA;
		INT32 tilesize = tiles - TaitoChars;
		INT32 samplesize = samples - TaitoES5505Rom;

		if (samplesize >= 0xa00000) {
			samplesize = 0x1000000;
		}

		if (tilecount == 0) tilesize *= 2;
		if (spritecount == 0) spritesize *= 2;

		for (INT32 i = 1; i < 1<<30; i<<=1) {
			if (i >= samplesize) {
				samplesize = i;
				break;
			}
		}

		TaitoSpriteARomSize = spritesize;
		TaitoCharRomSize = tilesize;
		TaitoF3ES5506RomSize = samplesize;
	//	bprintf (0, _T("Load: %x, %x, %x\n"), spritesize, tilesize, samplesize);
	}

	return 0;
}

static INT32 DrvInit(INT32 (*pRomLoadCB)(), void (*pPalUpdateCB)(UINT16), INT32 extend, INT32 kludge, INT32 spritelag)
{
	m_f3_game = kludge;

	TaitoF3GetRoms(false);

	MemIndex();
	INT32 nLen = TaitoMemEnd - (UINT8 *)0;
	if ((TaitoMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(TaitoMem, 0, nLen);
	MemIndex();

	{
		if (TaitoF3GetRoms(true)) return 1;

		if (pRomLoadCB) {
			if (pRomLoadCB()) return 1;
		}

		tile_decode(TaitoSpriteARomSize, TaitoCharRomSize);
	}

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,	0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,	0x400000, 0x41ffff, MAP_RAM);
	SekMapMemory(Taito68KRam1,	0x420000, 0x43ffff, MAP_RAM); // mirror
	SekMapMemory(TaitoPaletteRam,	0x440000, 0x447fff, MAP_ROM); // write through handler
	SekMapMemory(TaitoSpriteRam,	0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(DrvPfRAM,		0x610000, 0x617fff, MAP_ROM); // write through handler
	SekMapMemory(DrvPfRAM + 0x8000,	0x618000, 0x61bfff, MAP_RAM);
	SekMapMemory(TaitoVideoRam,	0x61c000, 0x61dfff, MAP_RAM);
	SekMapMemory(DrvVRAMRAM,	0x61e000, 0x61ffff, MAP_ROM); // write through handler
	SekMapMemory(DrvLineRAM,	0x620000, 0x62ffff, MAP_RAM);
	SekMapMemory(DrvPivotRAM,	0x630000, 0x63ffff, MAP_ROM); // write through handler
	SekMapMemory(DrvCtrlRAM,	0x660000, 0x6603ff, MAP_WRITE); // 0-1f
	SekMapMemory(TaitoF3SharedRam,	0xc00000, 0xc007ff, MAP_RAM);
	SekSetWriteLongHandler(0,	f3_main_write_long);
	SekSetWriteWordHandler(0,	f3_main_write_word);
	SekSetWriteByteHandler(0,	f3_main_write_byte);
	SekSetReadLongHandler(0,	f3_main_read_long);
	SekSetReadWordHandler(0,	f3_main_read_word);
	SekSetReadByteHandler(0,	f3_main_read_byte);

	SekMapHandler(1,		0x440000, 0x447fff, MAP_WRITE);
	SekSetWriteLongHandler(1,	f3_palette_write_long);
	SekSetWriteWordHandler(1,	f3_palette_write_word);
	SekSetWriteByteHandler(1,	f3_palette_write_byte);

	SekMapHandler(2,		0x61e000, 0x61ffff, MAP_WRITE);
	SekSetWriteLongHandler(2,	f3_VRAM_write_long);
	SekSetWriteWordHandler(2,	f3_VRAM_write_word);
	SekSetWriteByteHandler(2,	f3_VRAM_write_byte);

	SekMapHandler(3,		0x630000, 0x63ffff, MAP_WRITE);
	SekSetWriteLongHandler(3,	f3_pivot_write_long);
	SekSetWriteWordHandler(3,	f3_pivot_write_word);
	SekSetWriteByteHandler(3,	f3_pivot_write_byte);

	SekMapHandler(4,		0x610000, 0x617fff, MAP_WRITE);
	SekSetWriteLongHandler(4,	f3_playfield_write_long);
	SekSetWriteWordHandler(4,	f3_playfield_write_word);
	SekSetWriteByteHandler(4,	f3_playfield_write_byte);
	SekClose();

	TaitoF3SoundInit(1);

	EEPROMInit(&eeprom_interface_93C46);
	EEPROMIgnoreErrMessage(1);

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nScreenHeight, &nScreenWidth);
	} else {
		BurnDrvGetVisibleSize(&nScreenWidth, &nScreenHeight);
	}

	pPaletteUpdateCallback = pPalUpdateCB;
	extended_layers = extend;
	sprite_lag = spritelag;

	m_spritelist = (struct tempsprite*)BurnMalloc(0x400 * sizeof(struct tempsprite));
	m_sprite_end = m_spritelist;

	TaitoF3VideoInit();

	bitmap_width[0] = (extend) ? 1024 : 512;
	bitmap_width[1] = (extend) ? 1024 : 512;
	bitmap_width[2] = (extend) ? 1024 : 512;
	bitmap_width[3] = (extend) ? 1024 : 512;
	bitmap_width[4] = 512;
	bitmap_width[5] = 512;
	bitmap_width[6] = 512;
	bitmap_width[7] = 512;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	SekExit();
	TaitoF3SoundExit();

	EEPROMExit();

	BurnFree (TaitoMem);

	BurnFree (m_spritelist);

	TaitoClearVariables(); // from taito.cpp

	pPaletteUpdateCallback = NULL;

	f3_kludge = 0;

	return 0;
}

static void f3_12bit_palette_update(UINT16 offset)
{
	UINT32 x = *((UINT32*)(TaitoPaletteRam + (offset & ~3)));

	UINT8 r = ((x >> 28) & 0x0f) * 0x0f;
	UINT8 g = ((x >> 24) & 0x0f) * 0x0f;
	UINT8 b = ((x >> 20) & 0x0f) * 0x0f;

	TaitoPalette[offset/4] = r*0x10000+g*0x100+b; //BurnHighCol(r,g,b, 0);
}

static void f3_21bit_typeA_palette_update(UINT16 offset)
{
	UINT32 x = *((UINT32*)(TaitoPaletteRam + (offset & ~3)));

	UINT8 r = x;
	UINT8 g = x >> 24;
	UINT8 b = x >> 16;

	if (offset < 0x400 || offset > 0x4000) {
		r <<= 1;
		g <<= 1;
		b <<= 1;
	}

	TaitoPalette[offset/4] = r*0x10000+g*0x100+b;
}

static void f3_21bit_typeB_palette_update(UINT16 offset)
{
	UINT32 x = *((UINT32*)(TaitoPaletteRam + (offset & ~3)));

	UINT8 r = x;
	UINT8 g = x >> 24;
	UINT8 b = x >> 16;

	if (offset > 0x7000) {
		r <<= 1;
		g <<= 1;
		b <<= 1;
	}

	TaitoPalette[offset/4] = r*0x10000+g*0x100+b; //BurnHighCol(r,g,b, 0);
}

static void f3_24bit_palette_update(UINT16 offset)
{
	UINT32 x = *((UINT32*)(TaitoPaletteRam + (offset & ~3)));

	UINT8 r = x;
	UINT8 g = x >> 24;
	UINT8 b = x >> 16;

	TaitoPalette[offset/4] = r*0x10000+g*0x100+b; //BurnHighCol(r,g,b, 0);
}

static void draw_pf_layer(INT32 layer)
{
	INT32 offset = (layer * (0x1000 << extended_layers));

	UINT16 *ram = (UINT16*)(DrvPfRAM + offset);

	INT32 width = extended_layers ? 1024 : 512;
	INT32 wide = width / 16;

	for (INT32 offs = 0; offs < wide * 32; offs++)
	{
		if (dirty_tiles[((offs * 4) + offset) / 4] == 0) continue;
		dirty_tiles[((offs * 4) + offset) / 4] = 0;

		INT32 sx = (offs % wide) * 16;
		INT32 sy = (offs / wide) * 16;

		UINT16 tile = ram[offs * 2 + 0];
		UINT16 code = (ram[offs * 2 + 1] & 0xffff) % tileno_mask;

		UINT8 category = (tile >> 9) & 1;

		INT32 explane = (tile >> 10) & 3;
		INT32 flipx   = (tile >> 14) & 1;
		INT32 flipy   = (tile >> 15) & 1;
		INT32 color   = ((tile & 0x1ff) & (~explane)) << 4;

		INT32 penmask = (explane << 4) | 0x0f;

		if (m_flipscreen) {
			sx = (width - 16) - sx;
			sy = (512 - 16) - sy;
			flipy ^= 1;
			flipx ^= 1;
		}

		{
			INT32 flip = (flipy * 0xf0) + (flipx * 0x0f);

			UINT8 *gfx = TaitoChars + (code * 0x100);
			UINT16 *dst = bitmap_layer[layer] + (sy * width) + sx;
			UINT8 *flagptr = bitmap_flags[layer] + (sy * width) + sx;

			for (INT32 y = 0; y < 16; y++, sy++, dst += width, flagptr += width)
			{
				for (INT32 x = 0; x < 16; x++)
				{
					int pxl = gfx[((y*16)+x)^flip] & penmask;

					dst[x] = pxl + color;

					if (pxl == 0) {
						flagptr[x] = category;
					} else {
						flagptr[x] = category | 0x10;
					}
				}
			}
		}
	}
}

static void draw_vram_layer()
{
	UINT16 *ram = (UINT16*)TaitoVideoRam;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sx = (offs & 0x3f) * 8;
		INT32 sy = (offs / 0x40) * 8;

		INT32 tile = ram[offs] & 0xffff;

		INT32 code = tile & 0x00ff;

		INT32 flipx = tile & 0x0100;
		INT32 flipy = tile & 0x8000;

		if (m_flipscreen) {
			sx = (512 - 8) - sx;
			sy = (512 - 8) - sy;
			flipx ^= 0x100;
			flipy ^= 0x8000;
		}

		INT32 color = (tile >> 9) & 0x3f;

		{
			color *= 16;
			UINT8 *gfx = TaitoCharsB + (code * 8 * 8);

			INT32 flip = ((flipy ? 0x38 : 0) | (flipx ? 7 : 0));

			UINT16 *dst = bitmap_layer[8] + (sy * 512) + sx;
			UINT8 *flagptr = bitmap_flags[8] + (sy * 512) + sx;

			for (INT32 y = 0; y < 8; y++)
			{
				for (INT32 x = 0; x < 8; x++)
				{
					INT32 pxl = gfx[((y*8)+x)^flip];

					dst[x] = pxl + color;

					if (pxl == 0) {
						flagptr[x] = /*category | */ 0;
					} else {
						flagptr[x] = /*category | */0x10;
					}
				}

				dst += 512;
				flagptr += 512;
			}
		}
	}
}

static void draw_pixel_layer()
{
	UINT16 *ram = (UINT16*)TaitoVideoRam;

	UINT16 y_offs = *((UINT16*)(DrvCtrlRAM + 0x1a)) & 0x1ff;
	if (m_flipscreen) y_offs += 0x100;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs / 0x20) * 8;
		INT32 sy = (offs & 0x1f) * 8;

		INT32 col_off = ((offs & 0x1f) * 0x40) + ((offs & 0xfe0) >> 5);

		if ((((offs & 0x1f) * 8 + y_offs) & 0x1ff) > 0xff)
			col_off += 0x800;

		INT32 tile = ram[col_off];

		INT32 code = offs;

		INT32 flipx = tile & 0x0100;
		INT32 flipy = tile & 0x8000;

		INT32 color = (tile >> 9) & 0x3f;

		if (m_flipscreen)
		{
			flipx ^= 0x0100;
			flipy ^= 0x8000;
			sy = (256 - 8) - sy;
			sx = (512 - 8) - sx;
		}

		{
			color *= 16;
			UINT8 *gfx = TaitoCharsPivot + (code * 8 * 8);

			INT32 flip = ((flipy ? 0x38 : 0) | (flipx ? 7 : 0));

			UINT16 *dst = bitmap_layer[9] + (sy * 512) + sx;
			UINT8 *flagptr = bitmap_flags[9] + (sy * 512) + sx;

			for (INT32 y = 0; y < 8; y++)
			{
				for (INT32 x = 0; x < 8; x++)
				{
					INT32 pxl = gfx[((y*8)+x)^flip];

					dst[x] = pxl + color;

					if (pxl == 0) {
						flagptr[x] = /*category | */ 0;
					} else {
						flagptr[x] = /*category | */0x10;
					}
				}

				dst += 512;
				flagptr += 512;
			}
		}
	}
}




#if 1

#define PSET_T					\
	c = *source & m_sprite_pen_mask;	\
	if(c)						\
	{							\
		p=*pri;					\
		if(!p || p==0xff)		\
		{						\
			*dest = pal[c];		\
			*pri = pri_dst;		\
		}						\
	}

#define PSET_O					\
	p=*pri;						\
	if(!p || p==0xff)			\
	{							\
		*dest = pal[(*source & m_sprite_pen_mask)];	\
		*pri = pri_dst;			\
	}

#define NEXT_P					\
	source += dx;				\
	dest++;						\
	pri++;

static void f3_drawgfx(
		int code,
		int color,
		int flipx,int flipy,
		int sx,int sy,
		UINT8 pri_dst)
{
	pri_dst=1<<pri_dst;

	/* KW 991012 -- Added code to force clip to bitmap boundary */
//	myclip = clip;
//	myclip &= dest_bmp.cliprect();

	{
		const UINT32 *pal = TaitoPalette + (0x1000 + ((color & 0xff) << 4));
		const UINT8 *code_base = TaitoSpritesA + (code * 0x100);

		{
			/* compute sprite increment per screen pixel */
			int dx = 1;
			int dy = 1;

			int ex = sx+16;
			int ey = sy+16;

			int x_index_base;
			int y_index;

			if( flipx )
			{
				x_index_base = 15;
				dx = -1;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = 15;
				dy = -1;
			}
			else
			{
				y_index = 0;
			}

			if(sx < min_x)
			{ /* clip left */
				int pixels = min_x-sx;
				sx += pixels;
				x_index_base += pixels*dx;
			}
			if( sy < min_y )
			{ /* clip top */
				int pixels = min_y-sy;
				sy += pixels;
				y_index += pixels*dy;
			}
			/* NS 980211 - fixed incorrect clipping */
			if( ex > max_x+1 )
			{ /* clip right */
				int pixels = ex-max_x-1;
				ex -= pixels;
			}
			if( ey > max_y+1 )
			{ /* clip bottom */
				int pixels = ey-max_y-1;
				ey -= pixels;
			}

			if( ex>sx && ey>sy)
			{ /* skip if inner loop doesn't draw anything */
				{
					int y=ey-sy;
					int x=(ex-sx-1);//|(state->m_tile_opaque_sp[code % gfx->elements()]<<4); // iq_132
					const UINT8 *source0 = code_base + y_index * 16 + x_index_base;
					UINT32 *dest0 = output_bitmap + sy * 512 + sx;
					UINT8 *pri0   = TaitoPriorityMap + sy * 1024 + sx;

					int yadv = 512;
					int yadvp = 1024;

					dy=dy*16;

					while(1)
					{
						const UINT8 *source = source0;
						UINT32 *dest = dest0;
						UINT8 *pri = pri0;

						switch(x)
						{
							int c;
							UINT8 p;
							case 31: PSET_O NEXT_P
							case 30: PSET_O NEXT_P
							case 29: PSET_O NEXT_P
							case 28: PSET_O NEXT_P
							case 27: PSET_O NEXT_P
							case 26: PSET_O NEXT_P
							case 25: PSET_O NEXT_P
							case 24: PSET_O NEXT_P
							case 23: PSET_O NEXT_P
							case 22: PSET_O NEXT_P
							case 21: PSET_O NEXT_P
							case 20: PSET_O NEXT_P
							case 19: PSET_O NEXT_P
							case 18: PSET_O NEXT_P
							case 17: PSET_O NEXT_P
							case 16: PSET_O break;

							case 15: PSET_T NEXT_P
							case 14: PSET_T NEXT_P
							case 13: PSET_T NEXT_P
							case 12: PSET_T NEXT_P
							case 11: PSET_T NEXT_P
							case 10: PSET_T NEXT_P
							case  9: PSET_T NEXT_P
							case  8: PSET_T NEXT_P
							case  7: PSET_T NEXT_P
							case  6: PSET_T NEXT_P
							case  5: PSET_T NEXT_P
							case  4: PSET_T NEXT_P
							case  3: PSET_T NEXT_P
							case  2: PSET_T NEXT_P
							case  1: PSET_T NEXT_P
							case  0: PSET_T
						}

						if(!(--y)) break;
						source0 += dy;
						dest0+=yadv;
						pri0+=yadvp;
					}
				}
			}
		}
	}
}
#undef PSET_T
#undef PSET_O
#undef NEXT_P


void f3_drawgfxzoom(
		int code,
		int color,
		int flipx,int flipy,
		int sx,int sy,
		int scalex, int scaley,
		UINT8 pri_dst)
{
	pri_dst=1<<pri_dst;

	/* KW 991012 -- Added code to force clip to bitmap boundary */
//	myclip = clip;
//	myclip &= dest_bmp.cliprect();


	{
		const UINT32 *pal = TaitoPalette + (0x1000 + ((color & 0xff) << 4));
		const UINT8 *code_base = TaitoSpritesA + code * 0x100;

		{
			/* compute sprite increment per screen pixel */
			int dx = (16<<16)/scalex;
			int dy = (16<<16)/scaley;

			int ex = sx+scalex;
			int ey = sy+scaley;

			int x_index_base;
			int y_index;

			if( flipx )
			{
				x_index_base = (scalex-1)*dx;
				dx = -dx;
			}
			else
			{
				x_index_base = 0;
			}

			if( flipy )
			{
				y_index = (scaley-1)*dy;
				dy = -dy;
			}
			else
			{
				y_index = 0;
			}

			if(sx < min_x)
			{ /* clip left */
				int pixels = min_x-sx;
				sx += pixels;
				x_index_base += pixels*dx;
			}
			if( sy < min_y )
			{ /* clip top */
				int pixels = min_y-sy;
				sy += pixels;
				y_index += pixels*dy;
			}
			/* NS 980211 - fixed incorrect clipping */
			if( ex > max_x+1 )
			{ /* clip right */
				int pixels = ex-max_x-1;
				ex -= pixels;
			}
			if( ey > max_y+1 )
			{ /* clip bottom */
				int pixels = ey-max_y-1;
				ey -= pixels;
			}

			if( ex>sx )
			{ /* skip if inner loop doesn't draw anything */
				{
					int y;
					for( y=sy; y<ey; y++ )
					{
						const UINT8 *source = code_base + (y_index>>16) * 16;
						UINT32 *dest = output_bitmap + y * 512;
						UINT8 *pri = TaitoPriorityMap + y * 1024;

						int x, x_index = x_index_base;
						for( x=sx; x<ex; x++ )
						{
							int c = source[x_index>>16] & m_sprite_pen_mask;
							if(c)
							{
								UINT8 p=pri[x];
								if (p == 0 || p == 0xff)
								{
									dest[x] = pal[c];
									pri[x] = pri_dst;
								}
							}
							x_index += dx;
						}
						y_index += dy;
					}
				}
			}
		}
	}
}


#define CALC_ZOOM(p)	{										\
	p##_addition = 0x100 - block_zoom_##p + p##_addition_left;	\
	p##_addition_left = p##_addition & 0xf;						\
	p##_addition = p##_addition >> 4;							\
	/*zoom##p = p##_addition << 12;*/							\
}


static void get_sprite_info(UINT16 *spriteram16_ptr)
{
#define DARIUSG_KLUDGE

	//const rectangle &visarea = m_screen->visible_area();
	//const int min_x=visarea.min_x,max_x=visarea.max_x;
	//const int min_y=visarea.min_y,max_y=visarea.max_y;

	int offs,spritecont,flipx,flipy,/*old_x,*/color,x,y;
	int sprite,global_x=0,global_y=0,subglobal_x=0,subglobal_y=0;
	int block_x=0, block_y=0;
	int last_color=0,last_x=0,last_y=0,block_zoom_x=0,block_zoom_y=0;
	int this_x,this_y;
	int y_addition=16, x_addition=16;
	int multi=0;
	int sprite_top;

	int x_addition_left = 8, y_addition_left = 8;

	struct tempsprite *sprite_ptr = m_spritelist;

	int total_sprites=0;
	int jumpcnt = 0;
	color=0;
	flipx=flipy=0;
	//old_x=0;
	y=x=0;

	sprite_top=0x2000;
	for (offs = 0; offs < sprite_top && (total_sprites < 0x400); offs += 8)
	{
		const int current_offs=offs; /* Offs can change during loop, current_offs cannot */

		/* Check if the sprite list jump command bit is set */
		if ((spriteram16_ptr[current_offs+6+0]) & 0x8000) {
			uint32_t jump = (spriteram16_ptr[current_offs+6+0])&0x3ff;

			int32_t new_offs=((offs&0x4000)|((jump<<4)/2));
			if (new_offs==offs || jumpcnt > 250)
				break;
			offs=new_offs - 8;
			jumpcnt++;
		}

		/* Check if special command bit is set */
		if (spriteram16_ptr[current_offs+2+1] & 0x8000) {
			uint32_t cntrl=(spriteram16_ptr[current_offs+4+1])&0xffff;
			m_flipscreen=cntrl&0x2000;

			/*  cntrl&0x1000 = disabled?  (From F2 driver, doesn't seem used anywhere)
			    cntrl&0x0010 = ???
			    cntrl&0x0020 = ???
			*/

			m_sprite_extra_planes = (cntrl & 0x0300) >> 8;   // 0 = 4bpp, 1 = 5bpp, 2 = unused?, 3 = 6bpp
			m_sprite_pen_mask = (m_sprite_extra_planes << 4) | 0x0f;

			/* Sprite bank select */
			if (cntrl&1) {
				offs=offs|0x4000;
				sprite_top=sprite_top|0x4000;
			}
		}

		/* Set global sprite scroll */
		if (((spriteram16_ptr[current_offs+2+0]) & 0xf000) == 0xa000) {
			global_x = (spriteram16_ptr[current_offs+2+0]) & 0xfff;
			if (global_x >= 0x800) global_x -= 0x1000;
			global_y = spriteram16_ptr[current_offs+2+1] & 0xfff;
			if (global_y >= 0x800) global_y -= 0x1000;
		}

		/* And sub-global sprite scroll */
		if (((spriteram16_ptr[current_offs+2+0]) & 0xf000) == 0x5000) {
			subglobal_x = (spriteram16_ptr[current_offs+2+0]) & 0xfff;
			if (subglobal_x >= 0x800) subglobal_x -= 0x1000;
			subglobal_y = spriteram16_ptr[current_offs+2+1] & 0xfff;
			if (subglobal_y >= 0x800) subglobal_y -= 0x1000;
		}

		if (((spriteram16_ptr[current_offs+2+0]) & 0xf000) == 0xb000) {
			subglobal_x = (spriteram16_ptr[current_offs+2+0]) & 0xfff;
			if (subglobal_x >= 0x800) subglobal_x -= 0x1000;
			subglobal_y = spriteram16_ptr[current_offs+2+1] & 0xfff;
			if (subglobal_y >= 0x800) subglobal_y -= 0x1000;
			global_y=subglobal_y;
			global_x=subglobal_x;
		}

		/* A real sprite to process! */
		sprite = (spriteram16_ptr[current_offs+0+0]) | ((spriteram16_ptr[current_offs+4+1]&1)<<16);
		spritecont = spriteram16_ptr[current_offs+4+0]>>8;

/* These games either don't set the XY control bits properly (68020 bug?), or
    have some different mode from the others */
#ifdef DARIUSG_KLUDGE
		if (m_f3_game==DARIUSG || m_f3_game==GEKIRIDO || m_f3_game==CLEOPATR || m_f3_game==RECALH)
			multi=spritecont&0xf0;
#endif

		/* Check if this sprite is part of a continued block */
		if (multi) {
			/* Bit 0x4 is 'use previous colour' for this block part */
			if (spritecont&0x4) color=last_color;
			else color=(spriteram16_ptr[current_offs+4+0])&0xff;

#ifdef DARIUSG_KLUDGE
			if (m_f3_game==DARIUSG || m_f3_game==GEKIRIDO || m_f3_game==CLEOPATR || m_f3_game==RECALH) {
				/* Adjust X Position */
				if ((spritecont & 0x40) == 0) {
					if (spritecont & 0x4) {
						x = block_x;
					} else {
						this_x = spriteram16_ptr[current_offs+2+0];
						if (this_x&0x800) this_x= 0 - (0x800 - (this_x&0x7ff)); else this_x&=0x7ff;

						if ((spriteram16_ptr[current_offs+2+0])&0x8000) {
							this_x+=0;
						} else if ((spriteram16_ptr[current_offs+2+0])&0x4000) {
							/* Ignore subglobal (but apply global) */
							this_x+=global_x;
						} else { /* Apply both scroll offsets */
							this_x+=global_x+subglobal_x;
						}

						x = block_x = this_x;
					}
					x_addition_left = 8;
					CALC_ZOOM(x)
				}
				else if ((spritecont & 0x80) != 0) {
					x = last_x+x_addition;
					CALC_ZOOM(x)
				}

				/* Adjust Y Position */
				if ((spritecont & 0x10) == 0) {
					if (spritecont & 0x4) {
						y = block_y;
					} else {
						this_y = spriteram16_ptr[current_offs+2+1]&0xffff;
						if (this_y&0x800) this_y= 0 - (0x800 - (this_y&0x7ff)); else this_y&=0x7ff;

						if ((spriteram16_ptr[current_offs+2+0])&0x8000) {
							this_y+=0;
						} else if ((spriteram16_ptr[current_offs+2+0])&0x4000) {
							/* Ignore subglobal (but apply global) */
							this_y+=global_y;
						} else { /* Apply both scroll offsets */
							this_y+=global_y+subglobal_y;
						}

						y = block_y = this_y;
					}
					y_addition_left = 8;
					CALC_ZOOM(y)
				}
				else if ((spritecont & 0x20) != 0) {
					y = last_y+y_addition;
					CALC_ZOOM(y)
				}
			} else
#endif
			{
				/* Adjust X Position */
				if ((spritecont & 0x40) == 0) {
					x = block_x;
					x_addition_left = 8;
					CALC_ZOOM(x)
				}
				else if ((spritecont & 0x80) != 0) {
					x = last_x+x_addition;
					CALC_ZOOM(x)
				}
				/* Adjust Y Position */
				if ((spritecont & 0x10) == 0) {
					y = block_y;
					y_addition_left = 8;
					CALC_ZOOM(y)
				}
				else if ((spritecont & 0x20) != 0) {
					y = last_y+y_addition;
					CALC_ZOOM(y)
				}
				/* Both zero = reread block latch? */
			}
		}
		/* Else this sprite is the possible start of a block */
		else {
			color = (spriteram16_ptr[current_offs+4+0])&0xff;
			last_color=color;

			/* Sprite positioning */
			this_y = spriteram16_ptr[current_offs+2+1]&0xffff;
			this_x = spriteram16_ptr[current_offs+2+0]&0xffff;
			if (this_y&0x800) this_y= 0 - (0x800 - (this_y&0x7ff)); else this_y&=0x7ff;
			if (this_x&0x800) this_x= 0 - (0x800 - (this_x&0x7ff)); else this_x&=0x7ff;

			/* Ignore both scroll offsets for this block */
			if ((spriteram16_ptr[current_offs+2+0])&0x8000) {
				this_x+=0;
				this_y+=0;
			} else if ((spriteram16_ptr[current_offs+2+0])&0x4000) {
				/* Ignore subglobal (but apply global) */
				this_x+=global_x;
				this_y+=global_y;
			} else { /* Apply both scroll offsets */
				this_x+=global_x+subglobal_x;
				this_y+=global_y+subglobal_y;
			}

			block_y = y = this_y;
			block_x = x = this_x;

			block_zoom_x=spriteram16_ptr[current_offs+0+1];
			block_zoom_y=(block_zoom_x>>8)&0xff;
			block_zoom_x&=0xff;

			x_addition_left = 8;
			CALC_ZOOM(x)

			y_addition_left = 8;
			CALC_ZOOM(y)
		}

		/* These features are common to sprite and block parts */
		flipx = spritecont&0x1;
		flipy = spritecont&0x2;
		multi = spritecont&0x8;
		last_x=x;
		last_y=y;

		if (!sprite) continue;
		if (!x_addition || !y_addition) continue;

		if (m_flipscreen)
		{
			int tx,ty;

			tx = 512-x_addition-x;
			ty = y; //256-y_addition-y;

			if (tx+x_addition<=min_x || tx>max_x || ty+y_addition<=min_y || ty>max_y) continue;
			sprite_ptr->x = tx;
			sprite_ptr->y = ty;
			sprite_ptr->flipx = !flipx;
			sprite_ptr->flipy = flipy;
		}
		else
		{
			if (x+x_addition<=min_x || x>max_x || y+y_addition<=min_y || y>max_y) continue;
			sprite_ptr->x = x;
			sprite_ptr->y = y;
			sprite_ptr->flipx = flipx;
			sprite_ptr->flipy = flipy;
		}

		sprite_ptr->code = sprite % sprite_code_mask;
		sprite_ptr->color = color;
		sprite_ptr->zoomx = x_addition;
		sprite_ptr->zoomy = y_addition;
		sprite_ptr->pri = (color & 0xc0) >> 6;
		sprite_ptr++;
		total_sprites++;
	}
	if (jumpcnt>150) bprintf(0, _T("Sprite Jumps: %d. \n"), jumpcnt);
	m_sprite_end = sprite_ptr;
}
#undef CALC_ZOOM


static void draw_sprites()
{
	const struct tempsprite *sprite_ptr;
	sprite_ptr = m_sprite_end;
	m_sprite_pri_usage=0;

	// if sprites use more than 4bpp, the bottom bits of the color code must be masked out.
	// This fixes (at least) stage 1 battle ships and attract mode explosions in Ray Force.

	while (sprite_ptr != m_spritelist)
	{
		int pri;
		sprite_ptr--;

		pri=sprite_ptr->pri;
		m_sprite_pri_usage|=1<<pri;

		if(sprite_ptr->zoomx==16 && sprite_ptr->zoomy==16)
			f3_drawgfx(
					sprite_ptr->code,
					sprite_ptr->color & (~m_sprite_extra_planes),
					sprite_ptr->flipx,sprite_ptr->flipy,
					sprite_ptr->x,sprite_ptr->y,
					pri);
		else
			f3_drawgfxzoom(
					sprite_ptr->code,
					sprite_ptr->color & (~m_sprite_extra_planes),
					sprite_ptr->flipx,sprite_ptr->flipy,
					sprite_ptr->x,sprite_ptr->y,
					sprite_ptr->zoomx,sprite_ptr->zoomy,
					pri);
	}
}

#endif


static INT32 m_width_mask=0x3ff;
static INT32 m_twidth_mask=0x7f;
static INT32 m_twidth_mask_bit=7;

#define ORIENTATION_FLIP_Y	1


	UINT16 *m_src0;
	UINT16 *m_src_s0;
	UINT16 *m_src_e0;
	UINT16 m_clip_al0;
	UINT16 m_clip_ar0;
	UINT16 m_clip_bl0;
	UINT16 m_clip_br0;
	UINT8 *m_tsrc0;
	UINT8 *m_tsrc_s0;
	UINT32 m_x_count0;
	UINT32 m_x_zoom0;
	UINT16 *m_src1;
	UINT16 *m_src_s1;
	UINT16 *m_src_e1;
	UINT16 m_clip_al1;
	UINT16 m_clip_ar1;
	UINT16 m_clip_bl1;
	UINT16 m_clip_br1;
	UINT8 *m_tsrc1;
	UINT8 *m_tsrc_s1;
	UINT32 m_x_count1;
	UINT32 m_x_zoom1;
	UINT16 *m_src2;
	UINT16 *m_src_s2;
	UINT16 *m_src_e2;
	UINT16 m_clip_al2;
	UINT16 m_clip_ar2;
	UINT16 m_clip_bl2;
	UINT16 m_clip_br2;
	UINT8 *m_tsrc2;
	UINT8 *m_tsrc_s2;
	UINT32 m_x_count2;
	UINT32 m_x_zoom2;
	UINT16 *m_src3;
	UINT16 *m_src_s3;
	UINT16 *m_src_e3;
	UINT16 m_clip_al3;
	UINT16 m_clip_ar3;
	UINT16 m_clip_bl3;
	UINT16 m_clip_br3;
	UINT8 *m_tsrc3;
	UINT8 *m_tsrc_s3;
	UINT32 m_x_count3;
	UINT32 m_x_zoom3;
	UINT16 *m_src4;
	UINT16 *m_src_s4;
	UINT16 *m_src_e4;
	UINT16 m_clip_al4;
	UINT16 m_clip_ar4;
	UINT16 m_clip_bl4;
	UINT16 m_clip_br4;
	UINT8 *m_tsrc4;
	UINT8 *m_tsrc_s4;
	UINT32 m_x_count4;
	UINT32 m_x_zoom4;

	UINT8 m_add_sat[256][256];

	int m_f3_alpha_level_2as;
	int m_f3_alpha_level_2ad;
	int m_f3_alpha_level_3as;
	int m_f3_alpha_level_3ad;
	int m_f3_alpha_level_2bs;
	int m_f3_alpha_level_2bd;
	int m_f3_alpha_level_3bs;
	int m_f3_alpha_level_3bd;
	int m_alpha_level_last;

	int m_alpha_s_1_1;
	int m_alpha_s_1_2;
	int m_alpha_s_1_4;
	int m_alpha_s_1_5;
	int m_alpha_s_1_6;
	int m_alpha_s_1_8;
	int m_alpha_s_1_9;
	int m_alpha_s_1_a;
	int m_alpha_s_2a_0;
	int m_alpha_s_2a_4;
	int m_alpha_s_2a_8;
	int m_alpha_s_2b_0;
	int m_alpha_s_2b_4;
	int m_alpha_s_2b_8;
	int m_alpha_s_3a_0;
	int m_alpha_s_3a_1;
	int m_alpha_s_3a_2;
	int m_alpha_s_3b_0;
	int m_alpha_s_3b_1;
	int m_alpha_s_3b_2;
	UINT32 m_dval;
	UINT8 m_pval;
	UINT8 m_tval;
	UINT8 m_pdest_2a;
	UINT8 m_pdest_2b;
	int m_tr_2a;
	int m_tr_2b;
	UINT8 m_pdest_3a;
	UINT8 m_pdest_3b;
	int m_tr_3a;
	int m_tr_3b;

#define BYTE4_XOR_LE(x)	x


struct f3_playfield_line_inf
{
	int alpha_mode[256];
	int pri[256];

	/* use for draw_scanlines */
	UINT16 *src[256],*src_s[256],*src_e[256];
	UINT8 *tsrc[256],*tsrc_s[256];
	int x_count[256];
	UINT32 x_zoom[256];
	UINT32 clip0[256];
	UINT32 clip1[256];
};

struct f3_spritealpha_line_inf
{
	UINT16 alpha_level[256];
	UINT16 spri[256];
	UINT16 sprite_alpha[256];
	UINT32 sprite_clip0[256];
	UINT32 sprite_clip1[256];
	INT16 clip0_l[256];
	INT16 clip0_r[256];
	INT16 clip1_l[256];
	INT16 clip1_r[256];
};



static struct f3_playfield_line_inf m_pf_line_inf[5];
static struct f3_spritealpha_line_inf m_sa_line_inf[1];


static void clear_f3_stuff()
{
	memset(&m_pf_line_inf, 0, sizeof(m_pf_line_inf));
	memset(&m_sa_line_inf, 0, sizeof(m_sa_line_inf));
}


static int (*m_dpix_n[8][16])(UINT32 s_pix);
static int (**m_dpix_lp[5])(UINT32 s_pix);
static int (**m_dpix_sp[9])(UINT32 s_pix);





/*============================================================================*/

#define SET_ALPHA_LEVEL(d,s)            \
{                                       \
	int level = s;                      \
	if(level == 0) level = -1;          \
	d = level+1;                        \
}

inline void f3_alpha_set_level()
{
//  SET_ALPHA_LEVEL(m_alpha_s_1_1, m_f3_alpha_level_2ad)
	SET_ALPHA_LEVEL(m_alpha_s_1_1, 255-m_f3_alpha_level_2as)
//  SET_ALPHA_LEVEL(m_alpha_s_1_2, m_f3_alpha_level_2bd)
	SET_ALPHA_LEVEL(m_alpha_s_1_2, 255-m_f3_alpha_level_2bs)
	SET_ALPHA_LEVEL(m_alpha_s_1_4, m_f3_alpha_level_3ad)
//  SET_ALPHA_LEVEL(m_alpha_s_1_5, m_f3_alpha_level_3ad*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_5, m_f3_alpha_level_3ad*(255-m_f3_alpha_level_2as)/255)
//  SET_ALPHA_LEVEL(m_alpha_s_1_6, m_f3_alpha_level_3ad*m_f3_alpha_level_2bd/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_6, m_f3_alpha_level_3ad*(255-m_f3_alpha_level_2bs)/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_8, m_f3_alpha_level_3bd)
//  SET_ALPHA_LEVEL(m_alpha_s_1_9, m_f3_alpha_level_3bd*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_9, m_f3_alpha_level_3bd*(255-m_f3_alpha_level_2as)/255)
//  SET_ALPHA_LEVEL(m_alpha_s_1_a, m_f3_alpha_level_3bd*m_f3_alpha_level_2bd/255)
	SET_ALPHA_LEVEL(m_alpha_s_1_a, m_f3_alpha_level_3bd*(255-m_f3_alpha_level_2bs)/255)

	SET_ALPHA_LEVEL(m_alpha_s_2a_0, m_f3_alpha_level_2as)
	SET_ALPHA_LEVEL(m_alpha_s_2a_4, m_f3_alpha_level_2as*m_f3_alpha_level_3ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_2a_8, m_f3_alpha_level_2as*m_f3_alpha_level_3bd/255)

	SET_ALPHA_LEVEL(m_alpha_s_2b_0, m_f3_alpha_level_2bs)
	SET_ALPHA_LEVEL(m_alpha_s_2b_4, m_f3_alpha_level_2bs*m_f3_alpha_level_3ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_2b_8, m_f3_alpha_level_2bs*m_f3_alpha_level_3bd/255)

	SET_ALPHA_LEVEL(m_alpha_s_3a_0, m_f3_alpha_level_3as)
	SET_ALPHA_LEVEL(m_alpha_s_3a_1, m_f3_alpha_level_3as*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_3a_2, m_f3_alpha_level_3as*m_f3_alpha_level_2bd/255)

	SET_ALPHA_LEVEL(m_alpha_s_3b_0, m_f3_alpha_level_3bs)
	SET_ALPHA_LEVEL(m_alpha_s_3b_1, m_f3_alpha_level_3bs*m_f3_alpha_level_2ad/255)
	SET_ALPHA_LEVEL(m_alpha_s_3b_2, m_f3_alpha_level_3bs*m_f3_alpha_level_2bd/255)
}
#undef SET_ALPHA_LEVEL

#define COLOR1 BYTE4_XOR_LE(0)
#define COLOR2 BYTE4_XOR_LE(1)
#define COLOR3 BYTE4_XOR_LE(2)

inline void f3_alpha_blend32_s(int alphas, uint32_t s)
{
	uint8_t *sc = (uint8_t *)&s;
	uint8_t *dc = (uint8_t *)&m_dval;
	dc[COLOR1] = (alphas * sc[COLOR1]) >> 8;
	dc[COLOR2] = (alphas * sc[COLOR2]) >> 8;
	dc[COLOR3] = (alphas * sc[COLOR3]) >> 8;
}

inline void f3_alpha_blend32_d(int alphas, uint32_t s)
{
	uint8_t *sc = (uint8_t *)&s;
	uint8_t *dc = (uint8_t *)&m_dval;
	dc[COLOR1] = m_add_sat[dc[COLOR1]][(alphas * sc[COLOR1]) >> 8];
	dc[COLOR2] = m_add_sat[dc[COLOR2]][(alphas * sc[COLOR2]) >> 8];
	dc[COLOR3] = m_add_sat[dc[COLOR3]][(alphas * sc[COLOR3]) >> 8];
}

/*============================================================================*/

inline void f3_alpha_blend_1_1(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_1,s);}
inline void f3_alpha_blend_1_2(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_2,s);}
inline void f3_alpha_blend_1_4(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_4,s);}
inline void f3_alpha_blend_1_5(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_5,s);}
inline void f3_alpha_blend_1_6(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_6,s);}
inline void f3_alpha_blend_1_8(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_8,s);}
inline void f3_alpha_blend_1_9(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_9,s);}
inline void f3_alpha_blend_1_a(uint32_t s){f3_alpha_blend32_d(m_alpha_s_1_a,s);}

inline void f3_alpha_blend_2a_0(uint32_t s){f3_alpha_blend32_s(m_alpha_s_2a_0,s);}
inline void f3_alpha_blend_2a_4(uint32_t s){f3_alpha_blend32_d(m_alpha_s_2a_4,s);}
inline void f3_alpha_blend_2a_8(uint32_t s){f3_alpha_blend32_d(m_alpha_s_2a_8,s);}

inline void f3_alpha_blend_2b_0(uint32_t s){f3_alpha_blend32_s(m_alpha_s_2b_0,s);}
inline void f3_alpha_blend_2b_4(uint32_t s){f3_alpha_blend32_d(m_alpha_s_2b_4,s);}
inline void f3_alpha_blend_2b_8(uint32_t s){f3_alpha_blend32_d(m_alpha_s_2b_8,s);}

inline void f3_alpha_blend_3a_0(uint32_t s){f3_alpha_blend32_s(m_alpha_s_3a_0,s);}
inline void f3_alpha_blend_3a_1(uint32_t s){f3_alpha_blend32_d(m_alpha_s_3a_1,s);}
inline void f3_alpha_blend_3a_2(uint32_t s){f3_alpha_blend32_d(m_alpha_s_3a_2,s);}

inline void f3_alpha_blend_3b_0(uint32_t s){f3_alpha_blend32_s(m_alpha_s_3b_0,s);}
inline void f3_alpha_blend_3b_1(uint32_t s){f3_alpha_blend32_d(m_alpha_s_3b_1,s);}
inline void f3_alpha_blend_3b_2(uint32_t s){f3_alpha_blend32_d(m_alpha_s_3b_2,s);}

/*============================================================================*/

int dpix_1_noalpha(uint32_t s_pix) {m_dval = s_pix; return 1;}
int dpix_ret1(uint32_t /*s_pix*/) {return 1;}
int dpix_ret0(uint32_t /*s_pix*/) {return 0;}
int dpix_1_1(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_1(s_pix); return 1;}
int dpix_1_2(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_2(s_pix); return 1;}
int dpix_1_4(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_4(s_pix); return 1;}
int dpix_1_5(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_5(s_pix); return 1;}
int dpix_1_6(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_6(s_pix); return 1;}
int dpix_1_8(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_8(s_pix); return 1;}
int dpix_1_9(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_9(s_pix); return 1;}
int dpix_1_a(uint32_t s_pix) {if(s_pix) f3_alpha_blend_1_a(s_pix); return 1;}

int dpix_2a_0(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_2a_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_2a) {m_pval |= m_pdest_2a;return 0;}
	return 1;
}
int dpix_2a_4(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_2a_4(s_pix);
	if(m_pdest_2a) {m_pval |= m_pdest_2a;return 0;}
	return 1;
}
int dpix_2a_8(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_2a_8(s_pix);
	if(m_pdest_2a) {m_pval |= m_pdest_2a;return 0;}
	return 1;
}

int dpix_3a_0(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_3a_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_3a) {m_pval |= m_pdest_3a;return 0;}
	return 1;
}
int dpix_3a_1(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_3a_1(s_pix);
	if(m_pdest_3a) {m_pval |= m_pdest_3a;return 0;}
	return 1;
}
int dpix_3a_2(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_3a_2(s_pix);
	if(m_pdest_3a) {m_pval |= m_pdest_3a;return 0;}
	return 1;
}

int dpix_2b_0(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_2b_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_2b) {m_pval |= m_pdest_2b;return 0;}
	return 1;
}
int dpix_2b_4(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_2b_4(s_pix);
	if(m_pdest_2b) {m_pval |= m_pdest_2b;return 0;}
	return 1;
}
int dpix_2b_8(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_2b_8(s_pix);
	if(m_pdest_2b) {m_pval |= m_pdest_2b;return 0;}
	return 1;
}

int dpix_3b_0(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_3b_0(s_pix);
	else      m_dval = 0;
	if(m_pdest_3b) {m_pval |= m_pdest_3b;return 0;}
	return 1;
}
int dpix_3b_1(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_3b_1(s_pix);
	if(m_pdest_3b) {m_pval |= m_pdest_3b;return 0;}
	return 1;
}
int dpix_3b_2(uint32_t s_pix)
{
	if(s_pix) f3_alpha_blend_3b_2(s_pix);
	if(m_pdest_3b) {m_pval |= m_pdest_3b;return 0;}
	return 1;
}

int dpix_2_0(uint32_t s_pix)
{
	uint8_t tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_2b)     {f3_alpha_blend_2b_0(s_pix);if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {f3_alpha_blend_2a_0(s_pix);if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_2b)     {m_dval = 0;if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {m_dval = 0;if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	return 0;
}
int dpix_2_4(uint32_t s_pix)
{
	uint8_t tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_2b)     {f3_alpha_blend_2b_4(s_pix);if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {f3_alpha_blend_2a_4(s_pix);if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_2b)     {if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	return 0;
}
int dpix_2_8(uint32_t s_pix)
{
	uint8_t tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_2b)     {f3_alpha_blend_2b_8(s_pix);if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {f3_alpha_blend_2a_8(s_pix);if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_2b)     {if(m_pdest_2b) m_pval |= m_pdest_2b;else return 1;}
		else if(tr2==m_tr_2a)    {if(m_pdest_2a) m_pval |= m_pdest_2a;else return 1;}
	}
	return 0;
}

int dpix_3_0(uint32_t s_pix)
{
	uint8_t tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_3b)     {f3_alpha_blend_3b_0(s_pix);if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {f3_alpha_blend_3a_0(s_pix);if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_3b)     {m_dval = 0;if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {m_dval = 0;if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	return 0;
}
int dpix_3_1(uint32_t s_pix)
{
	uint8_t tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_3b)     {f3_alpha_blend_3b_1(s_pix);if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {f3_alpha_blend_3a_1(s_pix);if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_3b)     {if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	return 0;
}
int dpix_3_2(uint32_t s_pix)
{
	uint8_t tr2=m_tval&1;
	if(s_pix)
	{
		if(tr2==m_tr_3b)     {f3_alpha_blend_3b_2(s_pix);if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {f3_alpha_blend_3a_2(s_pix);if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	else
	{
		if(tr2==m_tr_3b)     {if(m_pdest_3b) m_pval |= m_pdest_3b;else return 1;}
		else if(tr2==m_tr_3a)    {if(m_pdest_3a) m_pval |= m_pdest_3a;else return 1;}
	}
	return 0;
}

inline void dpix_1_sprite(uint32_t s_pix)
{
	if(s_pix)
	{
		uint8_t p1 = m_pval&0xf0;
		if     (p1==0x10)   f3_alpha_blend_1_1(s_pix);
		else if(p1==0x20)   f3_alpha_blend_1_2(s_pix);
		else if(p1==0x40)   f3_alpha_blend_1_4(s_pix);
		else if(p1==0x50)   f3_alpha_blend_1_5(s_pix);
		else if(p1==0x60)   f3_alpha_blend_1_6(s_pix);
		else if(p1==0x80)   f3_alpha_blend_1_8(s_pix);
		else if(p1==0x90)   f3_alpha_blend_1_9(s_pix);
		else if(p1==0xa0)   f3_alpha_blend_1_a(s_pix);
	}
}

inline void dpix_bg(uint32_t bgcolor)
{
	uint8_t p1 = m_pval&0xf0;
	if(!p1)         m_dval = bgcolor;
	else if(p1==0x10)   f3_alpha_blend_1_1(bgcolor);
	else if(p1==0x20)   f3_alpha_blend_1_2(bgcolor);
	else if(p1==0x40)   f3_alpha_blend_1_4(bgcolor);
	else if(p1==0x50)   f3_alpha_blend_1_5(bgcolor);
	else if(p1==0x60)   f3_alpha_blend_1_6(bgcolor);
	else if(p1==0x80)   f3_alpha_blend_1_8(bgcolor);
	else if(p1==0x90)   f3_alpha_blend_1_9(bgcolor);
	else if(p1==0xa0)   f3_alpha_blend_1_a(bgcolor);
}

/******************************************************************************/

int alpha_blend_inited = 0;

void init_alpha_blend_func()
{
	alpha_blend_inited = 1;

	m_dpix_n[0][0x0]=&dpix_1_noalpha;
	m_dpix_n[0][0x1]=&dpix_1_noalpha;
	m_dpix_n[0][0x2]=&dpix_1_noalpha;
	m_dpix_n[0][0x3]=&dpix_1_noalpha;
	m_dpix_n[0][0x4]=&dpix_1_noalpha;
	m_dpix_n[0][0x5]=&dpix_1_noalpha;
	m_dpix_n[0][0x6]=&dpix_1_noalpha;
	m_dpix_n[0][0x7]=&dpix_1_noalpha;
	m_dpix_n[0][0x8]=&dpix_1_noalpha;
	m_dpix_n[0][0x9]=&dpix_1_noalpha;
	m_dpix_n[0][0xa]=&dpix_1_noalpha;
	m_dpix_n[0][0xb]=&dpix_1_noalpha;
	m_dpix_n[0][0xc]=&dpix_1_noalpha;
	m_dpix_n[0][0xd]=&dpix_1_noalpha;
	m_dpix_n[0][0xe]=&dpix_1_noalpha;
	m_dpix_n[0][0xf]=&dpix_1_noalpha;

	m_dpix_n[1][0x0]=&dpix_1_noalpha;
	m_dpix_n[1][0x1]=&dpix_1_1;
	m_dpix_n[1][0x2]=&dpix_1_2;
	m_dpix_n[1][0x3]=&dpix_ret1;
	m_dpix_n[1][0x4]=&dpix_1_4;
	m_dpix_n[1][0x5]=&dpix_1_5;
	m_dpix_n[1][0x6]=&dpix_1_6;
	m_dpix_n[1][0x7]=&dpix_ret1;
	m_dpix_n[1][0x8]=&dpix_1_8;
	m_dpix_n[1][0x9]=&dpix_1_9;
	m_dpix_n[1][0xa]=&dpix_1_a;
	m_dpix_n[1][0xb]=&dpix_ret1;
	m_dpix_n[1][0xc]=&dpix_ret1;
	m_dpix_n[1][0xd]=&dpix_ret1;
	m_dpix_n[1][0xe]=&dpix_ret1;
	m_dpix_n[1][0xf]=&dpix_ret1;

	m_dpix_n[2][0x0]=&dpix_2a_0;
	m_dpix_n[2][0x1]=&dpix_ret0;
	m_dpix_n[2][0x2]=&dpix_ret0;
	m_dpix_n[2][0x3]=&dpix_ret0;
	m_dpix_n[2][0x4]=&dpix_2a_4;
	m_dpix_n[2][0x5]=&dpix_ret0;
	m_dpix_n[2][0x6]=&dpix_ret0;
	m_dpix_n[2][0x7]=&dpix_ret0;
	m_dpix_n[2][0x8]=&dpix_2a_8;
	m_dpix_n[2][0x9]=&dpix_ret0;
	m_dpix_n[2][0xa]=&dpix_ret0;
	m_dpix_n[2][0xb]=&dpix_ret0;
	m_dpix_n[2][0xc]=&dpix_ret0;
	m_dpix_n[2][0xd]=&dpix_ret0;
	m_dpix_n[2][0xe]=&dpix_ret0;
	m_dpix_n[2][0xf]=&dpix_ret0;

	m_dpix_n[3][0x0]=&dpix_3a_0;
	m_dpix_n[3][0x1]=&dpix_3a_1;
	m_dpix_n[3][0x2]=&dpix_3a_2;
	m_dpix_n[3][0x3]=&dpix_ret0;
	m_dpix_n[3][0x4]=&dpix_ret0;
	m_dpix_n[3][0x5]=&dpix_ret0;
	m_dpix_n[3][0x6]=&dpix_ret0;
	m_dpix_n[3][0x7]=&dpix_ret0;
	m_dpix_n[3][0x8]=&dpix_ret0;
	m_dpix_n[3][0x9]=&dpix_ret0;
	m_dpix_n[3][0xa]=&dpix_ret0;
	m_dpix_n[3][0xb]=&dpix_ret0;
	m_dpix_n[3][0xc]=&dpix_ret0;
	m_dpix_n[3][0xd]=&dpix_ret0;
	m_dpix_n[3][0xe]=&dpix_ret0;
	m_dpix_n[3][0xf]=&dpix_ret0;

	m_dpix_n[4][0x0]=&dpix_2b_0;
	m_dpix_n[4][0x1]=&dpix_ret0;
	m_dpix_n[4][0x2]=&dpix_ret0;
	m_dpix_n[4][0x3]=&dpix_ret0;
	m_dpix_n[4][0x4]=&dpix_2b_4;
	m_dpix_n[4][0x5]=&dpix_ret0;
	m_dpix_n[4][0x6]=&dpix_ret0;
	m_dpix_n[4][0x7]=&dpix_ret0;
	m_dpix_n[4][0x8]=&dpix_2b_8;
	m_dpix_n[4][0x9]=&dpix_ret0;
	m_dpix_n[4][0xa]=&dpix_ret0;
	m_dpix_n[4][0xb]=&dpix_ret0;
	m_dpix_n[4][0xc]=&dpix_ret0;
	m_dpix_n[4][0xd]=&dpix_ret0;
	m_dpix_n[4][0xe]=&dpix_ret0;
	m_dpix_n[4][0xf]=&dpix_ret0;

	m_dpix_n[5][0x0]=&dpix_3b_0;
	m_dpix_n[5][0x1]=&dpix_3b_1;
	m_dpix_n[5][0x2]=&dpix_3b_2;
	m_dpix_n[5][0x3]=&dpix_ret0;
	m_dpix_n[5][0x4]=&dpix_ret0;
	m_dpix_n[5][0x5]=&dpix_ret0;
	m_dpix_n[5][0x6]=&dpix_ret0;
	m_dpix_n[5][0x7]=&dpix_ret0;
	m_dpix_n[5][0x8]=&dpix_ret0;
	m_dpix_n[5][0x9]=&dpix_ret0;
	m_dpix_n[5][0xa]=&dpix_ret0;
	m_dpix_n[5][0xb]=&dpix_ret0;
	m_dpix_n[5][0xc]=&dpix_ret0;
	m_dpix_n[5][0xd]=&dpix_ret0;
	m_dpix_n[5][0xe]=&dpix_ret0;
	m_dpix_n[5][0xf]=&dpix_ret0;

	m_dpix_n[6][0x0]=&dpix_2_0;
	m_dpix_n[6][0x1]=&dpix_ret0;
	m_dpix_n[6][0x2]=&dpix_ret0;
	m_dpix_n[6][0x3]=&dpix_ret0;
	m_dpix_n[6][0x4]=&dpix_2_4;
	m_dpix_n[6][0x5]=&dpix_ret0;
	m_dpix_n[6][0x6]=&dpix_ret0;
	m_dpix_n[6][0x7]=&dpix_ret0;
	m_dpix_n[6][0x8]=&dpix_2_8;
	m_dpix_n[6][0x9]=&dpix_ret0;
	m_dpix_n[6][0xa]=&dpix_ret0;
	m_dpix_n[6][0xb]=&dpix_ret0;
	m_dpix_n[6][0xc]=&dpix_ret0;
	m_dpix_n[6][0xd]=&dpix_ret0;
	m_dpix_n[6][0xe]=&dpix_ret0;
	m_dpix_n[6][0xf]=&dpix_ret0;

	m_dpix_n[7][0x0]=&dpix_3_0;
	m_dpix_n[7][0x1]=&dpix_3_1;
	m_dpix_n[7][0x2]=&dpix_3_2;
	m_dpix_n[7][0x3]=&dpix_ret0;
	m_dpix_n[7][0x4]=&dpix_ret0;
	m_dpix_n[7][0x5]=&dpix_ret0;
	m_dpix_n[7][0x6]=&dpix_ret0;
	m_dpix_n[7][0x7]=&dpix_ret0;
	m_dpix_n[7][0x8]=&dpix_ret0;
	m_dpix_n[7][0x9]=&dpix_ret0;
	m_dpix_n[7][0xa]=&dpix_ret0;
	m_dpix_n[7][0xb]=&dpix_ret0;
	m_dpix_n[7][0xc]=&dpix_ret0;
	m_dpix_n[7][0xd]=&dpix_ret0;
	m_dpix_n[7][0xe]=&dpix_ret0;
	m_dpix_n[7][0xf]=&dpix_ret0;

	for(int i = 0; i < 256; i++)
		for(int j = 0; j < 256; j++)
			m_add_sat[i][j] = (i + j < 256) ? i + j : 255;
}

/******************************************************************************/

#define GET_PIXMAP_POINTER(pf_num) \
{ \
	const struct f3_playfield_line_inf *line_tmp=line_t[pf_num]; \
	m_src##pf_num=line_tmp->src[y]; \
	m_src_s##pf_num=line_tmp->src_s[y]; \
	m_src_e##pf_num=line_tmp->src_e[y]; \
	m_tsrc##pf_num=line_tmp->tsrc[y]; \
	m_tsrc_s##pf_num=line_tmp->tsrc_s[y]; \
	m_x_count##pf_num=line_tmp->x_count[y]; \
	m_x_zoom##pf_num=line_tmp->x_zoom[y]; \
	m_clip_al##pf_num=line_tmp->clip0[y]&0xffff; \
	m_clip_ar##pf_num=line_tmp->clip0[y]>>16; \
	m_clip_bl##pf_num=line_tmp->clip1[y]&0xffff; \
	m_clip_br##pf_num=line_tmp->clip1[y]>>16; \
}

#define CULC_PIXMAP_POINTER(pf_num) \
{ \
	m_x_count##pf_num += m_x_zoom##pf_num; \
	if(m_x_count##pf_num>>16) \
	{ \
		m_x_count##pf_num &= 0xffff; \
		m_src##pf_num++; \
		m_tsrc##pf_num++; \
		if(m_src##pf_num==m_src_e##pf_num) {m_src##pf_num=m_src_s##pf_num; m_tsrc##pf_num=m_tsrc_s##pf_num;} \
	} \
}

#define UPDATE_PIXMAP_SP(pf_num)	\
if(cx>=clip_als && cx<clip_ars && !(cx>=clip_bls && cx<clip_brs)) \
	{ \
		sprite_pri=sprite[pf_num]&m_pval; \
		if(sprite_pri) \
		{ \
			if(sprite[pf_num]&0x100) break; \
			if(!m_dpix_sp[sprite_pri]) \
			{ \
				if(!(m_pval&0xf0)) break; \
				else {dpix_1_sprite(*dsti);*dsti=m_dval;break;} \
			} \
			if((*m_dpix_sp[sprite_pri][m_pval>>4])(*dsti)) {*dsti=m_dval;break;} \
		} \
	}

#define UPDATE_PIXMAP_LP(pf_num) \
	if (cx>=m_clip_al##pf_num && cx<m_clip_ar##pf_num && !(cx>=m_clip_bl##pf_num && cx<m_clip_br##pf_num)) 	\
	{ \
		m_tval=*m_tsrc##pf_num; \
		if(m_tval&0xf0) \
			if((*m_dpix_lp[pf_num][m_pval>>4])(clut[*m_src##pf_num])) {*dsti=m_dval;break;} \
	}


static void draw_scanlines(int xsize,INT16 *draw_line_num,
							const struct f3_playfield_line_inf **line_t,
							const int *sprite,
							UINT32 orient,
							int skip_layer_num)
{
	UINT32 *clut = TaitoPalette;
	UINT32 bgcolor=clut[0];
	int length;

	const int x=46;

	UINT16 clip_als=0, clip_ars=0, clip_bls=0, clip_brs=0;

	UINT8 *dstp0,*dstp;

	int yadv = 512;
	int yadvp = 1024;
	int i=0,y=draw_line_num[0];
	int ty = y;

#if 1
	if (orient & ORIENTATION_FLIP_Y)
	{
		ty = 512 - 1 - ty;
		yadv = -yadv;
		yadvp = -yadvp;
	}
#endif

	

	dstp0 = TaitoPriorityMap + (ty * 1024) + x;

	m_pdest_2a = m_f3_alpha_level_2ad ? 0x10 : 0;
	m_pdest_2b = m_f3_alpha_level_2bd ? 0x20 : 0;
	m_tr_2a =(m_f3_alpha_level_2as==0 && m_f3_alpha_level_2ad==255) ? -1 : 0;
	m_tr_2b =(m_f3_alpha_level_2bs==0 && m_f3_alpha_level_2bd==255) ? -1 : 1;
	m_pdest_3a = m_f3_alpha_level_3ad ? 0x40 : 0;
	m_pdest_3b = m_f3_alpha_level_3bd ? 0x80 : 0;
	m_tr_3a =(m_f3_alpha_level_3as==0 && m_f3_alpha_level_3ad==255) ? -1 : 0;
	m_tr_3b =(m_f3_alpha_level_3bs==0 && m_f3_alpha_level_3bd==255) ? -1 : 1;

	{
		UINT32 *dsti0,*dsti;
		dsti0 = output_bitmap + (ty * 512) + x;
		while(1)
		{
			int cx=0;

			clip_als=m_sa_line_inf[0].sprite_clip0[y]&0xffff;
			clip_ars=m_sa_line_inf[0].sprite_clip0[y]>>16;
			clip_bls=m_sa_line_inf[0].sprite_clip1[y]&0xffff;
			clip_brs=m_sa_line_inf[0].sprite_clip1[y]>>16;

			length=xsize;
			dsti = dsti0;
			dstp = dstp0;

			switch(skip_layer_num)
			{
				case 0: GET_PIXMAP_POINTER(0)
				case 1: GET_PIXMAP_POINTER(1)
				case 2: GET_PIXMAP_POINTER(2)
				case 3: GET_PIXMAP_POINTER(3)
				case 4: GET_PIXMAP_POINTER(4)
			}

			while (1)
			{
				m_pval=*dstp;
				if (m_pval!=0xff)
				{
					UINT8 sprite_pri;
					switch(skip_layer_num)
					{
						case 0: UPDATE_PIXMAP_SP(0) UPDATE_PIXMAP_LP(0)
						case 1: UPDATE_PIXMAP_SP(1) UPDATE_PIXMAP_LP(1)
						case 2: UPDATE_PIXMAP_SP(2) UPDATE_PIXMAP_LP(2)
						case 3: UPDATE_PIXMAP_SP(3) UPDATE_PIXMAP_LP(3)
						case 4: UPDATE_PIXMAP_SP(4) UPDATE_PIXMAP_LP(4)
						case 5: UPDATE_PIXMAP_SP(5)
								if(!bgcolor) {if(!(m_pval&0xf0)) {*dsti=0;break;}}
								else dpix_bg(bgcolor);
								*dsti=m_dval;
					}
				}

				if(!(--length)) break;
				dsti++;
				dstp++;
				cx++;

				switch(skip_layer_num)
				{
					case 0: CULC_PIXMAP_POINTER(0)
					case 1: CULC_PIXMAP_POINTER(1)
					case 2: CULC_PIXMAP_POINTER(2)
					case 3: CULC_PIXMAP_POINTER(3)
					case 4: CULC_PIXMAP_POINTER(4)
				}
			}

			i++;
			if(draw_line_num[i]<0) break;
			if(draw_line_num[i]==y+1)
			{
				dsti0 += yadv;
				dstp0 += yadvp;
				y++;
				continue;
			}
			else
			{
				dsti0 += (draw_line_num[i]-y)*yadv;
				dstp0 += (draw_line_num[i]-y)*yadvp;
				y=draw_line_num[i];
			}
		}
	}
}
#undef GET_PIXMAP_POINTER
#undef CULC_PIXMAP_POINTER

static void visible_tile_check(
						struct f3_playfield_line_inf *line_t,
						int line,
						UINT32 x_index_fx,UINT32 y_index,
						UINT16 *f3_pf_data_n)
{
	UINT16 *pf_base;
	int i,trans_all,tile_index,tile_num;
	int alpha_type,alpha_mode;
	int opaque_all;
	int total_elements;

	alpha_mode=line_t->alpha_mode[line];
	if(!alpha_mode) return;

	total_elements = tileno_mask; // iq_132

	tile_index=x_index_fx>>16;
	tile_num=(((line_t->x_zoom[line]*320+(x_index_fx & 0xffff)+0xffff)>>16)+(tile_index%16)+15)/16;
	tile_index/=16;

	if (m_flipscreen)
	{
		pf_base=f3_pf_data_n+((31-(y_index/16))<<m_twidth_mask_bit);
		tile_index=(m_twidth_mask-tile_index)-tile_num+1;
	}
	else pf_base=f3_pf_data_n+((y_index/16)<<m_twidth_mask_bit);

	trans_all=1;
	opaque_all=1;
	alpha_type=0;
	for(i=0;i<tile_num;i++)
	{
		UINT32 tile=(pf_base[(tile_index*2+0)&m_twidth_mask]<<16)|(pf_base[(tile_index*2+1)&m_twidth_mask]);
		UINT8  extra_planes = (tile>>(16+10)) & 3;
		if(tile&0xffff)
		{
			trans_all=0;
			if(opaque_all)
			{
				if(m_tile_opaque_pf[extra_planes][(tile&0xffff)%total_elements]!=1) opaque_all=0;
			}

			if(alpha_mode==1)
			{
				if(!opaque_all) return;
			}
			else
			{
				if(alpha_type!=3)
				{
					if((tile>>(16+9))&1) alpha_type|=2;
					else                 alpha_type|=1;
				}
				else if(!opaque_all) break;
			}
		}
		else if(opaque_all) opaque_all=0;

		tile_index++;
	}

	if(trans_all)   {line_t->alpha_mode[line]=0;return;}

	if(alpha_mode>1)
	{
		line_t->alpha_mode[line]|=alpha_type<<4;
	}

	if(opaque_all)
		line_t->alpha_mode[line]|=0x80;
}

static void calculate_clip(int y, UINT16 pri, UINT32* clip0, UINT32* clip1, int* line_enable)
{
	const struct f3_spritealpha_line_inf *sa_line_t=&m_sa_line_inf[0];

	switch (pri)
	{
	case 0x0100: /* Clip plane 1 enable */
		{
			if (sa_line_t->clip0_l[y] > sa_line_t->clip0_r[y])
				*line_enable=0;
			else
				*clip0=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);
			*clip1=0;
		}
		break;
	case 0x0110: /* Clip plane 1 enable, inverted */
		{
			*clip1=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);
			*clip0=0x7fff0000;
		}
		break;
	case 0x0200: /* Clip plane 2 enable */
		{
			if (sa_line_t->clip1_l[y] > sa_line_t->clip1_r[y])
				*line_enable=0;
			else
				*clip0=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);
			*clip1=0;
		}
		break;
	case 0x0220: /* Clip plane 2 enable, inverted */
		{
			*clip1=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);
			*clip0=0x7fff0000;
		}
		break;
	case 0x0300: /* Clip plane 1 & 2 enable */
		{
			int clipl=0,clipr=0;

			if (sa_line_t->clip1_l[y] > sa_line_t->clip0_l[y])
				clipl=sa_line_t->clip1_l[y];
			else
				clipl=sa_line_t->clip0_l[y];

			if (sa_line_t->clip1_r[y] < sa_line_t->clip0_r[y])
				clipr=sa_line_t->clip1_r[y];
			else
				clipr=sa_line_t->clip0_r[y];

			if (clipl > clipr)
				*line_enable=0;
			else
				*clip0=(clipl) | (clipr<<16);
			*clip1=0;
		}
		break;
	case 0x0310: /* Clip plane 1 & 2 enable, plane 1 inverted */
		{
			if (sa_line_t->clip1_l[y] > sa_line_t->clip1_r[y])
				line_enable=NULL;
			else
				*clip0=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);

			*clip1=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);
		}
		break;
	case 0x0320: /* Clip plane 1 & 2 enable, plane 2 inverted */
		{
			if (sa_line_t->clip0_l[y] > sa_line_t->clip0_r[y])
				line_enable=NULL;
			else
				*clip0=(sa_line_t->clip0_l[y]) | (sa_line_t->clip0_r[y]<<16);

			*clip1=(sa_line_t->clip1_l[y]) | (sa_line_t->clip1_r[y]<<16);
		}
		break;
	case 0x0330: /* Clip plane 1 & 2 enable, both inverted */
		{
			int clipl=0,clipr=0;

			if (sa_line_t->clip1_l[y] < sa_line_t->clip0_l[y])
				clipl=sa_line_t->clip1_l[y];
			else
				clipl=sa_line_t->clip0_l[y];

			if (sa_line_t->clip1_r[y] > sa_line_t->clip0_r[y])
				clipr=sa_line_t->clip1_r[y];
			else
				clipr=sa_line_t->clip0_r[y];

			if (clipl > clipr)
				*line_enable=0;
			else
				*clip1=(clipl) | (clipr<<16);
			*clip0=0x7fff0000;
		}
		break;
	default:
		// popmessage("Illegal clip mode");
		break;
	}
}

static void get_line_ram_info(INT32 which_map, int sx, int sy, int pos, UINT16 *f3_pf_data_n)
{
	UINT16 *m_f3_line_ram = (UINT16*)DrvLineRAM;
	struct f3_playfield_line_inf *line_t=&m_pf_line_inf[pos];

	int y,y_start,y_end,y_inc;
	int line_base,zoom_base,col_base,pri_base,inc;

	int line_enable;
	int colscroll=0,x_offset=0,line_zoom=0;
	UINT32 _y_zoom[256];
	UINT16 pri=0;
	int bit_select=1<<pos;

	int _colscroll[256];
	UINT32 _x_offset[256];
	int y_index_fx;

	sx+=((46<<16));

	if (m_flipscreen)
	{
		line_base=0xa1fe + (pos*0x200);
		zoom_base=0x81fe;// + (pos*0x200);
		col_base =0x41fe + (pos*0x200);
		pri_base =0xb1fe + (pos*0x200);
		inc=-2;
		y_start=255;
		y_end=-1;
		y_inc=-1;

		if (extended_layers)    sx=-sx+(((188-512)&0xffff)<<16); else sx=-sx+(188<<16); /* Adjust for flipped scroll position */
		y_index_fx=-sy-(256<<16); /* Adjust for flipped scroll position */
	}
	else
	{
		line_base=0xa000 + (pos*0x200);
		zoom_base=0x8000;// + (pos*0x200);
		col_base =0x4000 + (pos*0x200);
		pri_base =0xb000 + (pos*0x200);
		inc=2;
		y_start=0;
		y_end=256;
		y_inc=1;

		y_index_fx=sy;
	}

	y=y_start;

	while(y!=y_end)
	{
		/* The zoom, column and row values can latch according to control ram */
		{
			if (m_f3_line_ram[0x600+(y)]&bit_select)
				x_offset=(m_f3_line_ram[line_base/2]&0xffff)<<10;
			if (m_f3_line_ram[0x700+(y)]&bit_select)
				pri=m_f3_line_ram[pri_base/2]&0xffff;

			// Zoom for playfields 1 & 3 is interleaved, as is the latch select
			switch (pos)
			{
			case 0:
				if (m_f3_line_ram[0x400+(y)]&bit_select)
					line_zoom=m_f3_line_ram[(zoom_base+0x000)/2]&0xffff;
				break;
			case 1:
				if (m_f3_line_ram[0x400+(y)]&0x2)
					line_zoom=((m_f3_line_ram[(zoom_base+0x200)/2]&0xffff)&0xff00) | (line_zoom&0x00ff);
				if (m_f3_line_ram[0x400+(y)]&0x8)
					line_zoom=((m_f3_line_ram[(zoom_base+0x600)/2]&0xffff)&0x00ff) | (line_zoom&0xff00);
				break;
			case 2:
				if (m_f3_line_ram[0x400+(y)]&bit_select)
					line_zoom=m_f3_line_ram[(zoom_base+0x400)/2]&0xffff;
				break;
			case 3:
				if (m_f3_line_ram[0x400+(y)]&0x8)
					line_zoom=((m_f3_line_ram[(zoom_base+0x600)/2]&0xffff)&0xff00) | (line_zoom&0x00ff);
				if (m_f3_line_ram[0x400+(y)]&0x2)
					line_zoom=((m_f3_line_ram[(zoom_base+0x200)/2]&0xffff)&0x00ff) | (line_zoom&0xff00);
				break;
			default:
				break;
			}

			// Column scroll only affects playfields 2 & 3
			if (pos>=2 && m_f3_line_ram[0x000+(y)]&bit_select)
				colscroll=(m_f3_line_ram[col_base/2]>> 0)&0x3ff;
		}

		if (!pri || (!m_flipscreen && y<24) || (m_flipscreen && y>231) ||
			(pri&0xc000)==0xc000 || !(pri&0x2000)/**/)
			line_enable=0;
		else if(pri&0x4000) //alpha1
			line_enable=2;
		else if(pri&0x8000) //alpha2
			line_enable=3;
		else
			line_enable=1;

		_colscroll[y]=colscroll;
		_x_offset[y]=(x_offset&0xffff0000) - (x_offset&0x0000ffff);
		_y_zoom[y] = (line_zoom&0xff) << 9;

		/* Evaluate clipping */
		if (pri&0x0800)
			line_enable=0;
		else if (pri&0x0330)
		{
			//fast path todo - remove line enable
			calculate_clip(y, pri&0x0330, &line_t->clip0[y], &line_t->clip1[y], &line_enable);
		}
		else
		{
			/* No clipping */
			line_t->clip0[y]=0x7fff0000;
			line_t->clip1[y]=0;
		}

		line_t->x_zoom[y]=0x10000 - (line_zoom&0xff00);
		line_t->alpha_mode[y]=line_enable;
		line_t->pri[y]=pri;

		zoom_base+=inc;
		line_base+=inc;
		col_base +=inc;
		pri_base +=inc;
		y +=y_inc;
	}

	UINT8 *pmap = bitmap_flags[which_map];
	UINT16 * tm = bitmap_layer[which_map];
	UINT16 * tmap = bitmap_layer[which_map];
	INT32 map_width = bitmap_width[which_map];

	y=y_start;
	while(y!=y_end)
	{
		UINT32 x_index_fx;
		UINT32 y_index;

		/* The football games use values in the range 0x200-0x3ff where the crowd should be drawn - !?

		   This appears to cause it to reference outside of the normal tilemap RAM area into the unused
		   area on the 32x32 tilemap configuration.. but exactly how isn't understood

		    Until this is understood we're creating additional tilemaps for the otherwise unused area of
		    RAM and forcing it to look up there.

		    the crowd area still seems to 'lag' behind the pitch area however.. but these are the values
		    in ram??
		*/
		int cs = _colscroll[y];

		if (cs&0x200)
		{
			if (extended_layers == 0)
			{
				if (which_map == 2) {
					tmap = bitmap_layer[4]; //m_pf5_tilemap; // pitch -> crowd
					//pmap = bitmap_flags[4]; // breaks hthero & the footy games. keeping for reference. -dink
					//map_width = bitmap_width[4];
				}
				if (which_map == 3) {
					tmap = bitmap_layer[5]; //m_pf6_tilemap; // corruption on goals -> blank (hthero94)
					//pmap = bitmap_flags[5]; // "same as above"
					//map_width = bitmap_width[5];
				}
			}
		}
		else
		{
			tmap = tm;
			map_width = bitmap_width[which_map];
		}

		/* set pixmap pointer */
		UINT16 *srcbitmap = tmap;
		UINT8 *flagsbitmap = pmap;

//		bprintf (0, _T("%d, %d\n"), which_map, bitmap_width[which_map]);

		if(line_t->alpha_mode[y]!=0)
		{
			UINT16 *src_s;
			UINT8 *tsrc_s;

			x_index_fx = (sx+_x_offset[y]-(10*0x10000)+(10*line_t->x_zoom[y]))&((m_width_mask<<16)|0xffff);
			y_index = ((y_index_fx>>16)+_colscroll[y])&0x1ff;

			/* check tile status */
			visible_tile_check(line_t,y,x_index_fx,y_index,f3_pf_data_n);

			/* If clipping enabled for this line have to disable 'all opaque' optimisation */
			if (line_t->clip0[y]!=0x7fff0000 || line_t->clip1[y]!=0)
				line_t->alpha_mode[y]&=~0x80;

			/* set pixmap index */
			line_t->x_count[y]=x_index_fx & 0xffff; // Fractional part
			line_t->src_s[y]=src_s=srcbitmap + (y_index * map_width);
			line_t->src_e[y]=&src_s[m_width_mask+1];
			line_t->src[y]=&src_s[x_index_fx>>16];

			line_t->tsrc_s[y]=tsrc_s=flagsbitmap + (y_index * map_width);
			line_t->tsrc[y]=&tsrc_s[x_index_fx>>16];
		}

		y_index_fx += _y_zoom[y];
		y +=y_inc;
	}
}

static void get_vram_info(int sx, int sy)
{
	UINT16 *m_f3_line_ram = (UINT16*)DrvLineRAM;

	const struct f3_spritealpha_line_inf *sprite_alpha_line_t=&m_sa_line_inf[0];
	struct f3_playfield_line_inf *line_t=&m_pf_line_inf[4];

	int y,y_start,y_end,y_inc;
	int pri_base,inc;

	int line_enable;

	UINT16 pri=0;

	const int vram_width_mask=0x3ff;

	if (m_flipscreen)
	{
		pri_base =0x73fe;
		inc=-2;
		y_start=255;
		y_end=-1;
		y_inc=-1;
	}
	else
	{
		pri_base =0x7200;
		inc=2;
		y_start=0;
		y_end=256;
		y_inc=1;
	}

	y=y_start;
	while(y!=y_end)
	{
		/* The zoom, column and row values can latch according to control ram */
		{
			if (m_f3_line_ram[(0x0600/2)+(y)]&0x2)
				pri=(m_f3_line_ram[pri_base/2]&0xffff);
		}

		if (!pri || (!m_flipscreen && y<24) || (m_flipscreen && y>231) ||
			(pri&0xc000)==0xc000 || !(pri&0x2000)/**/)
			line_enable=0;
		else if(pri&0x4000) //alpha1
			line_enable=2;
		else if(pri&0x8000) //alpha2
			line_enable=3;
		else
			line_enable=1;

		line_t->pri[y]=pri;

		/* Evaluate clipping */
		if (pri&0x0800)
			line_enable=0;
		else if (pri&0x0330)
		{
			//fast path todo - remove line enable
			calculate_clip(y, pri&0x0330, &line_t->clip0[y], &line_t->clip1[y], &line_enable);
		}
		else
		{
			/* No clipping */
			line_t->clip0[y]=0x7fff0000;
			line_t->clip1[y]=0;
		}

		line_t->x_zoom[y]=0x10000;
		line_t->alpha_mode[y]=line_enable;
		if (line_t->alpha_mode[y]>1)
			line_t->alpha_mode[y]|=0x10;

		pri_base +=inc;
		y +=y_inc;
	}

	sx&=0x1ff;

	/* set pixmap pointer */
	UINT16 *srcbitmap_pixel = bitmap_layer[9];
	UINT8 *flagsbitmap_pixel = bitmap_flags[9];
	UINT16 *srcbitmap_vram = bitmap_layer[8];
	UINT8 *flagsbitmap_vram = bitmap_flags[8];

	y=y_start;
	while(y!=y_end)
	{
		if(line_t->alpha_mode[y]!=0)
		{
			UINT16 *src_s;
			UINT8 *tsrc_s;

			// These bits in control ram indicate whether the line is taken from
			// the VRAM tilemap layer or pixel layer.
			const int usePixelLayer=((sprite_alpha_line_t->sprite_alpha[y]&0xa000)==0xa000);

			/* set pixmap index */
			line_t->x_count[y]=0xffff;
			if (usePixelLayer)
				line_t->src_s[y]=src_s=srcbitmap_pixel + (sy&0xff) * 512;
			else
				line_t->src_s[y]=src_s=srcbitmap_vram + (sy&0x1ff) * 512;
			line_t->src_e[y]=&src_s[vram_width_mask+1];
			line_t->src[y]=&src_s[sx];

			if (usePixelLayer)
				line_t->tsrc_s[y]=tsrc_s=flagsbitmap_pixel + (sy&0xff) * 512;
			else
				line_t->tsrc_s[y]=tsrc_s=flagsbitmap_vram + (sy&0x1ff) * 512;
			line_t->tsrc[y]=&tsrc_s[sx];
		}

		sy++;
		y += y_inc;
	}
}


static void scanline_draw()
{
	int i,j,y,ys,ye;
	int y_start,y_end,y_start_next,y_end_next;
	UINT8 draw_line[256];
	INT16 draw_line_num[256];

	UINT32 rot=0;

	if (m_flipscreen)
	{
		rot=0;//ORIENTATION_FLIP_Y;
		ys=0;
		ye=232;
	}
	else
	{
		ys=24;
		ye=256;
	}

	y_start=ys;
	y_end=ye;
	memset(draw_line,0,256);

	while(1)
	{
		int pos;
		int pri[5],alpha_mode[5],alpha_mode_flag[5],alpha_level;
		UINT16 sprite_alpha;
		UINT8 sprite_alpha_check;
		UINT8 sprite_alpha_all_2a;
		int spri;
		int alpha;
		int layer_tmp[5];
		struct f3_playfield_line_inf *pf_line_inf = m_pf_line_inf;
		struct f3_spritealpha_line_inf *sa_line_inf = m_sa_line_inf;
		int count_skip_layer=0;
		int sprite[6]={0,0,0,0,0,0};
		const struct f3_playfield_line_inf *line_t[5];


		/* find same status of scanlines */
		pri[0]=pf_line_inf[0].pri[y_start];
		pri[1]=pf_line_inf[1].pri[y_start];
		pri[2]=pf_line_inf[2].pri[y_start];
		pri[3]=pf_line_inf[3].pri[y_start];
		pri[4]=pf_line_inf[4].pri[y_start];
		alpha_mode[0]=pf_line_inf[0].alpha_mode[y_start];
		alpha_mode[1]=pf_line_inf[1].alpha_mode[y_start];
		alpha_mode[2]=pf_line_inf[2].alpha_mode[y_start];
		alpha_mode[3]=pf_line_inf[3].alpha_mode[y_start];
		alpha_mode[4]=pf_line_inf[4].alpha_mode[y_start];
		alpha_level=sa_line_inf[0].alpha_level[y_start];
		spri=sa_line_inf[0].spri[y_start];
		sprite_alpha=sa_line_inf[0].sprite_alpha[y_start];

		draw_line[y_start]=1;
		draw_line_num[i=0]=y_start;
		y_start_next=-1;
		y_end_next=-1;
		for(y=y_start+1;y<y_end;y++)
		{
			if(!draw_line[y])
			{
				if(pri[0]!=pf_line_inf[0].pri[y]) y_end_next=y+1;
				else if(pri[1]!=pf_line_inf[1].pri[y]) y_end_next=y+1;
				else if(pri[2]!=pf_line_inf[2].pri[y]) y_end_next=y+1;
				else if(pri[3]!=pf_line_inf[3].pri[y]) y_end_next=y+1;
				else if(pri[4]!=pf_line_inf[4].pri[y]) y_end_next=y+1;
				else if(alpha_mode[0]!=pf_line_inf[0].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[1]!=pf_line_inf[1].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[2]!=pf_line_inf[2].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[3]!=pf_line_inf[3].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_mode[4]!=pf_line_inf[4].alpha_mode[y]) y_end_next=y+1;
				else if(alpha_level!=sa_line_inf[0].alpha_level[y]) y_end_next=y+1;
				else if(spri!=sa_line_inf[0].spri[y]) y_end_next=y+1;
				else if(sprite_alpha!=sa_line_inf[0].sprite_alpha[y]) y_end_next=y+1;
				else
				{
					draw_line[y]=1;
					draw_line_num[++i]=y;
					continue;
				}

				if(y_start_next<0) y_start_next=y;
			}
		}
		y_end=y_end_next;
		y_start=y_start_next;
		draw_line_num[++i]=-1;

		/* alpha blend */
		alpha_mode_flag[0]=alpha_mode[0]&~3;
		alpha_mode_flag[1]=alpha_mode[1]&~3;
		alpha_mode_flag[2]=alpha_mode[2]&~3;
		alpha_mode_flag[3]=alpha_mode[3]&~3;
		alpha_mode_flag[4]=alpha_mode[4]&~3;
		alpha_mode[0]&=3;
		alpha_mode[1]&=3;
		alpha_mode[2]&=3;
		alpha_mode[3]&=3;
		alpha_mode[4]&=3;
		if( alpha_mode[0]>1 ||
			alpha_mode[1]>1 ||
			alpha_mode[2]>1 ||
			alpha_mode[3]>1 ||
			alpha_mode[4]>1 ||
			(sprite_alpha&0xff) != 0xff  )
		{
			/* set alpha level */
			if(alpha_level!=m_alpha_level_last)
			{
				int al_s,al_d;
				int a=alpha_level;
				int b=(a>>8)&0xf;
				int c=(a>>4)&0xf;
				int d=(a>>0)&0xf;
				a>>=12;

				/* b000 7000 */
				al_s = ( (15-d)*256) / 8;
				al_d = ( (15-b)*256) / 8;
				if(al_s>255) al_s = 255;
				if(al_d>255) al_d = 255;
				m_f3_alpha_level_3as = al_s;
				m_f3_alpha_level_3ad = al_d;
				m_f3_alpha_level_2as = al_d;
				m_f3_alpha_level_2ad = al_s;

				al_s = ( (15-c)*256) / 8;
				al_d = ( (15-a)*256) / 8;
				if(al_s>255) al_s = 255;
				if(al_d>255) al_d = 255;
				m_f3_alpha_level_3bs = al_s;
				m_f3_alpha_level_3bd = al_d;
				m_f3_alpha_level_2bs = al_d;
				m_f3_alpha_level_2bd = al_s;

				f3_alpha_set_level();
				m_alpha_level_last=alpha_level;
			}

			/* set sprite alpha mode */
			sprite_alpha_check=0;
			sprite_alpha_all_2a=1;
			m_dpix_sp[1]=NULL;
			m_dpix_sp[2]=NULL;
			m_dpix_sp[4]=NULL;
			m_dpix_sp[8]=NULL;
			for(i=0;i<4;i++)    /* i = sprite priority offset */
			{
				UINT8 sprite_alpha_mode=(sprite_alpha>>(i*2))&3;
				UINT8 sftbit=1<<i;
				if(m_sprite_pri_usage&sftbit)
				{
					if(sprite_alpha_mode==1)
					{
						if(m_f3_alpha_level_2as==0 && m_f3_alpha_level_2ad==255)
							m_sprite_pri_usage&=~sftbit;  // Disable sprite priority block
						else
						{
							m_dpix_sp[sftbit]=m_dpix_n[2];
							sprite_alpha_check|=sftbit;
						}
					}
					else if(sprite_alpha_mode==2)
					{
						if(sprite_alpha&0xff00)
						{
							if(m_f3_alpha_level_3as==0 && m_f3_alpha_level_3ad==255) m_sprite_pri_usage&=~sftbit;
							else
							{
								m_dpix_sp[sftbit]=m_dpix_n[3];
								sprite_alpha_check|=sftbit;
								sprite_alpha_all_2a=0;
							}
						}
						else
						{
							if(m_f3_alpha_level_3bs==0 && m_f3_alpha_level_3bd==255) m_sprite_pri_usage&=~sftbit;
							else
							{
								m_dpix_sp[sftbit]=m_dpix_n[5];
								sprite_alpha_check|=sftbit;
								sprite_alpha_all_2a=0;
							}
						}
					}
				}
			}


			/* check alpha level */
			for(i=0;i<5;i++)    /* i = playfield num (pos) */
			{
				int alpha_type = (alpha_mode_flag[i]>>4)&3;

				if(alpha_mode[i]==2)
				{
					if(alpha_type==1)
					{
						/* if (m_f3_alpha_level_2as==0   && m_f3_alpha_level_2ad==255)
						 *     alpha_mode[i]=3; alpha_mode_flag[i] |= 0x80;}
						 * will display continue screen in gseeker (mt 00026) */
						if     (m_f3_alpha_level_2as==0   && m_f3_alpha_level_2ad==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_2as==255 && m_f3_alpha_level_2ad==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==2)
					{
						if     (m_f3_alpha_level_2bs==0   && m_f3_alpha_level_2bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_2as==255 && m_f3_alpha_level_2ad==0 &&
								m_f3_alpha_level_2bs==255 && m_f3_alpha_level_2bd==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==3)
					{
						if     (m_f3_alpha_level_2as==0   && m_f3_alpha_level_2ad==255 &&
								m_f3_alpha_level_2bs==0   && m_f3_alpha_level_2bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_2as==255 && m_f3_alpha_level_2ad==0   &&
								m_f3_alpha_level_2bs==255 && m_f3_alpha_level_2bd==0  ) alpha_mode[i]=1;
					}
				}
				else if(alpha_mode[i]==3)
				{
					if(alpha_type==1)
					{
						if     (m_f3_alpha_level_3as==0   && m_f3_alpha_level_3ad==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_3as==255 && m_f3_alpha_level_3ad==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==2)
					{
						if     (m_f3_alpha_level_3bs==0   && m_f3_alpha_level_3bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_3as==255 && m_f3_alpha_level_3ad==0 &&
								m_f3_alpha_level_3bs==255 && m_f3_alpha_level_3bd==0  ) alpha_mode[i]=1;
					}
					else if(alpha_type==3)
					{
						if     (m_f3_alpha_level_3as==0   && m_f3_alpha_level_3ad==255 &&
								m_f3_alpha_level_3bs==0   && m_f3_alpha_level_3bd==255) alpha_mode[i]=0;
						else if(m_f3_alpha_level_3as==255 && m_f3_alpha_level_3ad==0   &&
								m_f3_alpha_level_3bs==255 && m_f3_alpha_level_3bd==0  ) alpha_mode[i]=1;
					}
				}
			}

			if (    (alpha_mode[0]==1 || alpha_mode[0]==2 || !alpha_mode[0]) &&
					(alpha_mode[1]==1 || alpha_mode[1]==2 || !alpha_mode[1]) &&
					(alpha_mode[2]==1 || alpha_mode[2]==2 || !alpha_mode[2]) &&
					(alpha_mode[3]==1 || alpha_mode[3]==2 || !alpha_mode[3]) &&
					(alpha_mode[4]==1 || alpha_mode[4]==2 || !alpha_mode[4]) &&
					sprite_alpha_all_2a                     )
			{
				int alpha_type = (alpha_mode_flag[0] | alpha_mode_flag[1] | alpha_mode_flag[2] | alpha_mode_flag[3])&0x30;
				if(     (alpha_type==0x10 && m_f3_alpha_level_2as==255) ||
						(alpha_type==0x20 && m_f3_alpha_level_2as==255 && m_f3_alpha_level_2bs==255) ||
						(alpha_type==0x30 && m_f3_alpha_level_2as==255 && m_f3_alpha_level_2bs==255)  )
				{
					if(alpha_mode[0]>1) alpha_mode[0]=1;
					if(alpha_mode[1]>1) alpha_mode[1]=1;
					if(alpha_mode[2]>1) alpha_mode[2]=1;
					if(alpha_mode[3]>1) alpha_mode[3]=1;
					if(alpha_mode[4]>1) alpha_mode[4]=1;
					sprite_alpha_check=0;
					m_dpix_sp[1]=NULL;
					m_dpix_sp[2]=NULL;
					m_dpix_sp[4]=NULL;
					m_dpix_sp[8]=NULL;
				}
			}
		}
		else
		{
			sprite_alpha_check=0;
			m_dpix_sp[1]=NULL;
			m_dpix_sp[2]=NULL;
			m_dpix_sp[4]=NULL;
			m_dpix_sp[8]=NULL;
		}



		/* set scanline priority */
		{
			int pri_max_opa=-1;
			for(i=0;i<5;i++)    /* i = playfield num (pos) */
			{
				int p0=pri[i];
				int pri_sl1=p0&0x0f;

				layer_tmp[i]=i + (pri_sl1<<3);

				if(!alpha_mode[i])
				{
					layer_tmp[i]|=0x80;
					count_skip_layer++;
				}
				else if(alpha_mode[i]==1 && (alpha_mode_flag[i]&0x80))
				{
					if(layer_tmp[i]>pri_max_opa) pri_max_opa=layer_tmp[i];
				}
			}

			if(pri_max_opa!=-1)
			{
				if(pri_max_opa>layer_tmp[0]) {layer_tmp[0]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[1]) {layer_tmp[1]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[2]) {layer_tmp[2]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[3]) {layer_tmp[3]|=0x80;count_skip_layer++;}
				if(pri_max_opa>layer_tmp[4]) {layer_tmp[4]|=0x80;count_skip_layer++;}
			}
		}


		/* sort layer_tmp */
		for(i=0;i<4;i++)
		{
			for(j=i+1;j<5;j++)
			{
				if(layer_tmp[i]<layer_tmp[j])
				{
					int temp = layer_tmp[i];
					layer_tmp[i] = layer_tmp[j];
					layer_tmp[j] = temp;
				}
			}
		}


		/* check sprite & layer priority */
		{
			int l0,l1,l2,l3,l4;
			int pri_sp[5];

			l0=layer_tmp[0]>>3;
			l1=layer_tmp[1]>>3;
			l2=layer_tmp[2]>>3;
			l3=layer_tmp[3]>>3;
			l4=layer_tmp[4]>>3;

			pri_sp[0]=spri&0xf;
			pri_sp[1]=(spri>>4)&0xf;
			pri_sp[2]=(spri>>8)&0xf;
			pri_sp[3]=spri>>12;

			for(i=0;i<4;i++)    /* i = sprite priority offset */
			{
				int sp,sflg=1<<i;
				if(!(m_sprite_pri_usage & sflg)) continue;
				sp=pri_sp[i];

				/*
				    sprite priority==playfield priority
				        GSEEKER (plane leaving hangar) --> sprite
				        BUBSYMPH (title)       ---> sprite
				        DARIUSG (ZONE V' BOSS) ---> playfield
				*/

				if (m_f3_game == BUBSYMPH ) sp++;        //BUBSYMPH (title)
				if (m_f3_game == GSEEKER ) sp++;     //GSEEKER (plane leaving hangar)

						if(       sp>l0) sprite[0]|=sflg;
				else if(sp<=l0 && sp>l1) sprite[1]|=sflg;
				else if(sp<=l1 && sp>l2) sprite[2]|=sflg;
				else if(sp<=l2 && sp>l3) sprite[3]|=sflg;
				else if(sp<=l3 && sp>l4) sprite[4]|=sflg;
				else if(sp<=l4         ) sprite[5]|=sflg;
			}
		}


		/* draw scanlines */
		alpha=0;
		for(i=count_skip_layer;i<5;i++)
		{
			pos=layer_tmp[i]&7;
			line_t[i]=&pf_line_inf[pos];

			if(sprite[i]&sprite_alpha_check) alpha=1;
			else if(!alpha) sprite[i]|=0x100;

			if(alpha_mode[pos]>1)
			{
				int alpha_type=(((alpha_mode_flag[pos]>>4)&3)-1)*2;
				m_dpix_lp[i]=m_dpix_n[alpha_mode[pos]+alpha_type];
				alpha=1;
			}
			else
			{
				if(alpha) m_dpix_lp[i]=m_dpix_n[1];
				else      m_dpix_lp[i]=m_dpix_n[0];
			}
		}
		if(sprite[5]&sprite_alpha_check) alpha=1;
		else if(!alpha) sprite[5]|=0x100;

		draw_scanlines(320,draw_line_num,line_t,sprite,rot,count_skip_layer);
		if(y_start<0) break;
	}
}


static void get_spritealphaclip_info()
{
	UINT16 *m_f3_line_ram = (UINT16*)DrvLineRAM;
	struct f3_spritealpha_line_inf *line_t=&m_sa_line_inf[0];

	int y,y_end,y_inc;

	int spri_base,clip_base_low,clip_base_high,inc;

	UINT16 spri=0;
	UINT16 sprite_clip=0;
	UINT16 clip0_low=0, clip0_high=0, clip1_low=0;
	int alpha_level=0;
	UINT16 sprite_alpha=0;

	if (m_flipscreen)
	{
		spri_base=0x77fe;
		clip_base_low=0x51fe;
		clip_base_high=0x45fe;
		inc=-2;
		y=255;
		y_end=-1;
		y_inc=-1;

	}
	else
	{
		spri_base=0x7600;
		clip_base_low=0x5000;
		clip_base_high=0x4400;
		inc=2;
		y=0;
		y_end=256;
		y_inc=1;
	}

	while(y!=y_end)
	{
		/* The zoom, column and row values can latch according to control ram */
		{
			if (m_f3_line_ram[0x100+(y)]&1)
				clip0_low=(m_f3_line_ram[clip_base_low/2]>> 0)&0xffff;
			if (m_f3_line_ram[0x000+(y)]&4)
				clip0_high=(m_f3_line_ram[clip_base_high/2]>> 0)&0xffff;
			if (m_f3_line_ram[0x100+(y)]&2)
				clip1_low=(m_f3_line_ram[(clip_base_low+0x200)/2]>> 0)&0xffff;

			if (m_f3_line_ram[(0x0600/2)+(y)]&0x8)
				spri=m_f3_line_ram[spri_base/2]&0xffff;
			if (m_f3_line_ram[(0x0600/2)+(y)]&0x4)
				sprite_clip=m_f3_line_ram[(spri_base-0x200)/2]&0xffff;
			if (m_f3_line_ram[(0x0400/2)+(y)]&0x1)
				sprite_alpha=m_f3_line_ram[(spri_base-0x1600)/2]&0xffff;
			if (m_f3_line_ram[(0x0400/2)+(y)]&0x2)
				alpha_level=m_f3_line_ram[(spri_base-0x1400)/2]&0xffff;
		}


		line_t->alpha_level[y]=alpha_level;
		line_t->spri[y]=spri;
		line_t->sprite_alpha[y]=sprite_alpha;
		line_t->clip0_l[y]=((clip0_low&0xff)|((clip0_high&0x1000)>>4)) - 47;
		line_t->clip0_r[y]=(((clip0_low&0xff00)>>8)|((clip0_high&0x2000)>>5)) - 47;
		line_t->clip1_l[y]=((clip1_low&0xff)|((clip0_high&0x4000)>>6)) - 47;
		line_t->clip1_r[y]=(((clip1_low&0xff00)>>8)|((clip0_high&0x8000)>>7)) - 47;
		if (line_t->clip0_l[y]<0) line_t->clip0_l[y]=0;
		if (line_t->clip0_r[y]<0) line_t->clip0_r[y]=0;
		if (line_t->clip1_l[y]<0) line_t->clip1_l[y]=0;
		if (line_t->clip1_r[y]<0) line_t->clip1_r[y]=0;

		/* Evaluate sprite clipping */
		if (sprite_clip&0x080)
		{
			line_t->sprite_clip0[y]=0x7fff7fff;
			line_t->sprite_clip1[y]=0;
		}
		else if (sprite_clip&0x33)
		{
			int line_enable=1;
			calculate_clip(y, ((sprite_clip&0x33)<<4), &line_t->sprite_clip0[y], &line_t->sprite_clip1[y], &line_enable);
			if (line_enable==0)
				line_t->sprite_clip0[y]=0x7fff7fff;
		}
		else
		{
			line_t->sprite_clip0[y]=0x7fff0000;
			line_t->sprite_clip1[y]=0;
		}

		spri_base+=inc;
		clip_base_low+=inc;
		clip_base_high+=inc;
		y +=y_inc;
	}
}

void TaitoF3VideoInit()
{
	clear_f3_stuff();
	m_f3_alpha_level_2as=127;
	m_f3_alpha_level_2ad=127;
	m_f3_alpha_level_3as=127;
	m_f3_alpha_level_3ad=127;
	m_f3_alpha_level_2bs=127;
	m_f3_alpha_level_2bd=127;
	m_f3_alpha_level_3bs=127;
	m_f3_alpha_level_3bd=127;
	m_alpha_level_last = -1;

	m_pdest_2a = 0x10;
	m_pdest_2b = 0x20;
	m_tr_2a = 0;
	m_tr_2b = 1;
	m_pdest_3a = 0x40;
	m_pdest_3b = 0x80;
	m_tr_3a = 0;
	m_tr_3b = 1;

	m_width_mask=(extended_layers) ? 0x3ff : 0x1ff;
	m_twidth_mask=(extended_layers) ? 0x7f : 0x3f;
	m_twidth_mask_bit=(extended_layers) ? 7 : 6;

	init_alpha_blend_func();
}

static void DrawCommon(INT32 scanline_start)
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x8000; i+=4) {
			pPaletteUpdateCallback(i);
		}
		DrvRecalc = 0;
	}

	UINT16 *m_f3_control_0 = (UINT16*)DrvCtrlRAM;
	UINT16 *m_f3_control_1 = (UINT16*)(DrvCtrlRAM + 0x10);

	UINT32 sy_fix[5],sx_fix[5];

	/* Setup scroll */
	sy_fix[0]=((m_f3_control_0[4]&0xffff)<< 9) + (1<<16);
	sy_fix[1]=((m_f3_control_0[5]&0xffff)<< 9) + (1<<16);
	sy_fix[2]=((m_f3_control_0[6]&0xffff)<< 9) + (1<<16);
	sy_fix[3]=((m_f3_control_0[7]&0xffff)<< 9) + (1<<16);
	sx_fix[0]=((m_f3_control_0[0]&0xffc0)<<10) - (6<<16);
	sx_fix[1]=((m_f3_control_0[1]&0xffc0)<<10) - (10<<16);
	sx_fix[2]=((m_f3_control_0[2]&0xffc0)<<10) - (14<<16);
	sx_fix[3]=((m_f3_control_0[3]&0xffc0)<<10) - (18<<16);
	sx_fix[4]=-(m_f3_control_1[4])+41;
	sy_fix[4]=-(m_f3_control_1[5]&0x1ff);

	sx_fix[0]-=((m_f3_control_0[0]&0x003f)<<10)+0x0400-0x10000;
	sx_fix[1]-=((m_f3_control_0[1]&0x003f)<<10)+0x0400-0x10000;
	sx_fix[2]-=((m_f3_control_0[2]&0x003f)<<10)+0x0400-0x10000;
	sx_fix[3]-=((m_f3_control_0[3]&0x003f)<<10)+0x0400-0x10000;

	if (m_flipscreen)
	{
		sy_fix[0]= 0x3000000-sy_fix[0];
		sy_fix[1]= 0x3000000-sy_fix[1];
		sy_fix[2]= 0x3000000-sy_fix[2];
		sy_fix[3]= 0x3000000-sy_fix[3];

		sx_fix[0]=-0x1a00000-sx_fix[0];
		sx_fix[1]=-0x1a00000-sx_fix[1];
		sx_fix[2]=-0x1a00000-sx_fix[2];
		sx_fix[3]=-0x1a00000-sx_fix[3];

		sx_fix[4]=-sx_fix[4] + 75;
		sy_fix[4]=-sy_fix[4];
	}

	memset (TaitoPriorityMap, 0, 1024 * 512);
	memset (output_bitmap, 0, 512 * 512 * sizeof(UINT32));

	switch (sprite_lag) {
		case 2: get_sprite_info((UINT16*)TaitoSpriteRamDelayed2); break;
		case 1: get_sprite_info((UINT16*)TaitoSpriteRamDelayed); break;
    		default: get_sprite_info((UINT16*)TaitoSpriteRam); 	break;
	}

	if (nSpriteEnable & 1) draw_sprites();

	get_spritealphaclip_info();

	for (INT32 i = 0; i < (8 >> extended_layers); i++) {
		if (nBurnLayer & 1) draw_pf_layer(i);
	}

	if (nBurnLayer & 2) draw_pixel_layer();
	if (nBurnLayer & 4) draw_vram_layer();

	{
		get_line_ram_info(0,sx_fix[0],sy_fix[0],0,(UINT16*)(DrvPfRAM + (extended_layers ? 0x0000 : 0x0000)));

		get_line_ram_info(1,sx_fix[1],sy_fix[1],1,(UINT16*)(DrvPfRAM + (extended_layers ? 0x2000 : 0x1000)));

		get_line_ram_info(2,sx_fix[2],sy_fix[2],2,(UINT16*)(DrvPfRAM + (extended_layers ? 0x4000 : 0x2000)));

		get_line_ram_info(3,sx_fix[3],sy_fix[3],3,(UINT16*)(DrvPfRAM + (extended_layers ? 0x6000 : 0x3000)));

		get_vram_info(sx_fix[4],sy_fix[4]);

		if (nBurnLayer & 8) scanline_draw();
	}

	// copy video to draw surface
	{
		if (m_flipscreen)
		{
			scanline_start = (scanline_start == 0x1234) ? 1 : 0; // iq_132!!! super-kludge for gunlock. -dink

			UINT32 *src = output_bitmap + ((nScreenHeight + scanline_start - 1) * 512) + 46;
			UINT8 *dst = pBurnDraw;

			for (INT32 y = 0, i = 0; y < nScreenHeight; y++)
			{	
				for (INT32 x = 0; x < nScreenWidth; x++, i++, dst += nBurnBpp)
				{
					PutPix(dst, src[x]);
				}

				src -= 512;
			}
		}
		else
		{
			UINT32 *src = output_bitmap + (scanline_start * 512) + 46;
			UINT8 *dst = pBurnDraw;

			for (INT32 y = 0, i = 0; y < nScreenHeight; y++)
			{	
				for (INT32 x = 0; x < nScreenWidth; x++, i++, dst += nBurnBpp)
				{
					PutPix(dst, src[x]);
				}

				src += 512;
			}
		}
	}
}

static INT32 DrvDraw224A_Flipped() // 224A, w/ m_flipscreen
{
	DrawCommon(0x1234);

	return 0;
}

static INT32 DrvDraw224A()
{
	DrawCommon(31);

	return 0;
}

static INT32 DrvDraw224B()
{
	DrawCommon(32);

	return 0;
}

static INT32 DrvDraw()
{
	DrawCommon(24);

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
		memset (DrvInputs, 0xff, 5 * sizeof(short));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
		}

		DrvInputs[1] &= ~0xff00;
		DrvInputs[4] = (DrvInputs[4] & ~2) | ((DrvSrv[0]) ? 0x00 : 0x02);

		DrvInputs[4] |= 0xf0; // set all coins disabled

		INT32 cur_coin = ((DrvJoy5[4] & 1) << 4) | ((DrvJoy5[5] & 1) << 5) | ((DrvJoy5[6] & 1) << 6) | ((DrvJoy5[7] & 1) << 7);

		for (INT32 i = 0x10; i < 0x100; i <<= 1) {
			if ((cur_coin & i) == i && (previous_coin & i) == 0) {
				DrvInputs[4] &= ~i;
			}
		}

		previous_coin = cur_coin;
	}

	INT32 nInterleave = 256;
	nTaitoCyclesTotal[0] = 16000000 / 60; // do not touch!
	nTaitoCyclesDone[0] = nTaitoCyclesDone[1] = 0;

	SekNewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		nCurrentCPU = 0;
		SekOpen(0);
		nNext = (i + 1) * nTaitoCyclesTotal[nCurrentCPU] / nInterleave;
		nTaitoCyclesSegment = nNext - nTaitoCyclesDone[nCurrentCPU];
		nTaitoCyclesDone[nCurrentCPU] += SekRun(nTaitoCyclesSegment);
		if (i == 255) SekSetIRQLine(2, CPU_IRQSTATUS_AUTO);
		if (i == 7) SekSetIRQLine(3, CPU_IRQSTATUS_AUTO);
		SekClose();

		if (sound_cpu_in_reset == 0)
			TaitoF3CpuUpdate(nInterleave, i);
	}

	TaitoF3SoundUpdate(pBurnSoundOut, nBurnSoundLen);

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	switch (sprite_lag) {
		case 0: break; // no delay
		case 2:	memcpy(TaitoSpriteRamDelayed2, TaitoSpriteRamDelayed, 0x10000); // no break!
		case 1: /* 1 & 2 */ memcpy(TaitoSpriteRamDelayed, TaitoSpriteRam, 0x10000); break;
	}

	return 0;
}


static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin =  0x029702;
	}

	if (nAction & ACB_MEMORY_ROM) {
		ba.Data		= Taito68KRom1;
		ba.nLen		= 0x0200000;
		ba.nAddress	= 0;
		ba.szName	= "Main ROM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_MEMORY_RAM) {	
		ba.Data		= Taito68KRam1;
		ba.nLen		= 0x020000;
		ba.nAddress	= 0x400000;
		ba.szName	= "F3 RAM";
		BurnAcb(&ba);

		ba.Data		= TaitoPaletteRam;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x440000;
		ba.szName	= "Palette RAM";
		BurnAcb(&ba);

		ba.Data		= TaitoSpriteRam;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x600000;
		ba.szName	= "Sprite RAM";
		BurnAcb(&ba);

		ba.Data		= DrvPfRAM;
		ba.nLen		= 0x000c000;
		ba.nAddress	= 0x610000;
		ba.szName	= "Playfield RAM";
		BurnAcb(&ba);

		ba.Data		= TaitoVideoRam;
		ba.nLen		= 0x0002000;
		ba.nAddress	= 0x61c000;
		ba.szName	= "Video RAM";
		BurnAcb(&ba);

		ba.Data		= DrvVRAMRAM;
		ba.nLen		= 0x0002000;
		ba.nAddress	= 0x61e000;
		ba.szName	= "VRAM";
		BurnAcb(&ba);
		
		ba.Data		= DrvLineRAM;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x620000;
		ba.szName	= "Line RAM";
		BurnAcb(&ba);
		
		ba.Data		= DrvPivotRAM;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x620000;
		ba.szName	= "Pivot RAM";
		BurnAcb(&ba);

		ba.Data		= DrvCtrlRAM;
		ba.nLen		= 0x0000400;
		ba.nAddress	= 0x660000;
		ba.szName	= "Control Registers";
		BurnAcb(&ba);

		ba.Data		= TaitoF3SoundRam;
		ba.nLen		= 0x010000;
		ba.nAddress	= 0x000000;
		ba.szName	= "F3 Sound RAM";
		BurnAcb(&ba);

		ba.Data		= TaitoF3SharedRam;
		ba.nLen		= 0x000800;
		ba.nAddress	= 0x000000;
		ba.szName	= "F3 Shared RAM";
		BurnAcb(&ba);

		ba.Data		= TaitoES5510DSPRam;
		ba.nLen		= 0x000200;
		ba.nAddress	= 0x000000;
		ba.szName	= "ES5510 DSP RAM";
		BurnAcb(&ba);

		ba.Data		= TaitoES5510GPR;
		ba.nLen		= 0x0000c0 * sizeof(UINT32);
		ba.nAddress	= 0x000000;
		ba.szName	= "ES5510 GPR RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		TaitoF3SoundScan(nAction, pnMin);

		if (nAction & ACB_WRITE) {
			for (INT32 i = 0; i < 0x2000; i+=4) {
				DrvVRAMExpand(i);
			}

			for (INT32 i = 0; i < 0x10000; i+=4) {
				DrvPivotExpand(i);
			}

			f3_reset_dirtybuffer();
		}
	}

	return 0;
}



// Ring Rage (Ver 2.3O 1992/08/09)

static struct BurnRomInfo ringrageRomDesc[] = {
	{ "d21-23.40",		0x040000, 0x14e9ed65, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d21-22.38",		0x040000, 0x6f8b65b0, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d21-21.36",		0x040000, 0xbf7234bc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d21-25.34",		0x040000, 0xaeff6e19, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d21-02.66",		0x200000, 0xfacd3a02, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d21-03.67",		0x200000, 0x6f653e68, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d21-04.68",		0x200000, 0x9dcdceca, TAITO_SPRITESA },           //  6

	{ "d21-06.49",		0x080000, 0x92d4a720, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d21-07.50",		0x080000, 0x6da696e9, TAITO_CHARS_BYTESWAP },     //  8
	{ "d21-08.51",		0x080000, 0xa0d95be9, TAITO_CHARS } ,             //  9

	{ "d21-18.5",		0x020000, 0x133b55d0, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d21-19.6",		0x020000, 0x1f98908f, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d21-01.17",		0x200000, 0x1fb6f07d, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d21-05.18",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(ringrage)
STD_ROM_FN(ringrage)

static INT32 ringrageInit()
{
	return DrvInit(NULL, f3_12bit_palette_update, 0, RINGRAGE, 2);
}

struct BurnDriver BurnDrvRingrage = {
	"ringrage", NULL, NULL, NULL, "1992",
	"Ring Rage (Ver 2.3O 1992/08/09)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, ringrageRomInfo, ringrageRomName, NULL, NULL, F3InputInfo, NULL,
	ringrageInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Ring Rage (Ver 2.3A 1992/08/09)

static struct BurnRomInfo ringrageuRomDesc[] = {
	{ "d21-23.40",		0x040000, 0x14e9ed65, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d21-22.38",		0x040000, 0x6f8b65b0, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d21-21.36",		0x040000, 0xbf7234bc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d21-24.34",		0x040000, 0x404dee67, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d21-02.66",		0x200000, 0xfacd3a02, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d21-03.67",		0x200000, 0x6f653e68, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d21-04.68",		0x200000, 0x9dcdceca, TAITO_SPRITESA },           //  6

	{ "d21-06.49",		0x080000, 0x92d4a720, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d21-07.50",		0x080000, 0x6da696e9, TAITO_CHARS_BYTESWAP },     //  8
	{ "d21-08.51",		0x080000, 0xa0d95be9, TAITO_CHARS } ,             //  9

	{ "d21-18.5",		0x020000, 0x133b55d0, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d21-19.6",		0x020000, 0x1f98908f, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d21-01.17",		0x200000, 0x1fb6f07d, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d21-05.18",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13
}; 

STD_ROM_PICK(ringrageu)
STD_ROM_FN(ringrageu)

struct BurnDriver BurnDrvRingrageu = {
	"ringrageu", "ringrage", NULL, NULL, "1992",
	"Ring Rage (Ver 2.3A 1992/08/09)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, ringrageuRomInfo, ringrageuRomName, NULL, NULL, F3InputInfo, NULL,
	ringrageInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Ring Rage (Ver 2.3J 1992/08/09)

static struct BurnRomInfo ringragejRomDesc[] = {
	{ "d21-23.40",		0x040000, 0x14e9ed65, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d21-22.38",		0x040000, 0x6f8b65b0, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d21-21.36",		0x040000, 0xbf7234bc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d21-20.34",		0x040000, 0xa8eb68a4, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d21-02.66",		0x200000, 0xfacd3a02, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d21-03.67",		0x200000, 0x6f653e68, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d21-04.68",		0x200000, 0x9dcdceca, TAITO_SPRITESA },           //  6

	{ "d21-06.49",		0x080000, 0x92d4a720, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d21-07.50",		0x080000, 0x6da696e9, TAITO_CHARS_BYTESWAP },     //  8
	{ "d21-08.51",		0x080000, 0xa0d95be9, TAITO_CHARS } ,             //  9

	{ "d21-18.5",		0x020000, 0x133b55d0, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d21-19.6",		0x020000, 0x1f98908f, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d21-01.17",		0x200000, 0x1fb6f07d, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d21-05.18",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(ringragej)
STD_ROM_FN(ringragej)

struct BurnDriver BurnDrvRingragej = {
	"ringragej", "ringrage", NULL, NULL, "1992",
	"Ring Rage (Ver 2.3J 1992/08/09)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, ringragejRomInfo, ringragejRomName, NULL, NULL, F3InputInfo, NULL,
	ringrageInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};



// Arabian Magic (Ver 1.0O 1992/07/06)

static struct BurnRomInfo arabianmRomDesc[] = {
	{ "d29-23.ic40",	0x040000, 0x89a0c706, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d29-22.ic38",	0x040000, 0x4afc22a4, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d29-21.ic36",	0x040000, 0xac32eb38, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d29-25.ic34",	0x040000, 0xb9b652ed, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d29-03.ic66",	0x100000, 0xaeaff456, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d29-04.ic67",	0x100000, 0x01711cfe, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d29-05.ic68",	0x100000, 0x9b5f7a17, TAITO_SPRITESA },           //  6

	{ "d29-06.ic49",	0x080000, 0xeea07bf3, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d29-07.ic50",	0x080000, 0xdb3c094d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d29-08.ic51",	0x080000, 0xd7562851, TAITO_CHARS } ,             //  9

	{ "d29-18.ic5",		0x020000, 0xd97780df, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d29-19.ic6",		0x020000, 0xb1ad365c, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d29-01.ic17",	0x200000, 0x545ac4b3, TAITO_ES5505_BYTESWAP },    //  12 Ensoniq Samples
	{ "d29-02.ic18",	0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    //  13

	{ "D29-11.IC15.bin",0x000157, 0x5dd5c8f9, BRF_OPT }, // 14 plds
	{ "pal20l8b.2",		0x000144, 0xc91437e2, BRF_OPT }, // 15
	{ "D29-13.IC14.bin",0x000157, 0x74d61d36, BRF_OPT }, // 16
	{ "palce16v8h.11",	0x000117, 0x51088324, BRF_OPT }, // 17
	{ "pal16l8b.22",	0x000104, 0x3e01e854, BRF_OPT }, // 18
	{ "palce16v8h.31",	0x000117, 0xe0789727, BRF_OPT }, // 19
	{ "pal16l8b.62",	0x000104, 0x7093e2f3, BRF_OPT }, // 20
	{ "D29-14.IC28.bin",0x000157, 0x25d205d5, BRF_OPT }, // 21
	{ "pal20l8b.70",	0x000144, 0x92b5b97c, BRF_OPT }, // 22
};

STD_ROM_PICK(arabianm)
STD_ROM_FN(arabianm)

static INT32 arabianmInit()
{
	return DrvInit(NULL, f3_12bit_palette_update, 0, ARABIANM, 2);
}

struct BurnDriver BurnDrvArabianm = {
	"arabianm", NULL, NULL, NULL, "1992",
	"Arabian Magic (Ver 1.0O 1992/07/06)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, arabianmRomInfo, arabianmRomName, NULL, NULL, F3InputInfo, NULL,
	arabianmInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Arabian Magic (Ver 1.0J 1992/07/06)

static struct BurnRomInfo arabianmjRomDesc[] = {
	{ "d29-23.ic40",	0x040000, 0x89a0c706, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d29-22.ic38",	0x040000, 0x4afc22a4, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d29-21.ic36",	0x040000, 0xac32eb38, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d29-20.ic34",	0x040000, 0x57b833c1, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d29-03.ic66",	0x100000, 0xaeaff456, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d29-04.ic67",	0x100000, 0x01711cfe, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d29-05.ic68",	0x100000, 0x9b5f7a17, TAITO_SPRITESA },           //  6

	{ "d29-06.ic49",	0x080000, 0xeea07bf3, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d29-07.ic50",	0x080000, 0xdb3c094d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d29-08.ic51",	0x080000, 0xd7562851, TAITO_CHARS } ,             //  9

	{ "d29-18.ic5",		0x020000, 0xd97780df, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d29-19.ic6",		0x020000, 0xb1ad365c, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d29-01.ic17",	0x200000, 0x545ac4b3, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d29-02.ic18",	0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13

	{ "D29-11.IC15.bin",0x000157, 0x5dd5c8f9, BRF_OPT }, // 14 plds
	{ "pal20l8b.2",		0x000144, 0xc91437e2, BRF_OPT }, // 15
	{ "D29-13.IC14.bin",0x000157, 0x74d61d36, BRF_OPT }, // 16
	{ "palce16v8h.11",	0x000117, 0x51088324, BRF_OPT }, // 17
	{ "pal16l8b.22",	0x000104, 0x3e01e854, BRF_OPT }, // 18
	{ "palce16v8h.31",	0x000117, 0xe0789727, BRF_OPT }, // 19
	{ "pal16l8b.62",	0x000104, 0x7093e2f3, BRF_OPT }, // 20
	{ "D29-14.IC28.bin",0x000157, 0x25d205d5, BRF_OPT }, // 21
	{ "pal20l8b.70",	0x000144, 0x92b5b97c, BRF_OPT }, // 22
};

STD_ROM_PICK(arabianmj)
STD_ROM_FN(arabianmj)

struct BurnDriver BurnDrvArabianmj = {
	"arabianmj", "arabianm", NULL, NULL, "1992",
	"Arabian Magic (Ver 1.0J 1992/07/06)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, arabianmjRomInfo, arabianmjRomName, NULL, NULL, F3InputInfo, NULL,
	arabianmInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Arabian Magic (Ver 1.0A 1992/07/06)

static struct BurnRomInfo arabianmuRomDesc[] = {
	{ "d29-23.ic40",	0x040000, 0x89a0c706, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d29-22.ic38",	0x040000, 0x4afc22a4, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d29-21.ic36",	0x040000, 0xac32eb38, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d29-24.ic34",	0x040000, 0xceb1627b, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d29-03.ic66",	0x100000, 0xaeaff456, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d29-04.ic67",	0x100000, 0x01711cfe, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d29-05.ic68",	0x100000, 0x9b5f7a17, TAITO_SPRITESA },           //  6

	{ "d29-06.ic49",	0x080000, 0xeea07bf3, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d29-07.ic50",	0x080000, 0xdb3c094d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d29-08.ic51",	0x080000, 0xd7562851, TAITO_CHARS } ,             //  9

	{ "d29-18.ic5",		0x020000, 0xd97780df, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d29-19.ic6",		0x020000, 0xb1ad365c, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d29-01.ic17",	0x200000, 0x545ac4b3, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d29-02.ic18",	0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13

	{ "D29-11.IC15.bin",0x000157, 0x5dd5c8f9, BRF_OPT }, // 14 plds
	{ "pal20l8b.2",		0x000144, 0xc91437e2, BRF_OPT }, // 15
	{ "D29-13.IC14.bin",0x000157, 0x74d61d36, BRF_OPT }, // 16
	{ "palce16v8h.11",	0x000117, 0x51088324, BRF_OPT }, // 17
	{ "pal16l8b.22",	0x000104, 0x3e01e854, BRF_OPT }, // 18
	{ "palce16v8h.31",	0x000117, 0xe0789727, BRF_OPT }, // 19
	{ "pal16l8b.62",	0x000104, 0x7093e2f3, BRF_OPT }, // 20
	{ "D29-14.IC28.bin",0x000157, 0x25d205d5, BRF_OPT }, // 21
	{ "pal20l8b.70",	0x000144, 0x92b5b97c, BRF_OPT }, // 22
};

STD_ROM_PICK(arabianmu)
STD_ROM_FN(arabianmu)

struct BurnDriver BurnDrvArabianmu = {
	"arabianmu", "arabianm", NULL, NULL, "1992",
	"Arabian Magic (Ver 1.0A 1992/07/06)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, arabianmuRomInfo, arabianmuRomName, NULL, NULL, F3InputInfo, NULL,
	arabianmInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Riding Fight (Ver 1.0O)

static struct BurnRomInfo ridingfRomDesc[] = {
	{ "d34-12.40",		0x040000, 0xe67e69d4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d34-11.38",		0x040000, 0x7d240a88, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d34-10.36",		0x040000, 0x8aa3f4ac, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d34_14.34",		0x040000, 0xe000198e, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d34-01.66",		0x200000, 0x7974e6aa, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d34-02.67",		0x200000, 0xf4422370, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "d34-05.49",		0x080000, 0x72e3ee4b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "d34-06.50",		0x080000, 0xedc9b9f3, TAITO_CHARS_BYTESWAP },     //  7

	{ "d34-07.5",		0x020000, 0x67239e2b, TAITO_68KROM2_BYTESWAP }, //  8 68k Code
	{ "d34-08.6",		0x020000, 0x2cf20323, TAITO_68KROM2_BYTESWAP }, //  9

	{ "d34-03.17",		0x200000, 0xe534ef74, TAITO_ES5505_BYTESWAP },  // 10 Ensoniq Samples
	{ "d34-04.18",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },  // 11
};

STD_ROM_PICK(ridingf)
STD_ROM_FN(ridingf)

static INT32 ridingfInit()
{
	return DrvInit(NULL, f3_12bit_palette_update, 1, RIDINGF, 1);
}

struct BurnDriver BurnDrvRidingf = {
	"ridingf", NULL, NULL, NULL, "1992",
	"Riding Fight (Ver 1.0O)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, ridingfRomInfo, ridingfRomName, NULL, NULL, F3InputInfo, NULL,
	ridingfInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Riding Fight (Ver 1.0J)

static struct BurnRomInfo ridingfjRomDesc[] = {
	{ "d34-12.40",		0x040000, 0xe67e69d4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d34-11.38",		0x040000, 0x7d240a88, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d34-10.36",		0x040000, 0x8aa3f4ac, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d34-09.34",		0x040000, 0x0e0e78a2, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d34-01.66",		0x200000, 0x7974e6aa, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d34-02.67",		0x200000, 0xf4422370, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "d34-05.49",		0x080000, 0x72e3ee4b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "d34-06.50",		0x080000, 0xedc9b9f3, TAITO_CHARS_BYTESWAP },     //  7

	{ "d34-07.5",		0x020000, 0x67239e2b, TAITO_68KROM2_BYTESWAP }, //  8 68k Code
	{ "d34-08.6",		0x020000, 0x2cf20323, TAITO_68KROM2_BYTESWAP }, //  9

	{ "d34-03.17",		0x200000, 0xe534ef74, TAITO_ES5505_BYTESWAP },  // 10 Ensoniq Samples
	{ "d34-04.18",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },  // 11
};

STD_ROM_PICK(ridingfj)
STD_ROM_FN(ridingfj)

struct BurnDriver BurnDrvRidingfj = {
	"ridingfj", "ridingf", NULL, NULL, "1992",
	"Riding Fight (Ver 1.0J)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, ridingfjRomInfo, ridingfjRomName, NULL, NULL, F3InputInfo, NULL,
	ridingfInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Riding Fight (Ver 1.0A)

static struct BurnRomInfo ridingfuRomDesc[] = {
	{ "d34-12.40",		0x040000, 0xe67e69d4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d34-11.38",		0x040000, 0x7d240a88, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d34-10.36",		0x040000, 0x8aa3f4ac, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d34_13.34",		0x040000, 0x97072918, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d34-01.66",		0x200000, 0x7974e6aa, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d34-02.67",		0x200000, 0xf4422370, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "d34-05.49",		0x080000, 0x72e3ee4b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "d34-06.50",		0x080000, 0xedc9b9f3, TAITO_CHARS_BYTESWAP },     //  7

	{ "d34-07.5",		0x020000, 0x67239e2b, TAITO_68KROM2_BYTESWAP }, //  8 68k Code
	{ "d34-08.6",		0x020000, 0x2cf20323, TAITO_68KROM2_BYTESWAP }, //  9

	{ "d34-03.17",		0x200000, 0xe534ef74, TAITO_ES5505_BYTESWAP },  // 10 Ensoniq Samples
	{ "d34-04.18",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },  // 11
};

STD_ROM_PICK(ridingfu)
STD_ROM_FN(ridingfu)

struct BurnDriver BurnDrvRidingfu = {
	"ridingfu", "ridingf", NULL, NULL, "1992",
	"Riding Fight (Ver 1.0A)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, ridingfuRomInfo, ridingfuRomName, NULL, NULL, F3InputInfo, NULL,
	ridingfInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};



// Grid Seeker: Project Storm Hammer (Ver 1.3O)

static struct BurnRomInfo gseekerRomDesc[] = {
	{ "d40_12.rom",		0x040000, 0x884055fb, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d40_11.rom",		0x040000, 0x85e701d2, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d40_10.rom",		0x040000, 0x1e659ac5, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d40_14.rom",		0x040000, 0xd9a76bd9, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d40_03.rom",		0x100000, 0xbcd70efc, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d40_04.rom",		0x100000, 0xcd2ac666, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d40_15.rom",		0x080000, 0x50555125, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d40_16.rom",		0x080000, 0x3f9bbe1e, TAITO_SPRITESA_BYTESWAP },  //  7

	{ "d40_05.rom",		0x100000, 0xbe6eec8f, TAITO_CHARS_BYTESWAP },     //  8 Layer Tiles
	{ "d40_06.rom",		0x100000, 0xa822abe4, TAITO_CHARS_BYTESWAP },     //  9

	{ "d40_07.rom",		0x020000, 0x7e9b26c2, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d40_08.rom",		0x020000, 0x9c926a28, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d40_01.rom",		0x200000, 0xee312e95, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d40_02.rom",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(gseeker)
STD_ROM_FN(gseeker)

static INT32 gseekerInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, GSEEKER, 1);
}

struct BurnDriver BurnDrvGseeker = {
	"gseeker", NULL, NULL, NULL, "1992",
	"Grid Seeker: Project Storm Hammer (Ver 1.3O)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gseekerRomInfo, gseekerRomName, NULL, NULL, F3InputInfo, NULL,
	gseekerInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Grid Seeker: Project Storm Hammer (Ver 1.3J)

static struct BurnRomInfo gseekerjRomDesc[] = {
	{ "d40_12.rom",		0x040000, 0x884055fb, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d40_11.rom",		0x040000, 0x85e701d2, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d40_10.rom",		0x040000, 0x1e659ac5, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d40-09.34",		0x040000, 0x37a90af5, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d40_03.rom",		0x100000, 0xbcd70efc, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d40_04.rom",		0x100000, 0xcd2ac666, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d40_15.rom",		0x080000, 0x50555125, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d40_16.rom",		0x080000, 0x3f9bbe1e, TAITO_SPRITESA_BYTESWAP },  //  7

	{ "d40_05.rom",		0x100000, 0xbe6eec8f, TAITO_CHARS_BYTESWAP },     //  8 Layer Tiles
	{ "d40_06.rom",		0x100000, 0xa822abe4, TAITO_CHARS_BYTESWAP },     //  9

	{ "d40_07.rom",		0x020000, 0x7e9b26c2, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d40_08.rom",		0x020000, 0x9c926a28, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d40_01.rom",		0x200000, 0xee312e95, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d40_02.rom",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(gseekerj)
STD_ROM_FN(gseekerj)

struct BurnDriver BurnDrvGseekerj = {
	"gseekerj", "gseeker", NULL, NULL, "1992",
	"Grid Seeker: Project Storm Hammer (Ver 1.3J)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gseekerjRomInfo, gseekerjRomName, NULL, NULL, F3InputInfo, NULL,
	gseekerInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Grid Seeker: Project Storm Hammer (Ver 1.3A)

static struct BurnRomInfo gseekeruRomDesc[] = {
	{ "d40_12.rom",		0x040000, 0x884055fb, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d40_11.rom",		0x040000, 0x85e701d2, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d40_10.rom",		0x040000, 0x1e659ac5, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d40-13.bin",		0x040000, 0xaea05b4f, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d40_03.rom",		0x100000, 0xbcd70efc, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d40_04.rom",		0x100000, 0xcd2ac666, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d40_15.rom",		0x080000, 0x50555125, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d40_16.rom",		0x080000, 0x3f9bbe1e, TAITO_SPRITESA_BYTESWAP },  //  7

	{ "d40_05.rom",		0x100000, 0xbe6eec8f, TAITO_CHARS_BYTESWAP },     //  8 Layer Tiles
	{ "d40_06.rom",		0x100000, 0xa822abe4, TAITO_CHARS_BYTESWAP },     //  9

	{ "d40_07.rom",		0x020000, 0x7e9b26c2, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d40_08.rom",		0x020000, 0x9c926a28, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d40_01.rom",		0x200000, 0xee312e95, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d40_02.rom",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(gseekeru)
STD_ROM_FN(gseekeru)

struct BurnDriver BurnDrvGseekeru = {
	"gseekeru", "gseeker", NULL, NULL, "1992",
	"Grid Seeker: Project Storm Hammer (Ver 1.3A)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gseekeruRomInfo, gseekeruRomName, NULL, NULL, F3InputInfo, NULL,
	gseekerInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Command War - Super Special Battle & War Game (Ver 0.0J) (Prototype)

static struct BurnRomInfo commandwRomDesc[] = {
	{ "cw_mpr3.bin",	0x040000, 0x636944fc, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "cw_mpr2.bin",	0x040000, 0x1151a42b, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "cw_mpr1.bin",	0x040000, 0x93669389, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "cw_mpr0.bin",	0x040000, 0x0468df52, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "cw_objl0.bin",	0x200000, 0x9822102e, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "cw_objm0.bin",	0x200000, 0xf7687684, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "cw_objl1.bin",	0x200000, 0xca3ad7f6, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "cw_objm1.bin",	0x200000, 0x504b1bf5, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "cw_objh0.bin",	0x200000, 0x83d7e0ae, TAITO_SPRITESA },           //  8
	{ "cw_objh1.bin",	0x200000, 0x324f5832, TAITO_SPRITESA },           //  9

	{ "cw_scr_l.bin",	0x100000, 0x4d202323, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "cw_scr_m.bin",	0x100000, 0x537b1c7d, TAITO_CHARS_BYTESWAP },     // 11
	{ "cw_scr_h.bin",	0x100000, 0x001f85dd, TAITO_CHARS },              // 12

	{ "cw_spr1.bin",	0x020000, 0xc8f81c25, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "cw_spr0.bin",	0x020000, 0x2aaa9dfb, TAITO_68KROM2_BYTESWAP },   // 14

	{ "cw_pcm_0.bin",	0x200000, 0xa1e26629, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "cw_pcm_1.bin",	0x200000, 0x39fc6cf4, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(commandw)
STD_ROM_FN(commandw)

static INT32 commandwInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, COMMANDW, 1);
}

struct BurnDriver BurnDrvCommandw = {
	"commandw", NULL, NULL, NULL, "1992",
	"Command War - Super Special Battle & War Game (Ver 0.0J) (Prototype)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, commandwRomInfo, commandwRomName, NULL, NULL, F3InputInfo, NULL,
	commandwInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Taito Cup Finals (Ver 1.0O 1993/02/28)

static struct BurnRomInfo cupfinalRomDesc[] = {
	{ "d49-13.20",		0x020000, 0xccee5e73, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d49-14.19",		0x020000, 0x2323bf2e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d49-16.18",		0x020000, 0x8e73f739, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d49-20.17",		0x020000, 0x1e9c392c, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d49-01.12",		0x200000, 0x1dc89f1c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d49-02.8",		0x200000, 0x1e4c374f, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d49-06.11",		0x100000, 0x71ef4ee1, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d49-07.7",		0x100000, 0xe5655b8f, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d49-03.4",		0x200000, 0xcf9a8727, TAITO_SPRITESA },           //  8
	{ "d49-08.3",		0x100000, 0x7d3c6536, TAITO_SPRITESA },           //  9

	{ "d49-09.47",		0x080000, 0x257ede01, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d49-10.45",		0x080000, 0xf587b787, TAITO_CHARS_BYTESWAP },     // 11
	{ "d49-11.43",		0x080000, 0x11318b26, TAITO_CHARS },              // 12

	{ "d49-17.32",		0x020000, 0xf2058eba, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d49-18.33",		0x020000, 0xa0fdd270, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d49-04.38",		0x200000, 0x44b365a9, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d49-05.41",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(cupfinal)
STD_ROM_FN(cupfinal)

static INT32 cupfinalInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, SCFINALS, 1);
}

struct BurnDriver BurnDrvCupfinal = {
	"cupfinal", NULL, NULL, NULL, "1993",
	"Taito Cup Finals (Ver 1.0O 1993/02/28)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, cupfinalRomInfo, cupfinalRomName, NULL, NULL, F3InputInfo, NULL,
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Hat Trick Hero '93 (Ver 1.0J 1993/02/28)

static struct BurnRomInfo hthero93RomDesc[] = {
	{ "d49-13.20",		0x020000, 0xccee5e73, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d49-14.19",		0x020000, 0x2323bf2e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d49-16.18",		0x020000, 0x8e73f739, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d49-19.17",		0x020000, 0xf0925800, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d49-01.12",		0x200000, 0x1dc89f1c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d49-02.8",		0x200000, 0x1e4c374f, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d49-06.11",		0x100000, 0x71ef4ee1, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d49-07.7",		0x100000, 0xe5655b8f, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d49-03.4",		0x200000, 0xcf9a8727, TAITO_SPRITESA },           //  8
	{ "d49-08.3",		0x100000, 0x7d3c6536, TAITO_SPRITESA },           //  9

	{ "d49-09.47",		0x080000, 0x257ede01, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d49-10.45",		0x080000, 0xf587b787, TAITO_CHARS_BYTESWAP },     // 11
	{ "d49-11.43",		0x080000, 0x11318b26, TAITO_CHARS },              // 12

	{ "d49-17.32",		0x020000, 0xf2058eba, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d49-18.33",		0x020000, 0xa0fdd270, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d49-04.38",		0x200000, 0x44b365a9, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d49-05.41",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(hthero93)
STD_ROM_FN(hthero93)

struct BurnDriver BurnDrvHthero93 = {
	"hthero93", "cupfinal", NULL, NULL, "1993",
	"Hat Trick Hero '93 (Ver 1.0J 1993/02/28)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, hthero93RomInfo, hthero93RomName, NULL, NULL, F3InputInfo, NULL,
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Hat Trick Hero '93 (Ver 1.0A 1993/02/28)

static struct BurnRomInfo hthero93uRomDesc[] = {
	{ "d49-13.24",		0x020000, 0xccee5e73, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d49-14.26",		0x020000, 0x2323bf2e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d49-16.37",		0x020000, 0x8e73f739, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d49-19.35",		0x020000, 0x699b09ba, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d49-01.12",		0x200000, 0x1dc89f1c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d49-02.8",		0x200000, 0x1e4c374f, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d49-06.11",		0x100000, 0x71ef4ee1, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d49-07.7",		0x100000, 0xe5655b8f, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d49-03.4",		0x200000, 0xcf9a8727, TAITO_SPRITESA },           //  8
	{ "d49-08.3",		0x100000, 0x7d3c6536, TAITO_SPRITESA },           //  9

	{ "d49-09.47",		0x080000, 0x257ede01, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d49-10.45",		0x080000, 0xf587b787, TAITO_CHARS_BYTESWAP },     // 11
	{ "d49-11.43",		0x080000, 0x11318b26, TAITO_CHARS },              // 12

	{ "d49-17.32",		0x020000, 0xf2058eba, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d49-18.33",		0x020000, 0xa0fdd270, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d49-04.38",		0x200000, 0x44b365a9, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d49-05.41",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 16

	{ "D49-12.IC60.bin",	0x000104, 0xaa4cff37, 0 | BRF_OPT },              // 17 palsgame
	{ "D49-21.IC17.bin",	0x000104, 0x821775d4, 0 | BRF_OPT },              // 18

	{ "D29-11.IC15.bin",	0x000157, 0x5dd5c8f9, 0 | BRF_OPT },              // 19 palsbase
	{ "D29-12.IC12.bin",	0x000144, 0xc872f1fd, 0 | BRF_OPT },              // 20
	{ "D29-13.IC14.bin",	0x000157, 0x74d61d36, 0 | BRF_OPT },              // 21
	{ "D29-14.IC28.bin",	0x000157, 0x25d205d5, 0 | BRF_OPT },              // 22
	{ "D29-15.IC29.bin",	0x000157, 0x692eb582, 0 | BRF_OPT },              // 23
	{ "D29-16.IC7.bin",	0x000117, 0x11875f52, 0 | BRF_OPT },              // 24
	{ "D29-17.IC16.bin",	0x000117, 0xa0f74b51, 0 | BRF_OPT },              // 25
};

STD_ROM_PICK(hthero93u)
STD_ROM_FN(hthero93u)

struct BurnDriver BurnDrvHthero93u = {
	"hthero93u", "cupfinal", NULL, NULL, "1993",
	"Hat Trick Hero '93 (Ver 1.0A 1993/02/28)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, hthero93uRomInfo, hthero93uRomName, NULL, NULL, F3InputInfo, NULL,
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Top Ranking Stars (Ver 2.1O 1993/05/21) (New Version)

static struct BurnRomInfo trstarRomDesc[] = {
	{ "d53-15-1.24",	0x040000, 0x098bba94, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d53-16-1.26",	0x040000, 0x4fa8b15c, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d53-18-1.37",	0x040000, 0xaa71cfcc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d53-20-1.35",	0x040000, 0x4de1e287, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d53-03.45",		0x200000, 0x91b66145, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d53-04.46",		0x200000, 0xac3a5e80, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d53-06.64",		0x100000, 0xf4bac410, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d53-07.65",		0x100000, 0x2f4773c3, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d53-05.47",		0x200000, 0xb9b68b15, TAITO_SPRITESA },           //  8
	{ "d53-08.66",		0x100000, 0xad13a1ee, TAITO_SPRITESA },           //  9

	{ "d53-09.48",		0x100000, 0x690554d3, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d53-10.49",		0x100000, 0x0ec05dc5, TAITO_CHARS_BYTESWAP },     // 11
	{ "d53-11.50",		0x100000, 0x39c0a546, TAITO_CHARS },              // 12

	{ "d53-13.10",		0x020000, 0x877f0361, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d53-14.23",		0x020000, 0xa8664867, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d53-01.2",		0x200000, 0x28fd2d9b, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d53-02.3",		0x200000, 0x8bd4367a, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(trstar)
STD_ROM_FN(trstar)

static INT32 trstarInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, TRSTAR, 0);
}

struct BurnDriver BurnDrvTrstar = {
	"trstar", NULL, NULL, NULL, "1993",
	"Top Ranking Stars (Ver 2.1O 1993/05/21) (New Version)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, trstarRomInfo, trstarRomName, NULL, NULL, F3InputInfo, NULL,
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Top Ranking Stars (Ver 2.1J 1993/05/21) (New Version)

static struct BurnRomInfo trstarjRomDesc[] = {
	{ "d53-15-1.24",	0x040000, 0x098bba94, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d53-16-1.26",	0x040000, 0x4fa8b15c, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d53-18-1.37",	0x040000, 0xaa71cfcc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d53-17-1.35",	0x040000, 0xa3ef83ab, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d53-03.45",		0x200000, 0x91b66145, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d53-04.46",		0x200000, 0xac3a5e80, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d53-06.64",		0x100000, 0xf4bac410, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d53-07.65",		0x100000, 0x2f4773c3, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d53-05.47",		0x200000, 0xb9b68b15, TAITO_SPRITESA },           //  8
	{ "d53-08.66",		0x100000, 0xad13a1ee, TAITO_SPRITESA },           //  9

	{ "d53-09.48",		0x100000, 0x690554d3, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d53-10.49",		0x100000, 0x0ec05dc5, TAITO_CHARS_BYTESWAP },     // 11
	{ "d53-11.50",		0x100000, 0x39c0a546, TAITO_CHARS },              // 12

	{ "d53-13.10",		0x020000, 0x877f0361, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d53-14.23",		0x020000, 0xa8664867, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d53-01.2",		0x200000, 0x28fd2d9b, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d53-02.3",		0x200000, 0x8bd4367a, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(trstarj)
STD_ROM_FN(trstarj)

struct BurnDriver BurnDrvTrstarj = {
	"trstarj", "trstar", NULL, NULL, "1993",
	"Top Ranking Stars (Ver 2.1J 1993/05/21) (New Version)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, trstarjRomInfo, trstarjRomName, NULL, NULL, F3InputInfo, NULL,
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Prime Time Fighter (Ver 2.1A 1993/05/21) (New Version)

static struct BurnRomInfo prmtmfgtRomDesc[] = {
	{ "d53-15-1.24",	0x040000, 0x098bba94, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d53-16-1.26",	0x040000, 0x4fa8b15c, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d53-18-1.37",	0x040000, 0xaa71cfcc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d53-19-1.35",	0x040000, 0x3ae6d211, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d53-03.45",		0x200000, 0x91b66145, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d53-04.46",		0x200000, 0xac3a5e80, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d53-06.64",		0x100000, 0xf4bac410, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d53-07.65",		0x100000, 0x2f4773c3, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d53-05.47",		0x200000, 0xb9b68b15, TAITO_SPRITESA },           //  8
	{ "d53-08.66",		0x100000, 0xad13a1ee, TAITO_SPRITESA },           //  9

	{ "d53-09.48",		0x100000, 0x690554d3, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d53-10.49",		0x100000, 0x0ec05dc5, TAITO_CHARS_BYTESWAP },     // 11
	{ "d53-11.50",		0x100000, 0x39c0a546, TAITO_CHARS },              // 12

	{ "d53-13.10",		0x020000, 0x877f0361, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d53-14.23",		0x020000, 0xa8664867, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d53-01.2",		0x200000, 0x28fd2d9b, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d53-02.3",		0x200000, 0x8bd4367a, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(prmtmfgt)
STD_ROM_FN(prmtmfgt)

struct BurnDriver BurnDrvPrmtmfgt = {
	"prmtmfgt", "trstar", NULL, NULL, "1993",
	"Prime Time Fighter (Ver 2.1A 1993/05/21) (New Version)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, prmtmfgtRomInfo, prmtmfgtRomName, NULL, NULL, F3InputInfo, NULL,
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Top Ranking Stars (Ver 2.1O 1993/05/21) (Old Version)

static struct BurnRomInfo trstaroRomDesc[] = {
	{ "d53-15.24",		0x040000, 0xf24de51b, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d53-16.26",		0x040000, 0xffc84429, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d53-18.37",		0x040000, 0xea2d6e13, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d53-20.35",		0x040000, 0x77e1f267, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d53-03.45",		0x200000, 0x91b66145, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d53-04.46",		0x200000, 0xac3a5e80, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d53-06.64",		0x100000, 0xf4bac410, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d53-07.65",		0x100000, 0x2f4773c3, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d53-05.47",		0x200000, 0xb9b68b15, TAITO_SPRITESA },           //  8
	{ "d53-08.66",		0x100000, 0xad13a1ee, TAITO_SPRITESA },           //  9

	{ "d53-09.48",		0x100000, 0x690554d3, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d53-10.49",		0x100000, 0x0ec05dc5, TAITO_CHARS_BYTESWAP },     // 11
	{ "d53-11.50",		0x100000, 0x39c0a546, TAITO_CHARS },              // 12

	{ "d53-13.10",		0x020000, 0x877f0361, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d53-14.23",		0x020000, 0xa8664867, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d53-01.2",		0x200000, 0x28fd2d9b, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d53-02.3",		0x200000, 0x8bd4367a, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(trstaro)
STD_ROM_FN(trstaro)

struct BurnDriver BurnDrvTrstaro = {
	"trstaro", "trstar", NULL, NULL, "1993",
	"Top Ranking Stars (Ver 2.1O 1993/05/21) (Old Version)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, trstaroRomInfo, trstaroRomName, NULL, NULL, F3InputInfo, NULL,
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Top Ranking Stars (Ver 2.1J 1993/05/21) (Old Version)

static struct BurnRomInfo trstarojRomDesc[] = {
	{ "d53-15.24",		0x040000, 0xf24de51b, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d53-16.26",		0x040000, 0xffc84429, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d53-18.37",		0x040000, 0xea2d6e13, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d53-17.35",		0x040000, 0x99ef934b, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d53-03.45",		0x200000, 0x91b66145, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d53-04.46",		0x200000, 0xac3a5e80, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d53-06.64",		0x100000, 0xf4bac410, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d53-07.65",		0x100000, 0x2f4773c3, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d53-05.47",		0x200000, 0xb9b68b15, TAITO_SPRITESA },           //  8
	{ "d53-08.66",		0x100000, 0xad13a1ee, TAITO_SPRITESA },           //  9

	{ "d53-09.48",		0x100000, 0x690554d3, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d53-10.49",		0x100000, 0x0ec05dc5, TAITO_CHARS_BYTESWAP },     // 11
	{ "d53-11.50",		0x100000, 0x39c0a546, TAITO_CHARS },              // 12

	{ "d53-13.10",		0x020000, 0x877f0361, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d53-14.23",		0x020000, 0xa8664867, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d53-01.2",		0x200000, 0x28fd2d9b, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d53-02.3",		0x200000, 0x8bd4367a, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(trstaroj)
STD_ROM_FN(trstaroj)

struct BurnDriver BurnDrvTrstaroj = {
	"trstaroj", "trstar", NULL, NULL, "1993",
	"Top Ranking Stars (Ver 2.1J 1993/05/21) (Old Version)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, trstarojRomInfo, trstarojRomName, NULL, NULL, F3InputInfo, NULL,
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Prime Time Fighter (Ver 2.1A 1993/05/21) (Old Version)

static struct BurnRomInfo prmtmfgtoRomDesc[] = {
	{ "d53-15.24",		0x040000, 0xf24de51b, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d53-16.26",		0x040000, 0xffc84429, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d53-18.37",		0x040000, 0xea2d6e13, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d53-19.35",		0x040000, 0x00e6c2f1, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d53-03.45",		0x200000, 0x91b66145, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d53-04.46",		0x200000, 0xac3a5e80, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d53-06.64",		0x100000, 0xf4bac410, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d53-07.65",		0x100000, 0x2f4773c3, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d53-05.47",		0x200000, 0xb9b68b15, TAITO_SPRITESA },           //  8
	{ "d53-08.66",		0x100000, 0xad13a1ee, TAITO_SPRITESA },           //  9

	{ "d53-09.48",		0x100000, 0x690554d3, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d53-10.49",		0x100000, 0x0ec05dc5, TAITO_CHARS_BYTESWAP },     // 11
	{ "d53-11.50",		0x100000, 0x39c0a546, TAITO_CHARS },              // 12

	{ "d53-13.10",		0x020000, 0x877f0361, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d53-14.23",		0x020000, 0xa8664867, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d53-01.2",		0x200000, 0x28fd2d9b, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d53-02.3",		0x200000, 0x8bd4367a, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(prmtmfgto)
STD_ROM_FN(prmtmfgto)

struct BurnDriver BurnDrvPrmtmfgto = {
	"prmtmfgto", "trstar", NULL, NULL, "1993",
	"Prime Time Fighter (Ver 2.1A 1993/05/21) (Old Version)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, prmtmfgtoRomInfo, prmtmfgtoRomName, NULL, NULL, F3InputInfo, NULL,
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Gunlock (Ver 2.3O 1994/01/20)

static struct BurnRomInfo gunlockRomDesc[] = {
	{ "d66-18.ic24",	0x040000, 0x8418513e, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d66-19.ic26",	0x040000, 0x95731473, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d66-21.ic37",	0x040000, 0xbd0d60f2, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d66-24.ic35",	0x040000, 0x97816378, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d66-03.ic45",	0x100000, 0xe7a4a491, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d66-04.ic46",	0x100000, 0xc1c7aaa7, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d66-05.ic47",	0x100000, 0xa3cefe04, TAITO_SPRITESA },           //  6

	{ "d66-06.ic48",	0x100000, 0xb3d8126d, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d66-07.ic49",	0x100000, 0xa6da9be7, TAITO_CHARS_BYTESWAP },     //  8
	{ "d66-08.ic49",	0x100000, 0x9959f30b, TAITO_CHARS },              //  9

	{ "d66-23.ic10",	0x040000, 0x57fb7c49, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d66-22.ic23",	0x040000, 0x83dd7f9b, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d66-01.ic2",		0x200000, 0x58c92efa, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d66-02.ic3",		0x200000, 0xdcdafaab, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(gunlock)
STD_ROM_FN(gunlock)

static INT32 gunlockInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, GUNLOCK, 2);
}

struct BurnDriver BurnDrvGunlock = {
	"gunlock", NULL, NULL, NULL, "1993",
	"Gunlock (Ver 2.3O 1994/01/20)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gunlockRomInfo, gunlockRomName, NULL, NULL, F3InputInfo, NULL,
	gunlockInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Ray Force (Ver 2.3A 1994/01/20)

static struct BurnRomInfo rayforceRomDesc[] = {
	{ "d66-18.ic24",	0x040000, 0x8418513e, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d66-19.ic26",	0x040000, 0x95731473, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d66-21.ic37",	0x040000, 0xbd0d60f2, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d66-25.ic35",	0x040000, 0xe08653ee, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d66-03.ic45",	0x100000, 0xe7a4a491, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d66-04.ic46",	0x100000, 0xc1c7aaa7, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d66-05.ic47",	0x100000, 0xa3cefe04, TAITO_SPRITESA },           //  6

	{ "d66-06.ic48",	0x100000, 0xb3d8126d, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d66-07.ic49",	0x100000, 0xa6da9be7, TAITO_CHARS_BYTESWAP },     //  8
	{ "d66-08.ic49",	0x100000, 0x9959f30b, TAITO_CHARS },              //  9

	{ "d66-23.ic10",	0x040000, 0x57fb7c49, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d66-22.ic23",	0x040000, 0x83dd7f9b, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d66-01.ic2",		0x200000, 0x58c92efa, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d66-02.ic3",		0x200000, 0xdcdafaab, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(rayforce)
STD_ROM_FN(rayforce)

struct BurnDriver BurnDrvRayforce = {
	"rayforce", "gunlock", NULL, NULL, "1993",
	"Ray Force (Ver 2.3A 1994/01/20)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, rayforceRomInfo, rayforceRomName, NULL, NULL, F3InputInfo, NULL,
	gunlockInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Ray Force (Ver 2.3J 1994/01/20)

static struct BurnRomInfo rayforcejRomDesc[] = {
	{ "d66-18.ic24",	0x040000, 0x8418513e, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d66-19.ic26",	0x040000, 0x95731473, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d66-21.ic37",	0x040000, 0xbd0d60f2, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d66-20.ic35",	0x040000, 0x798f0254, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d66-03.ic45",	0x100000, 0xe7a4a491, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d66-04.ic46",	0x100000, 0xc1c7aaa7, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d66-05.ic47",	0x100000, 0xa3cefe04, TAITO_SPRITESA },           //  6

	{ "d66-06.ic48",	0x100000, 0xb3d8126d, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d66-07.ic49",	0x100000, 0xa6da9be7, TAITO_CHARS_BYTESWAP },     //  8
	{ "d66-08.ic49",	0x100000, 0x9959f30b, TAITO_CHARS },              //  9

	{ "d66-23.ic10",	0x040000, 0x57fb7c49, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d66-22.ic23",	0x040000, 0x83dd7f9b, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d66-01.ic2",		0x200000, 0x58c92efa, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d66-02.ic3",		0x200000, 0xdcdafaab, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(rayforcej)
STD_ROM_FN(rayforcej)

struct BurnDriver BurnDrvRayforcej = {
	"rayforcej", "gunlock", NULL, NULL, "1993",
	"Ray Force (Ver 2.3J 1994/01/20)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, rayforcejRomInfo, rayforcejRomName, NULL, NULL, F3InputInfo, NULL,
	gunlockInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Super Cup Finals (Ver 2.2O 1994/01/13)

static struct BurnRomInfo scfinalsRomDesc[] = {
	{ "d68-09.ic40",	0x040000, 0x28193b3f, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d68-10.ic38",	0x040000, 0x67481bad, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d68-11.ic36",	0x040000, 0xd456c124, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d68-12.ic34",	0x040000, 0xdec41397, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d49-01.12",		0x200000, 0x1dc89f1c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d49-02.8",		0x200000, 0x1e4c374f, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d49-06.11",		0x100000, 0x71ef4ee1, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d49-07.7",		0x100000, 0xe5655b8f, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d49-03.4",		0x200000, 0xcf9a8727, TAITO_SPRITESA },           //  8
	{ "d49-08.3",		0x100000, 0x7d3c6536, TAITO_SPRITESA },           //  9

	{ "d49-09.47",		0x080000, 0x257ede01, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d49-10.45",		0x080000, 0xf587b787, TAITO_CHARS_BYTESWAP },     // 11
	{ "d49-11.43",		0x080000, 0x11318b26, TAITO_CHARS },              // 12

	{ "d49-17.ic5",		0x020000, 0xf2058eba, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d49-18.ic6",		0x020000, 0xa0fdd270, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d49-04.38",		0x200000, 0x44b365a9, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d49-05.41",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 16

	{ "scfinals.nv",	0x000080, 0xf25945fc, TAITO_DEFAULT_EEPROM },     // 17 eeprom

};

STD_ROM_PICK(scfinals)
STD_ROM_FN(scfinals)

static INT32 scfinalsCallback()
{
	UINT32 *ROM = (UINT32 *)Taito68KRom1;

	ROM[0x5af0/4] = 0x4e754e71;
	ROM[0xdd0/4] = 0x4e714e75;

	return 0;
}

static INT32 scfinalsInit()
{
	return DrvInit(scfinalsCallback, f3_24bit_palette_update, 0, SCFINALS, 1);
}

struct BurnDriver BurnDrvScfinals = {
	"scfinals", NULL, NULL, NULL, "1993",
	"Super Cup Finals (Ver 2.2O 1994/01/13)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, scfinalsRomInfo, scfinalsRomName, NULL, NULL, F3InputInfo, NULL,
	scfinalsInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Super Cup Finals (Ver 2.1O 1993/11/19)

static struct BurnRomInfo scfinalsoRomDesc[] = {
	{ "d68-01.20",		0x040000, 0xcb951856, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d68-02.19",		0x040000, 0x4f94413a, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d68-04.18",		0x040000, 0x4a4e4972, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d68-03.17",		0x040000, 0xa40be699, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d49-01.12",		0x200000, 0x1dc89f1c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d49-02.8",		0x200000, 0x1e4c374f, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d49-06.11",		0x100000, 0x71ef4ee1, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d49-07.7",		0x100000, 0xe5655b8f, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d49-03.4",		0x200000, 0xcf9a8727, TAITO_SPRITESA },           //  8
	{ "d49-08.3",		0x100000, 0x7d3c6536, TAITO_SPRITESA },           //  9

	{ "d49-09.47",		0x080000, 0x257ede01, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d49-10.45",		0x080000, 0xf587b787, TAITO_CHARS_BYTESWAP },     // 11
	{ "d49-11.43",		0x080000, 0x11318b26, TAITO_CHARS },              // 12

	{ "d49-17.32",		0x020000, 0xf2058eba, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d49-18.33",		0x020000, 0xa0fdd270, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d49-04.38",		0x200000, 0x44b365a9, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d49-05.41",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 16

	{ "scfinalso.nv",	0x000080, 0x1319752e, TAITO_DEFAULT_EEPROM },     // 17 eeprom
};

STD_ROM_PICK(scfinalso)
STD_ROM_FN(scfinalso)

struct BurnDriver BurnDrvScfinalso = {
	"scfinalso", "scfinals", NULL, NULL, "1993",
	"Super Cup Finals (Ver 2.1O 1993/11/19)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, scfinalsoRomInfo, scfinalsoRomName, NULL, NULL, F3InputInfo, NULL,
	scfinalsInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Light Bringer (Ver 2.2O 1994/04/08)

static struct BurnRomInfo lightbrRomDesc[] = {
	{ "d69-25.ic40",	0x080000, 0x27f1b8be, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d69-26.ic38",	0x080000, 0x2ff7dba6, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d69-28.ic36",	0x080000, 0xa5546162, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d69-27.ic34",	0x080000, 0xe232a949, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d69-06.bin",		0x200000, 0xcb4aac81, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d69-07.bin",		0x200000, 0xb749f984, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d69-09.bin",		0x100000, 0xa96c19b8, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d69-10.bin",		0x100000, 0x36aa80c6, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d69-08.bin",		0x200000, 0x5b68d7d8, TAITO_SPRITESA },           //  8
	{ "d69-11.bin",		0x100000, 0xc11adf92, TAITO_SPRITESA },           //  9

	{ "d69-03.bin",		0x200000, 0x6999c86f, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d69-04.bin",		0x200000, 0xcc91dcb7, TAITO_CHARS_BYTESWAP },     // 11
	{ "d69-05.bin",		0x200000, 0xf9f5433c, TAITO_CHARS },              // 12

	{ "d69-18.bin",		0x020000, 0x04600d7b, TAITO_68KROM2_BYTESWAP }, // 13 68k Code
	{ "d69-19.bin",		0x020000, 0x1484e853, TAITO_68KROM2_BYTESWAP }, // 14

	{ "d69-01.bin",		0x200000, 0x9ac93ac2, TAITO_ES5505_BYTESWAP },  // 15 Ensoniq Samples
	{ "d69-02.bin",		0x200000, 0xdce28dd7, TAITO_ES5505_BYTESWAP },  // 16
};

STD_ROM_PICK(lightbr)
STD_ROM_FN(lightbr)

static INT32 lightbrInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, LIGHTBR, 2);
}

struct BurnDriver BurnDrvLightbr = {
	"lightbr", NULL, NULL, NULL, "1993",
	"Light Bringer (Ver 2.2O 1994/04/08)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, lightbrRomInfo, lightbrRomName, NULL, NULL, F3InputInfo, NULL,
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Dungeon Magic (Ver 2.1O 1994/02/18)

static struct BurnRomInfo dungeonmRomDesc[] = {
	{ "d69-20.bin",		0x080000, 0x33650fe4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d69-13.bin",		0x080000, 0xdec2ec17, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d69-15.bin",		0x080000, 0x323e1955, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d69-22.bin",		0x080000, 0xf99e175d, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d69-06.bin",		0x200000, 0xcb4aac81, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d69-07.bin",		0x200000, 0xb749f984, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d69-09.bin",		0x100000, 0xa96c19b8, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d69-10.bin",		0x100000, 0x36aa80c6, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d69-08.bin",		0x200000, 0x5b68d7d8, TAITO_SPRITESA },           //  8
	{ "d69-11.bin",		0x100000, 0xc11adf92, TAITO_SPRITESA },           //  9

	{ "d69-03.bin",		0x200000, 0x6999c86f, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d69-04.bin",		0x200000, 0xcc91dcb7, TAITO_CHARS_BYTESWAP },     // 11
	{ "d69-05.bin",		0x200000, 0xf9f5433c, TAITO_CHARS },              // 12

	{ "d69-18.bin",		0x020000, 0x04600d7b, TAITO_68KROM2_BYTESWAP }, // 13 68k Code
	{ "d69-19.bin",		0x020000, 0x1484e853, TAITO_68KROM2_BYTESWAP }, // 14

	{ "d69-01.bin",		0x200000, 0x9ac93ac2, TAITO_ES5505_BYTESWAP },  // 15 Ensoniq Samples
	{ "d69-02.bin",		0x200000, 0xdce28dd7, TAITO_ES5505_BYTESWAP },  // 16
};

STD_ROM_PICK(dungeonm)
STD_ROM_FN(dungeonm)

struct BurnDriver BurnDrvDungeonm = {
	"dungeonm", "lightbr", NULL, NULL, "1993",
	"Dungeon Magic (Ver 2.1O 1994/02/18)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, dungeonmRomInfo, dungeonmRomName, NULL, NULL, F3InputInfo, NULL,
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Dungeon Magic (Ver 2.1A 1994/02/18)

static struct BurnRomInfo dungeonmuRomDesc[] = {
	{ "d69-20.bin",		0x080000, 0x33650fe4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d69-13.bin",		0x080000, 0xdec2ec17, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d69-15.bin",		0x080000, 0x323e1955, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d69-21.bin",		0x080000, 0xc9d4e051, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d69-06.bin",		0x200000, 0xcb4aac81, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d69-07.bin",		0x200000, 0xb749f984, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d69-09.bin",		0x100000, 0xa96c19b8, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d69-10.bin",		0x100000, 0x36aa80c6, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d69-08.bin",		0x200000, 0x5b68d7d8, TAITO_SPRITESA },           //  8
	{ "d69-11.bin",		0x100000, 0xc11adf92, TAITO_SPRITESA },           //  9

	{ "d69-03.bin",		0x200000, 0x6999c86f, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d69-04.bin",		0x200000, 0xcc91dcb7, TAITO_CHARS_BYTESWAP },     // 11
	{ "d69-05.bin",		0x200000, 0xf9f5433c, TAITO_CHARS },              // 12

	{ "d69-18.bin",		0x020000, 0x04600d7b, TAITO_68KROM2_BYTESWAP }, // 13 68k Code
	{ "d69-19.bin",		0x020000, 0x1484e853, TAITO_68KROM2_BYTESWAP }, // 14

	{ "d69-01.bin",		0x200000, 0x9ac93ac2, TAITO_ES5505_BYTESWAP },  // 15 Ensoniq Samples
	{ "d69-02.bin",		0x200000, 0xdce28dd7, TAITO_ES5505_BYTESWAP },  // 16
};

STD_ROM_PICK(dungeonmu)
STD_ROM_FN(dungeonmu)

struct BurnDriver BurnDrvDungeonmu = {
	"dungeonmu", "lightbr", NULL, NULL, "1993",
	"Dungeon Magic (Ver 2.1A 1994/02/18)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, dungeonmuRomInfo, dungeonmuRomName, NULL, NULL, F3InputInfo, NULL,
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Light Bringer (Ver 2.1J 1994/02/18)

static struct BurnRomInfo lightbrjRomDesc[] = {
	{ "d69-20.bin",		0x080000, 0x33650fe4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d69-13.bin",		0x080000, 0xdec2ec17, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d69-15.bin",		0x080000, 0x323e1955, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d69-14.bin",		0x080000, 0x990bf945, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d69-06.bin",		0x200000, 0xcb4aac81, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d69-07.bin",		0x200000, 0xb749f984, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d69-09.bin",		0x100000, 0xa96c19b8, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d69-10.bin",		0x100000, 0x36aa80c6, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d69-08.bin",		0x200000, 0x5b68d7d8, TAITO_SPRITESA },           //  8
	{ "d69-11.bin",		0x100000, 0xc11adf92, TAITO_SPRITESA },           //  9

	{ "d69-03.bin",		0x200000, 0x6999c86f, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d69-04.bin",		0x200000, 0xcc91dcb7, TAITO_CHARS_BYTESWAP },     // 11
	{ "d69-05.bin",		0x200000, 0xf9f5433c, TAITO_CHARS },              // 12

	{ "d69-18.bin",		0x020000, 0x04600d7b, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d69-19.bin",		0x020000, 0x1484e853, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d69-01.bin",		0x200000, 0x9ac93ac2, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d69-02.bin",		0x200000, 0xdce28dd7, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(lightbrj)
STD_ROM_FN(lightbrj)

struct BurnDriver BurnDrvLightbrj = {
	"lightbrj", "lightbr", NULL, NULL, "1993",
	"Light Bringer (Ver 2.1J 1994/02/18)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, lightbrjRomInfo, lightbrjRomName, NULL, NULL, F3InputInfo, NULL,
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// International Cup '94 (Ver 2.2O 1994/05/26)

static struct BurnRomInfo intcup94RomDesc[] = {
	{ "d78-07.20",		0x020000, 0x8525d990, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d78-06.19",		0x020000, 0x42db1d41, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d78-05.18",		0x020000, 0x5f7fbbbc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d78-11.17",		0x020000, 0xbb9d2987, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d49-01.12",		0x200000, 0x1dc89f1c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d49-02.8",		0x200000, 0x1e4c374f, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d49-06.11",		0x100000, 0x71ef4ee1, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d49-07.7",		0x100000, 0xe5655b8f, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d49-03.4",		0x200000, 0xcf9a8727, TAITO_SPRITESA },           //  8
	{ "d49-08.3",		0x100000, 0x7d3c6536, TAITO_SPRITESA },           //  9

	{ "d78-01.47",		0x080000, 0x543f8967, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d78-02.45",		0x080000, 0xe8289394, TAITO_CHARS_BYTESWAP },     // 11
	{ "d78-03.43",		0x080000, 0xa8bc36e5, TAITO_CHARS },              // 12

	{ "d78-08.32",		0x020000, 0xa629d07c, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d78-09.33",		0x020000, 0x1f0efe01, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d49-04.38",		0x200000, 0x44b365a9, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d49-05.41",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(intcup94)
STD_ROM_FN(intcup94)

struct BurnDriver BurnDrvIntcup94 = {
	"intcup94", NULL, NULL, NULL, "1994",
	"International Cup '94 (Ver 2.2O 1994/05/26)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, intcup94RomInfo, intcup94RomName, NULL, NULL, F3InputInfo, NULL,
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Hat Trick Hero '94 (Ver 2.2A 1994/05/26)

static struct BurnRomInfo hthero94RomDesc[] = {
	{ "d78-07.20",		0x020000, 0x8525d990, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d78-06.19",		0x020000, 0x42db1d41, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d78-05.18",		0x020000, 0x5f7fbbbc, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d78-10.17",		0x020000, 0xcc9a1911, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d49-01.12",		0x200000, 0x1dc89f1c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d49-02.8",		0x200000, 0x1e4c374f, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d49-06.11",		0x100000, 0x71ef4ee1, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d49-07.7",		0x100000, 0xe5655b8f, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d49-03.4",		0x200000, 0xcf9a8727, TAITO_SPRITESA },           //  8
	{ "d49-08.3",		0x100000, 0x7d3c6536, TAITO_SPRITESA },           //  9

	{ "d78-01.47",		0x080000, 0x543f8967, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "d78-02.45",		0x080000, 0xe8289394, TAITO_CHARS_BYTESWAP },     // 11
	{ "d78-03.43",		0x080000, 0xa8bc36e5, TAITO_CHARS },              // 12

	{ "d78-08.32",		0x020000, 0xa629d07c, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "d78-09.33",		0x020000, 0x1f0efe01, TAITO_68KROM2_BYTESWAP },   // 14

	{ "d49-04.38",		0x200000, 0x44b365a9, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "d49-05.41",		0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 16
};

STD_ROM_PICK(hthero94)
STD_ROM_FN(hthero94)

struct BurnDriver BurnDrvHthero94 = {
	"hthero94", "intcup94", NULL, NULL, "1994",
	"Hat Trick Hero '94 (Ver 2.2A 1994/05/26)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, hthero94RomInfo, hthero94RomName, NULL, NULL, F3InputInfo, NULL,
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Recalhorn (Ver 1.42J 1994/5/11) (Prototype)

static struct BurnRomInfo recalhRomDesc[] = {
	{ "rh_mpr3.bin",	0x080000, 0x65202dd4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "rh_mpr2.bin",	0x080000, 0x3eda66db, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "rh_mpr1.bin",	0x080000, 0x536e74ca, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "rh_mpr0.bin",	0x080000, 0x38025817, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "rh_objl.bin",	0x100000, 0xc1772b55, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "rh_objm.bin",	0x100000, 0xef87c0fd, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "rh_scrl.bin",	0x100000, 0x1e3f6b79, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "rh_scrm.bin",	0x100000, 0x37200968, TAITO_CHARS_BYTESWAP },     //  7

	{ "rh_spr1.bin",	0x020000, 0x504cbc1d, TAITO_68KROM2_BYTESWAP },   //  8 68k Code
	{ "rh_spr0.bin",	0x020000, 0x78fba467, TAITO_68KROM2_BYTESWAP },   //  9

	{ "rh_snd0.bin",	0x200000, 0x386f5e1b, TAITO_ES5505_BYTESWAP },    // 10 Ensoniq Samples
	{ "rh_snd1.bin",	0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },    // 11
};

STD_ROM_PICK(recalh)
STD_ROM_FN(recalh)

static INT32 recalhInit()
{
	return DrvInit(NULL, f3_21bit_typeB_palette_update, 1, RECALH, 1);
}

struct BurnDriver BurnDrvRecalh = {
	"recalh", NULL, NULL, NULL, "1994",
	"Recalhorn (Ver 1.42J 1994/5/11) (Prototype)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, recalhRomInfo, recalhRomName, NULL, NULL, F3InputInfo, NULL,
	recalhInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Kaiser Knuckle (Ver 2.1O 1994/07/29)

static struct BurnRomInfo kaiserknRomDesc[] = {
	{ "d84-25.20",		0x080000, 0x2840893f, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d84-24.19",		0x080000, 0xbf20c755, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d84-23.18",		0x080000, 0x39f12a9b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d84-29.17",		0x080000, 0x9821f17a, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d84-03.rom",		0x200000, 0xd786f552, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d84-04.rom",		0x200000, 0xd1f32b5d, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d84-06.rom",		0x200000, 0xfa924dab, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d84-07.rom",		0x200000, 0x54517a6b, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d84-09.rom",		0x200000, 0xfaa78d98, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "d84-10.rom",		0x200000, 0xb84b7320, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "d84-19.rom",		0x080000, 0x6ddf77e5, TAITO_SPRITESA_BYTESWAP },  // 10
	{ "d84-20.rom",		0x080000, 0xf85041e5, TAITO_SPRITESA_BYTESWAP },  // 11
	{ "d84-05.rom",		0x200000, 0x31a3c75d, TAITO_SPRITESA },           // 12
	{ "d84-08.rom",		0x200000, 0x07347bf1, TAITO_SPRITESA },           // 13
	{ "d84-11.rom",		0x200000, 0xa062c1d4, TAITO_SPRITESA },           // 14
	{ "d84-21.rom",		0x080000, 0x89f68b66, TAITO_SPRITESA },           // 15

	{ "d84-12.rom",		0x200000, 0x66a7a9aa, TAITO_CHARS_BYTESWAP },     // 16 Layer Tiles
	{ "d84-13.rom",		0x200000, 0xae125516, TAITO_CHARS_BYTESWAP },     // 17
	{ "d84-16.rom",		0x100000, 0xbcff9b2d, TAITO_CHARS_BYTESWAP },     // 18
	{ "d84-17.rom",		0x100000, 0x0be37cc3, TAITO_CHARS_BYTESWAP },     // 19
	{ "d84-14.rom",		0x200000, 0x2b2e693e, TAITO_CHARS },              // 20
	{ "d84-18.rom",		0x100000, 0xe812bcc5, TAITO_CHARS },              // 21

	{ "d84-26.32",		0x040000, 0x4f5b8563, TAITO_68KROM2_BYTESWAP },   // 22 68k Code
	{ "d84-27.33",		0x040000, 0xfb0cb1ba, TAITO_68KROM2_BYTESWAP },   // 23

	{ "d84-01.rom",		0x200000, 0x9ad22149, TAITO_ES5505_BYTESWAP },    // 24 Ensoniq Samples
	{ "d84-02.rom",		0x200000, 0x9e1827e4, TAITO_ES5505_BYTESWAP },    // 25
	{ "d84-15.rom",		0x100000, 0x31ceb152, TAITO_ES5505_BYTESWAP },    // 26
};

STD_ROM_PICK(kaiserkn)
STD_ROM_FN(kaiserkn)

static INT32 kaiserknInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, KAISERKN, 2);
}

struct BurnDriver BurnDrvKaiserkn = {
	"kaiserkn", NULL, NULL, NULL, "1994",
	"Kaiser Knuckle (Ver 2.1O 1994/07/29)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, kaiserknRomInfo, kaiserknRomName, NULL, NULL, KnInputInfo, NULL,
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Kaiser Knuckle (Ver 2.1J 1994/07/29)

static struct BurnRomInfo kaiserknjRomDesc[] = {
	{ "d84-25.20",		0x080000, 0x2840893f, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d84-24.19",		0x080000, 0xbf20c755, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d84-23.18",		0x080000, 0x39f12a9b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d84-22.17",		0x080000, 0x762f9056, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d84-03.rom",		0x200000, 0xd786f552, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d84-04.rom",		0x200000, 0xd1f32b5d, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d84-06.rom",		0x200000, 0xfa924dab, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d84-07.rom",		0x200000, 0x54517a6b, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d84-09.rom",		0x200000, 0xfaa78d98, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "d84-10.rom",		0x200000, 0xb84b7320, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "d84-19.rom",		0x080000, 0x6ddf77e5, TAITO_SPRITESA_BYTESWAP },  // 10
	{ "d84-20.rom",		0x080000, 0xf85041e5, TAITO_SPRITESA_BYTESWAP },  // 11
	{ "d84-05.rom",		0x200000, 0x31a3c75d, TAITO_SPRITESA },           // 12
	{ "d84-08.rom",		0x200000, 0x07347bf1, TAITO_SPRITESA },           // 13
	{ "d84-11.rom",		0x200000, 0xa062c1d4, TAITO_SPRITESA },           // 14
	{ "d84-21.rom",		0x080000, 0x89f68b66, TAITO_SPRITESA },           // 15

	{ "d84-12.rom",		0x200000, 0x66a7a9aa, TAITO_CHARS_BYTESWAP },     // 16 Layer Tiles
	{ "d84-13.rom",		0x200000, 0xae125516, TAITO_CHARS_BYTESWAP },     // 17
	{ "d84-16.rom",		0x100000, 0xbcff9b2d, TAITO_CHARS_BYTESWAP },     // 18
	{ "d84-17.rom",		0x100000, 0x0be37cc3, TAITO_CHARS_BYTESWAP },     // 19
	{ "d84-14.rom",		0x200000, 0x2b2e693e, TAITO_CHARS },              // 20
	{ "d84-18.rom",		0x100000, 0xe812bcc5, TAITO_CHARS },              // 21

	{ "d84-26.32",		0x040000, 0x4f5b8563, TAITO_68KROM2_BYTESWAP },   // 22 68k Code
	{ "d84-27.33",		0x040000, 0xfb0cb1ba, TAITO_68KROM2_BYTESWAP },   // 23

	{ "d84-01.rom",		0x200000, 0x9ad22149, TAITO_ES5505_BYTESWAP },    // 24 Ensoniq Samples
	{ "d84-02.rom",		0x200000, 0x9e1827e4, TAITO_ES5505_BYTESWAP },    // 25
	{ "d84-15.rom",		0x100000, 0x31ceb152, TAITO_ES5505_BYTESWAP },    // 26
};

STD_ROM_PICK(kaiserknj)
STD_ROM_FN(kaiserknj)

struct BurnDriver BurnDrvKaiserknj = {
	"kaiserknj", "kaiserkn", NULL, NULL, "1994",
	"Kaiser Knuckle (Ver 2.1J 1994/07/29)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, kaiserknjRomInfo, kaiserknjRomName, NULL, NULL, KnInputInfo, NULL,
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Global Champion (Ver 2.1A 1994/07/29)

static struct BurnRomInfo gblchmpRomDesc[] = {
	{ "d84-25.20",		0x080000, 0x2840893f, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d84-24.19",		0x080000, 0xbf20c755, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d84-23.18",		0x080000, 0x39f12a9b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d84-28.17",		0x080000, 0xef26c1ec, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d84-03.rom",		0x200000, 0xd786f552, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d84-04.rom",		0x200000, 0xd1f32b5d, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d84-06.rom",		0x200000, 0xfa924dab, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d84-07.rom",		0x200000, 0x54517a6b, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d84-09.rom",		0x200000, 0xfaa78d98, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "d84-10.rom",		0x200000, 0xb84b7320, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "d84-19.rom",		0x080000, 0x6ddf77e5, TAITO_SPRITESA_BYTESWAP },  // 10
	{ "d84-20.rom",		0x080000, 0xf85041e5, TAITO_SPRITESA_BYTESWAP },  // 11
	{ "d84-05.rom",		0x200000, 0x31a3c75d, TAITO_SPRITESA },           // 12
	{ "d84-08.rom",		0x200000, 0x07347bf1, TAITO_SPRITESA },           // 13
	{ "d84-11.rom",		0x200000, 0xa062c1d4, TAITO_SPRITESA },           // 14
	{ "d84-21.rom",		0x080000, 0x89f68b66, TAITO_SPRITESA },           // 15

	{ "d84-12.rom",		0x200000, 0x66a7a9aa, TAITO_CHARS_BYTESWAP },     // 16 Layer Tiles
	{ "d84-13.rom",		0x200000, 0xae125516, TAITO_CHARS_BYTESWAP },     // 17
	{ "d84-16.rom",		0x100000, 0xbcff9b2d, TAITO_CHARS_BYTESWAP },     // 18
	{ "d84-17.rom",		0x100000, 0x0be37cc3, TAITO_CHARS_BYTESWAP },     // 19
	{ "d84-14.rom",		0x200000, 0x2b2e693e, TAITO_CHARS },              // 20
	{ "d84-18.rom",		0x100000, 0xe812bcc5, TAITO_CHARS },              // 21

	{ "d84-26.32",		0x040000, 0x4f5b8563, TAITO_68KROM2_BYTESWAP },   // 22 68k Code
	{ "d84-27.33",		0x040000, 0xfb0cb1ba, TAITO_68KROM2_BYTESWAP },   // 23

	{ "d84-01.rom",		0x200000, 0x9ad22149, TAITO_ES5505_BYTESWAP },    // 24 Ensoniq Samples
	{ "d84-02.rom",		0x200000, 0x9e1827e4, TAITO_ES5505_BYTESWAP },    // 25
	{ "d84-15.rom",		0x100000, 0x31ceb152, TAITO_ES5505_BYTESWAP },    // 26
};

STD_ROM_PICK(gblchmp)
STD_ROM_FN(gblchmp)

struct BurnDriver BurnDrvGblchmp = {
	"gblchmp", "kaiserkn", NULL, NULL, "1994",
	"Global Champion (Ver 2.1A 1994/07/29)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, gblchmpRomInfo, gblchmpRomName, NULL, NULL, KnInputInfo, NULL,
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Dan-Ku-Ga (Ver 0.0J 1994/12/13) (Prototype)

static struct BurnRomInfo dankugaRomDesc[] = {
	{ "dkg_mpr3.20",	0x080000, 0xee1531ca, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "dkg_mpr2.19",	0x080000, 0x18a4748b, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "dkg_mpr1.18",	0x080000, 0x97566f69, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "dkg_mpr0.17",	0x080000, 0xad6ada07, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d84-03.rom",		0x200000, 0xd786f552, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d84-04.rom",		0x200000, 0xd1f32b5d, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d84-06.rom",		0x200000, 0xfa924dab, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d84-07.rom",		0x200000, 0x54517a6b, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d84-09.rom",		0x200000, 0xfaa78d98, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "d84-10.rom",		0x200000, 0xb84b7320, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "d84-19.rom",		0x080000, 0x6ddf77e5, TAITO_SPRITESA_BYTESWAP },  // 10
	{ "d84-20.rom",		0x080000, 0xf85041e5, TAITO_SPRITESA_BYTESWAP },  // 11
	{ "d84-05.rom",		0x200000, 0x31a3c75d, TAITO_SPRITESA },           // 12
	{ "d84-08.rom",		0x200000, 0x07347bf1, TAITO_SPRITESA },           // 13
	{ "d84-11.rom",		0x200000, 0xa062c1d4, TAITO_SPRITESA },           // 14
	{ "d84-21.rom",		0x080000, 0x89f68b66, TAITO_SPRITESA },           // 15

	{ "d84-12.rom",		0x200000, 0x66a7a9aa, TAITO_CHARS_BYTESWAP },     // 16 Layer Tiles
	{ "d84-13.rom",		0x200000, 0xae125516, TAITO_CHARS_BYTESWAP },     // 17
	{ "d84-16.rom",		0x100000, 0xbcff9b2d, TAITO_CHARS_BYTESWAP },     // 18
	{ "d84-17.rom",		0x100000, 0x0be37cc3, TAITO_CHARS_BYTESWAP },     // 19
	{ "d84-14.rom",		0x200000, 0x2b2e693e, TAITO_CHARS },              // 20
	{ "d84-18.rom",		0x100000, 0xe812bcc5, TAITO_CHARS },              // 21

	{ "d84-26.32",		0x040000, 0x4f5b8563, TAITO_68KROM2_BYTESWAP },   // 22 68k Code
	{ "d84-27.33",		0x040000, 0xfb0cb1ba, TAITO_68KROM2_BYTESWAP },   // 23

	{ "d84-01.rom",		0x200000, 0x9ad22149, TAITO_ES5505_BYTESWAP },    // 24 Ensoniq Samples
	{ "d84-02.rom",		0x200000, 0x9e1827e4, TAITO_ES5505_BYTESWAP },    // 25
	{ "d84-15.rom",		0x100000, 0x31ceb152, TAITO_ES5505_BYTESWAP },    // 26
};

STD_ROM_PICK(dankuga)
STD_ROM_FN(dankuga)

struct BurnDriver BurnDrvDankuga = {
	"dankuga", NULL, NULL, NULL, "1994",
	"Dan-Ku-Ga (Ver 0.0J 1994/12/13) (Prototype)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, dankugaRomInfo, dankugaRomName, NULL, NULL, KnInputInfo, NULL,
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Darius Gaiden - Silver Hawk (Ver 2.5O 1994/09/19)

static struct BurnRomInfo dariusgRomDesc[] = {
	{ "d87-12.bin",		0x080000, 0xde78f328, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d87-11.bin",		0x080000, 0xf7bed18e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d87-10.bin",		0x080000, 0x4149f66f, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d87-16.bin",		0x080000, 0x8f7e5901, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d87-03.bin",		0x200000, 0x4be1666e, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d87-04.bin",		0x200000, 0x2616002c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d87-05.bin",		0x200000, 0x4e5891a9, TAITO_SPRITESA },           //  6

	{ "d87-06.bin",		0x200000, 0x3b97a07c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d87-17.bin",		0x200000, 0xe601d63e, TAITO_CHARS_BYTESWAP },     //  8
	{ "d87-08.bin",		0x200000, 0x76d23602, TAITO_CHARS },              //  9

	{ "d87-13.bin",		0x040000, 0x15b1fff4, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d87-14.bin",		0x040000, 0xeecda29a, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d87-01.bin",		0x200000, 0x3848a110, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d87-02.bin",		0x200000, 0x9250abae, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(dariusg)
STD_ROM_FN(dariusg)

static INT32 dariusgInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, DARIUSG, 1);
}

struct BurnDriver BurnDrvDariusg = {
	"dariusg", NULL, NULL, NULL, "1994",
	"Darius Gaiden - Silver Hawk (Ver 2.5O 1994/09/19)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, dariusgRomInfo, dariusgRomName, NULL, NULL, F3InputInfo, NULL,
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Darius Gaiden - Silver Hawk (Ver 2.5J 1994/09/19)

static struct BurnRomInfo dariusgjRomDesc[] = {
	{ "d87-12.bin",		0x080000, 0xde78f328, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d87-11.bin",		0x080000, 0xf7bed18e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d87-10.bin",		0x080000, 0x4149f66f, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d87-09.bin",		0x080000, 0x6170382d, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d87-03.bin",		0x200000, 0x4be1666e, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d87-04.bin",		0x200000, 0x2616002c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d87-05.bin",		0x200000, 0x4e5891a9, TAITO_SPRITESA },           //  6

	{ "d87-06.bin",		0x200000, 0x3b97a07c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d87-17.bin",		0x200000, 0xe601d63e, TAITO_CHARS_BYTESWAP },     //  8
	{ "d87-08.bin",		0x200000, 0x76d23602, TAITO_CHARS },              //  9

	{ "d87-13.bin",		0x040000, 0x15b1fff4, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d87-14.bin",		0x040000, 0xeecda29a, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d87-01.bin",		0x200000, 0x3848a110, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d87-02.bin",		0x200000, 0x9250abae, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(dariusgj)
STD_ROM_FN(dariusgj)

struct BurnDriver BurnDrvDariusgj = {
	"dariusgj", "dariusg", NULL, NULL, "1994",
	"Darius Gaiden - Silver Hawk (Ver 2.5J 1994/09/19)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, dariusgjRomInfo, dariusgjRomName, NULL, NULL, F3InputInfo, NULL,
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Darius Gaiden - Silver Hawk (Ver 2.5A 1994/09/19)

static struct BurnRomInfo dariusguRomDesc[] = {
	{ "d87-12.bin",		0x080000, 0xde78f328, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d87-11.bin",		0x080000, 0xf7bed18e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d87-10.bin",		0x080000, 0x4149f66f, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d87-15.bin",		0x080000, 0xf8796997, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d87-03.bin",		0x200000, 0x4be1666e, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d87-04.bin",		0x200000, 0x2616002c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d87-05.bin",		0x200000, 0x4e5891a9, TAITO_SPRITESA },           //  6

	{ "d87-06.bin",		0x200000, 0x3b97a07c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d87-17.bin",		0x200000, 0xe601d63e, TAITO_CHARS_BYTESWAP },     //  8
	{ "d87-08.bin",		0x200000, 0x76d23602, TAITO_CHARS },              //  9

	{ "d87-13.bin",		0x040000, 0x15b1fff4, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d87-14.bin",		0x040000, 0xeecda29a, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d87-01.bin",		0x200000, 0x3848a110, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d87-02.bin",		0x200000, 0x9250abae, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(dariusgu)
STD_ROM_FN(dariusgu)

struct BurnDriver BurnDrvDariusgu = {
	"dariusgu", "dariusg", NULL, NULL, "1994",
	"Darius Gaiden - Silver Hawk (Ver 2.5A 1994/09/19)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, dariusguRomInfo, dariusguRomName, NULL, NULL, F3InputInfo, NULL,
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Darius Gaiden - Silver Hawk Extra Version (Ver 2.7J 1995/03/06) (Official Hack)

static struct BurnRomInfo dariusgxRomDesc[] = {
	{ "dge_mpr3.bin",	0x080000, 0x1c1e24a7, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "dge_mpr2.bin",	0x080000, 0x7be23e23, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "dge_mpr1.bin",	0x080000, 0xbc030f6f, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "dge_mpr0.bin",	0x080000, 0xc5bd135c, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d87-03.bin",		0x200000, 0x4be1666e, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d87-04.bin",		0x200000, 0x2616002c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d87-05.bin",		0x200000, 0x4e5891a9, TAITO_SPRITESA },           //  6

	{ "d87-06.bin",		0x200000, 0x3b97a07c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d87-17.bin",		0x200000, 0xe601d63e, TAITO_CHARS_BYTESWAP },     //  8
	{ "d87-08.bin",		0x200000, 0x76d23602, TAITO_CHARS },              //  9

	{ "d87-13.bin",		0x040000, 0x15b1fff4, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d87-14.bin",		0x040000, 0xeecda29a, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d87-01.bin",		0x200000, 0x3848a110, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d87-02.bin",		0x200000, 0x9250abae, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(dariusgx)
STD_ROM_FN(dariusgx)

struct BurnDriver BurnDrvDariusgx = {
	"dariusgx", NULL, NULL, NULL, "1994",
	"Darius Gaiden - Silver Hawk Extra Version (Ver 2.7J 1995/03/06) (Official Hack)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, dariusgxRomInfo, dariusgxRomName, NULL, NULL, F3InputInfo, NULL,
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Bubble Bobble II (Ver 2.6O 1994/12/16)

static struct BurnRomInfo bublbob2RomDesc[] = {
	{ "d90-21.ic20",	0x040000, 0x2a2b771a, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d90-20.ic19",	0x040000, 0xf01f63b6, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d90-19.ic18",	0x040000, 0x86eef19a, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d90-18.ic17",	0x040000, 0xf5b8cdce, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d90-03",		0x100000, 0x6fa894a1, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d90-02",		0x100000, 0x5ab04ca2, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d90-01",		0x100000, 0x8aedb9e5, TAITO_SPRITESA },           //  6

	{ "d90-08",		0x100000, 0x25a4fb2c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d90-07",		0x100000, 0xb436b42d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d90-06",		0x100000, 0x166a72b8, TAITO_CHARS },              //  9

	{ "d90-13.ic32",0x040000, 0x6762bd90, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d90-14.ic33",0x040000, 0x8e33357e, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d90-04",		0x200000, 0xfeee5fda, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d90-05",		0x200000, 0xc192331f, TAITO_ES5505_BYTESWAP },    // 13
	
	{ "d77-14_palce16v8q-15.ic21.bin", 0x117, 0x2c798a1c, BRF_OPT },
	{ "d77-12_palce16v8q-15.ic48.bin", 0x117, 0xb1cc6195, BRF_OPT },
	{ "d77-11_palce16v8q-15.ic37.bin", 0x117, 0xa733f0de, BRF_OPT },
};

STD_ROM_PICK(bublbob2)
STD_ROM_FN(bublbob2)

static INT32 bublbob2Init()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, BUBSYMPH, 1);
}

struct BurnDriver BurnDrvBublbob2 = {
	"bublbob2", NULL, NULL, NULL, "1994",
	"Bubble Bobble II (Ver 2.6O 1994/12/16)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bublbob2RomInfo, bublbob2RomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Bubble Bobble II (Ver 2.5O 1994/10/05)

static struct BurnRomInfo bublbob2oRomDesc[] = {
	{ "d90-12",		0x040000, 0x9e523996, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d90-11",		0x040000, 0xedfdbb7f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d90-10",		0x040000, 0x8e957d3d, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d90-17",		0x040000, 0x711f1894, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d90-03",		0x100000, 0x6fa894a1, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d90-02",		0x100000, 0x5ab04ca2, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d90-01",		0x100000, 0x8aedb9e5, TAITO_SPRITESA },           //  6

	{ "d90-08",		0x100000, 0x25a4fb2c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d90-07",		0x100000, 0xb436b42d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d90-06",		0x100000, 0x166a72b8, TAITO_CHARS },              //  9

	{ "d90-13.ic32",0x040000, 0x6762bd90, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d90-14.ic33",0x040000, 0x8e33357e, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d90-04",		0x200000, 0xfeee5fda, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d90-05",		0x200000, 0xc192331f, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(bublbob2o)
STD_ROM_FN(bublbob2o)

struct BurnDriver BurnDrvBublbob2o = {
	"bublbob2o", "bublbob2", NULL, NULL, "1994",
	"Bubble Bobble II (Ver 2.5O 1994/10/05)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bublbob2oRomInfo, bublbob2oRomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Bubble Symphony (Ver 2.5O 1994/10/05)

static struct BurnRomInfo bubsympheRomDesc[] = {
	{ "d90-12",		0x040000, 0x9e523996, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d90-11",		0x040000, 0xedfdbb7f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d90-10",		0x040000, 0x8e957d3d, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d90-16",		0x040000, 0xd12ef19b, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d90-03",		0x100000, 0x6fa894a1, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d90-02",		0x100000, 0x5ab04ca2, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d90-01",		0x100000, 0x8aedb9e5, TAITO_SPRITESA },           //  6

	{ "d90-08",		0x100000, 0x25a4fb2c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d90-07",		0x100000, 0xb436b42d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d90-06",		0x100000, 0x166a72b8, TAITO_CHARS },              //  9

	{ "d90-13.ic32",0x040000, 0x6762bd90, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d90-14.ic33",0x040000, 0x8e33357e, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d90-04",		0x200000, 0xfeee5fda, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d90-05",		0x200000, 0xc192331f, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(bubsymphe)
STD_ROM_FN(bubsymphe)

struct BurnDriver BurnDrvBubsymphe = {
	"bubsymphe", "bublbob2", NULL, NULL, "1994",
	"Bubble Symphony (Ver 2.5O 1994/10/05)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubsympheRomInfo, bubsympheRomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Bubble Symphony (Ver 2.5J 1994/10/05)

static struct BurnRomInfo bubsymphjRomDesc[] = {
	{ "d90-12",		0x040000, 0x9e523996, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d90-11",		0x040000, 0xedfdbb7f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d90-10",		0x040000, 0x8e957d3d, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d90-09",		0x040000, 0x3f2090b7, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d90-03",		0x100000, 0x6fa894a1, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d90-02",		0x100000, 0x5ab04ca2, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d90-01",		0x100000, 0x8aedb9e5, TAITO_SPRITESA },           //  6

	{ "d90-08",		0x100000, 0x25a4fb2c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d90-07",		0x100000, 0xb436b42d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d90-06",		0x100000, 0x166a72b8, TAITO_CHARS },              //  9

	{ "d90-13.ic32",0x040000, 0x6762bd90, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d90-14.ic33",0x040000, 0x8e33357e, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d90-04",		0x200000, 0xfeee5fda, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d90-05",		0x200000, 0xc192331f, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.bin",	0x000104, 0xb371532b, BRF_OPT }, // 14 plds
	{ "pal16l8a-d77-10.bin",	0x000104, 0x42f59227, BRF_OPT }, // 15
	{ "palce16v8q-d77-11.bin",	0x000117, 0xeacc294e, BRF_OPT }, // 16
	{ "palce16v8q-d77-12.bin",	0x000117, 0xe9920cfe, BRF_OPT }, // 17
	{ "palce16v8q-d77-14.bin",	0x000117, 0x7427e777, BRF_OPT }, // 18
};

STD_ROM_PICK(bubsymphj)
STD_ROM_FN(bubsymphj)

struct BurnDriver BurnDrvBubsymphj = {
	"bubsymphj", "bublbob2", NULL, NULL, "1994",
	"Bubble Symphony (Ver 2.5J 1994/10/05)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubsymphjRomInfo, bubsymphjRomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Bubble Bobble II (Ver 0.0J 1993/12/13, prototype)

static struct BurnRomInfo bublbob2pRomDesc[] = {
	{ "soft-3-8c9b.ic60",		0x40000, 0x15d0594e, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "soft-2-0587.ic61",		0x40000, 0xd1a5231f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "soft-1-9a9c.ic62",		0x40000, 0xc11a4d26, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "soft-0-a523.ic63",		0x40000, 0x58131f9e, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "cq80-obj-0l-c166.ic8",	0x80000, 0x9bff223b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "cq80-obj-0m-24f4.ic30",	0x80000, 0xee71f643, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "cq80-obj-0h-990d.ic32",	0x80000, 0x4d3a78e0, TAITO_SPRITESA },           //  6

	{ "cq80-scr0-5ba4.ic7",		0x80000, 0x044dc38b, TAITO_CHARS_BYTESWAP32 },   //  7 Layer Tiles
	{ "cq80-scr2-cc11.ic5",		0x80000, 0xb81aa2c7, TAITO_CHARS_BYTESWAP32 },   //  8
	{ "cq80-scr1-a5f3.ic6",		0x80000, 0x3cf3a3ba, TAITO_CHARS_BYTESWAP32 },   //  9
	{ "cq80-scr3-4266.ic4",		0x80000, 0xc114583f, TAITO_CHARS_BYTESWAP32 },   // 10
	{ "cq80-scr4-7fe1.ic3",		0x80000, 0x2bba1728, TAITO_CHARS_BYTESWAP  },    // 11

	{ "snd-h-348f.ic66",		0x20000, 0xf66e60f2, TAITO_68KROM2_BYTESWAP },   // 12 68k Code
	{ "snd-l-4ec1.ic65",		0x20000, 0xd302d8bc, TAITO_68KROM2_BYTESWAP },   // 13

	{ "cq80-snd-data0-7b5f.ic43",	0x80000, 0xbf8f26d3, TAITO_ES5505_BYTESWAP },    // 14 Ensoniq Samples
	{ "cq80-snd-data1-933b.ic44",	0x80000, 0x62b00475, TAITO_ES5505_BYTESWAP },    // 15
	{ "cq80-snd3-std5-3a9c.ic10",	0x80000, 0x26312451, TAITO_ES5505_BYTESWAP },    // 16
	{ "cq80-snd2-std6-a148.ic11",	0x80000, 0x2edaa9dc, TAITO_ES5505_BYTESWAP },    // 17

	{ "bb2proto-ic12.bin",		0x002e5, 0xacf20b88, 0 | BRF_OPT },              // 18 pals
	{ "bb2proto-ic24.bin",		0x002e5, 0xd15a4987, 0 | BRF_OPT },              // 19
	{ "pal16l8b.ic57.bin",		0x00104, 0x74b4d8be, 0 | BRF_OPT },              // 20
	{ "pal16l8b.ic58.bin",		0x00104, 0x17e2c9b8, 0 | BRF_OPT },              // 21
	{ "pal16l8b.ic59.bin",		0x00104, 0xdc0db200, 0 | BRF_OPT },              // 22
	{ "pal16l8b.ic64.bin",		0x00104, 0x3aed3d98, 0 | BRF_OPT },              // 23
};

STD_ROM_PICK(bublbob2p)
STD_ROM_FN(bublbob2p)

static INT32 bublbob2pRomCallback()
{
	memcpy (TaitoES5505Rom + 0x600000, TaitoES5505Rom + 0x200000, 0x200000);
	memset (TaitoES5505Rom + 0x200000, 0, 0x200000);

	return 0;
}

static INT32 bublbob2pInit()
{
	return DrvInit(bublbob2pRomCallback, f3_24bit_palette_update, 1, BUBSYMPH, 1);
}

struct BurnDriver BurnDrvBublbob2p = {
	"bublbob2p", "bublbob2", NULL, NULL, "1994",
	"Bubble Bobble II (Ver 0.0J 1993/12/13, prototype)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bublbob2pRomInfo, bublbob2pRomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2pInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Bubble Symphony (bootleg with OKI6295)

static struct BurnRomInfo bubsymphbRomDesc[] = {
	{ "bsb_d12.bin",	0x40000, 0xd05160fc, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "bsb_d13.bin",	0x40000, 0x83fc0d2c, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "bsb_d14.bin",	0x40000, 0xe6d49bb7, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "bsb_d15.bin",	0x40000, 0x014cf8e0, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "bsb_d18.bin",	0x80000, 0x22d7eeb5, TAITO_SPRITESA },           //  4 Sprites
	{ "bsb_d19.bin",	0x80000, 0xd36801fd, TAITO_SPRITESA },           //  5
	{ "bsb_d20.bin",	0x80000, 0x20222e15, TAITO_SPRITESA },           //  6
	{ "bsb_d17.bin",	0x80000, 0xea2eadfc, TAITO_SPRITESA },           //  7
	{ "bsb_d16.bin",	0x80000, 0xedccd4e0, TAITO_SPRITESA },           //  8

	{ "bsb_d13b.bin",	0x80000, 0x430af2aa, TAITO_CHARS },              //  9 Layer Tiles
	{ "bsb_d14b.bin",	0x80000, 0xc006e832, TAITO_CHARS },              // 10
	{ "bsb_d15b.bin",	0x80000, 0x74644ad4, TAITO_CHARS },              // 11
	{ "bsb_d12b.bin",	0x80000, 0xcb2e2abb, TAITO_CHARS },              // 12
	{ "bsb_d11b.bin",	0x80000, 0xd0607829, TAITO_CHARS },              // 13

	{ "bsb_d11.bin",	0x80000, 0x26bdc617, TAITO_MSM6295 },            // 14 oki
};

STD_ROM_PICK(bubsymphb)
STD_ROM_FN(bubsymphb)

struct BurnDriverD BurnDrvBubsymphb = {
	"bubsymphb", "bublbob2", NULL, NULL, "1994",
	"Bubble Symphony (bootleg with OKI6295)\0", NULL, "bootleg", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubsymphbRomInfo, bubsymphbRomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Bubble Symphony (Ver 2.5A 1994/10/05)

static struct BurnRomInfo bubsymphuRomDesc[] = {
	{ "d90-12",		0x040000, 0x9e523996, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d90-11",		0x040000, 0xedfdbb7f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d90-10",		0x040000, 0x8e957d3d, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d90-15",		0x040000, 0x06182802, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d90-03",		0x100000, 0x6fa894a1, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d90-02",		0x100000, 0x5ab04ca2, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d90-01",		0x100000, 0x8aedb9e5, TAITO_SPRITESA },           //  6

	{ "d90-08",		0x100000, 0x25a4fb2c, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "d90-07",		0x100000, 0xb436b42d, TAITO_CHARS_BYTESWAP },     //  8
	{ "d90-06",		0x100000, 0x166a72b8, TAITO_CHARS },              //  9

	{ "d90-13.ic32",0x040000, 0x6762bd90, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "d90-14.ic33",0x040000, 0x8e33357e, TAITO_68KROM2_BYTESWAP },   // 11

	{ "d90-04",		0x200000, 0xfeee5fda, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "d90-05",		0x200000, 0xc192331f, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(bubsymphu)
STD_ROM_FN(bubsymphu)

struct BurnDriver BurnDrvBubsymphu = {
	"bubsymphu", "bublbob2", NULL, NULL, "1994",
	"Bubble Symphony (Ver 2.5A 1994/10/05)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubsymphuRomInfo, bubsymphuRomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Space Invaders DX (Ver 2.6J 1994/09/14) (F3 Version)

static struct BurnRomInfo spcinvdjRomDesc[] = {
	{ "d93-04.bin",		0x20000, 0xcd9a4e5c, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d93-03.bin",		0x20000, 0x0174bfc1, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d93-02.bin",		0x20000, 0x01922b31, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d93-01.bin",		0x20000, 0x4a74ab1c, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d93-07.12",		0x80000, 0x8cf5b972, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d93-08.08",		0x80000, 0x4c11af2b, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "d93-09.47",		0x20000, 0x9076f663, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "d93-10.45",		0x20000, 0x8a3f531b, TAITO_CHARS_BYTESWAP },     //  7

	{ "d93-05.bin",		0x20000, 0xff365596, TAITO_68KROM2_BYTESWAP },   //  8 68k Code
	{ "d93-06.bin",		0x20000, 0xef7ad400, TAITO_68KROM2_BYTESWAP },   //  9

	{ "d93-11.38",		0x80000, 0xdf5853de, TAITO_ES5505_BYTESWAP },    // 10 Ensoniq Samples
	{ "d93-12.39",		0x80000, 0xb0f71d60, TAITO_ES5505_BYTESWAP },    // 11
	{ "d93-13.40",		0x80000, 0x26312451, TAITO_ES5505_BYTESWAP },    // 12
	{ "d93-14.41",		0x80000, 0x2edaa9dc, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(spcinvdj)
STD_ROM_FN(spcinvdj)

static INT32 spcinvdjInit()
{
	return DrvInit(NULL, f3_12bit_palette_update, 1, SPCINVDX, 1);
}

struct BurnDriver BurnDrvSpcinvdj = {
	"spcinvdj", "spacedx", NULL, NULL, "1994",
	"Space Invaders DX (Ver 2.6J 1994/09/14) (F3 Version)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, spcinvdjRomInfo, spcinvdjRomName, NULL, NULL, F3InputInfo, NULL,
	spcinvdjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Taito Power Goal (Ver 2.5O 1994/11/03)

static struct BurnRomInfo pwrgoalRomDesc[] = {
	{ "d94-18.bin",		0x040000, 0xb92681c3, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d94-17.bin",		0x040000, 0x6009333e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d94-16.bin",		0x040000, 0xc6dbc9c8, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d94-22.rom",		0x040000, 0xf672e487, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d94-09.bin",		0x200000, 0x425e6bec, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d94-06.bin",		0x200000, 0x0ed1df55, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d94-08.bin",		0x200000, 0xbd909caf, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d94-05.bin",		0x200000, 0x121c8542, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d94-07.bin",		0x200000, 0xc8c95e49, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "d94-04.bin",		0x200000, 0x24958b50, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "d94-03.bin",		0x200000, 0x95e32072, TAITO_SPRITESA },           // 10
	{ "d94-02.bin",		0x200000, 0xf460b9ac, TAITO_SPRITESA },           // 11
	{ "d94-01.bin",		0x200000, 0x410ffccd, TAITO_SPRITESA },           // 12

	{ "d94-14.bin",		0x100000, 0xb8ba5761, TAITO_CHARS_BYTESWAP },     // 13 Layer Tiles
	{ "d94-13.bin",		0x100000, 0xcafc68ce, TAITO_CHARS_BYTESWAP },     // 14
	{ "d94-12.bin",		0x100000, 0x47064189, TAITO_CHARS },              // 15

	{ "d94-19.bin",		0x040000, 0xc93dbcf4, TAITO_68KROM2_BYTESWAP }, // 16 68k Code
	{ "d94-20.bin",		0x040000, 0xf232bf64, TAITO_68KROM2_BYTESWAP }, // 17

	{ "d94-10.bin",		0x200000, 0xa22563ae, TAITO_ES5505_BYTESWAP },  // 18 Ensoniq Samples
	{ "d94-11.bin",		0x200000, 0x61ed83fa, TAITO_ES5505_BYTESWAP },  // 19
};

STD_ROM_PICK(pwrgoal)
STD_ROM_FN(pwrgoal)

static INT32 pwrgoalInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, HTHERO95, 1);
}

struct BurnDriver BurnDrvPwrgoal = {
	"pwrgoal", NULL, NULL, NULL, "1994",
	"Taito Power Goal (Ver 2.5O 1994/11/03)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, pwrgoalRomInfo, pwrgoalRomName, NULL, NULL, F3InputInfo, NULL,
	pwrgoalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Hat Trick Hero '95 (Ver 2.5J 1994/11/03)

static struct BurnRomInfo hthero95RomDesc[] = {
	{ "d94-18.bin",		0x040000, 0xb92681c3, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d94-17.bin",		0x040000, 0x6009333e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d94-16.bin",		0x040000, 0xc6dbc9c8, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d94-15.bin",		0x040000, 0x187c85ab, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d94-09.bin",		0x200000, 0x425e6bec, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d94-06.bin",		0x200000, 0x0ed1df55, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d94-08.bin",		0x200000, 0xbd909caf, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d94-05.bin",		0x200000, 0x121c8542, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d94-07.bin",		0x200000, 0xc8c95e49, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "d94-04.bin",		0x200000, 0x24958b50, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "d94-03.bin",		0x200000, 0x95e32072, TAITO_SPRITESA },           // 10
	{ "d94-02.bin",		0x200000, 0xf460b9ac, TAITO_SPRITESA },           // 11
	{ "d94-01.bin",		0x200000, 0x410ffccd, TAITO_SPRITESA },           // 12

	{ "d94-14.bin",		0x100000, 0xb8ba5761, TAITO_CHARS_BYTESWAP },     // 13 Layer Tiles
	{ "d94-13.bin",		0x100000, 0xcafc68ce, TAITO_CHARS_BYTESWAP },     // 14
	{ "d94-12.bin",		0x100000, 0x47064189, TAITO_CHARS },              // 15

	{ "d94-19.bin",		0x040000, 0xc93dbcf4, TAITO_68KROM2_BYTESWAP },   // 16 68k Code
	{ "d94-20.bin",		0x040000, 0xf232bf64, TAITO_68KROM2_BYTESWAP },   // 17

	{ "d94-10.bin",		0x200000, 0xa22563ae, TAITO_ES5505_BYTESWAP },    // 18 Ensoniq Samples
	{ "d94-11.bin",		0x200000, 0x61ed83fa, TAITO_ES5505_BYTESWAP },    // 19
};

STD_ROM_PICK(hthero95)
STD_ROM_FN(hthero95)

struct BurnDriver BurnDrvHthero95 = {
	"hthero95", "pwrgoal", NULL, NULL, "1994",
	"Hat Trick Hero '95 (Ver 2.5J 1994/11/03)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, hthero95RomInfo, hthero95RomName, NULL, NULL, F3InputInfo, NULL,
	pwrgoalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Hat Trick Hero '95 (Ver 2.5A 1994/11/03)

static struct BurnRomInfo hthero95uRomDesc[] = {
	{ "d94-18.bin",		0x040000, 0xb92681c3, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d94-17.bin",		0x040000, 0x6009333e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d94-16.bin",		0x040000, 0xc6dbc9c8, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d94-21.bin",		0x040000, 0x8175d411, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d94-09.bin",		0x200000, 0x425e6bec, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d94-06.bin",		0x200000, 0x0ed1df55, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "d94-08.bin",		0x200000, 0xbd909caf, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "d94-05.bin",		0x200000, 0x121c8542, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "d94-07.bin",		0x200000, 0xc8c95e49, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "d94-04.bin",		0x200000, 0x24958b50, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "d94-03.bin",		0x200000, 0x95e32072, TAITO_SPRITESA },           // 10
	{ "d94-02.bin",		0x200000, 0xf460b9ac, TAITO_SPRITESA },           // 11
	{ "d94-01.bin",		0x200000, 0x410ffccd, TAITO_SPRITESA },           // 12

	{ "d94-14.bin",		0x100000, 0xb8ba5761, TAITO_CHARS_BYTESWAP },     // 13 Layer Tiles
	{ "d94-13.bin",		0x100000, 0xcafc68ce, TAITO_CHARS_BYTESWAP },     // 14
	{ "d94-12.bin",		0x100000, 0x47064189, TAITO_CHARS },              // 15

	{ "d94-19.bin",		0x040000, 0xc93dbcf4, TAITO_68KROM2_BYTESWAP },   // 16 68k Code
	{ "d94-20.bin",		0x040000, 0xf232bf64, TAITO_68KROM2_BYTESWAP },   // 17

	{ "d94-10.bin",		0x200000, 0xa22563ae, TAITO_ES5505_BYTESWAP },    // 18 Ensoniq Samples
	{ "d94-11.bin",		0x200000, 0x61ed83fa, TAITO_ES5505_BYTESWAP },    // 19
};

STD_ROM_PICK(hthero95u)
STD_ROM_FN(hthero95u)

struct BurnDriver BurnDrvHthero95u = {
	"hthero95u", "pwrgoal", NULL, NULL, "1994",
	"Hat Trick Hero '95 (Ver 2.5A 1994/11/03)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, hthero95uRomInfo, hthero95uRomName, NULL, NULL, F3InputInfo, NULL,
	pwrgoalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Quiz Theater - 3tsu no Monogatari (Ver 2.3J 1994/11/10)

static struct BurnRomInfo qtheaterRomDesc[] = {
	{ "d95-12.20",		0x080000, 0xfcee76ee, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "d95-11.19",		0x080000, 0xb3c2b8d5, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "d95-10.18",		0x080000, 0x85236e40, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "d95-09.17",		0x080000, 0xf456519c, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "d95-02.12",		0x200000, 0x74ce6f3e, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "d95-01.8",		0x200000, 0x141beb7d, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "d95-06.47",		0x200000, 0x70a0dcbb, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "d95-05.45",		0x200000, 0x1a1a852b, TAITO_CHARS_BYTESWAP },     //  7

	{ "d95-07.32",		0x040000, 0x3c201d70, TAITO_68KROM2_BYTESWAP },   //  8 68k Code
	{ "d95-08.33",		0x040000, 0x01c23354, TAITO_68KROM2_BYTESWAP },   //  9

	{ "d95-03.38",		0x200000, 0x4149ea67, TAITO_ES5505_BYTESWAP },    // 10 Ensoniq Samples
	{ "d95-04.41",		0x200000, 0xe9049d16, TAITO_ES5505_BYTESWAP },    // 11
};

STD_ROM_PICK(qtheater)
STD_ROM_FN(qtheater)

static INT32 qtheaterInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, QTHEATER, 1);
}

struct BurnDriver BurnDrvQtheater = {
	"qtheater", NULL, NULL, NULL, "1994",
	"Quiz Theater - 3tsu no Monogatari (Ver 2.3J 1994/11/10)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_QUIZ, 0,
	NULL, qtheaterRomInfo, qtheaterRomName, NULL, NULL, F3InputInfo, NULL,
	qtheaterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Space Invaders '95: The Attack Of Lunar Loonies (Ver 2.5O 1995/06/14)

static struct BurnRomInfo spcinv95RomDesc[] = {
	{ "e06-14.20",		0x020000, 0x71ba7f00, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e06-13.19",		0x020000, 0xf506ba4b, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e06-12.18",		0x020000, 0x06cbd72b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e06-16.17",		0x020000, 0xd1eb3195, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e06-03",		0x100000, 0xa24070ef, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e06-02",		0x100000, 0x8f646dea, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e06-01",		0x100000, 0x51721b15, TAITO_SPRITESA },           //  6

	{ "e06-08",		0x100000, 0x72ae2fbf, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e06-07",		0x100000, 0x4b02e8f5, TAITO_CHARS_BYTESWAP },     //  8
	{ "e06-06",		0x100000, 0x9380db3c, TAITO_CHARS },              //  9

	{ "e06-09.32",		0x040000, 0x9bcafc87, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e06-10.33",		0x040000, 0xb752b61f, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e06-04",		0x200000, 0x1dac29df, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e06-05",		0x200000, 0xf370ff15, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.bin",	0x000104, 0xb371532b, BRF_OPT }, // 14 plds
	{ "pal16l8a-d77-10.bin",	0x000104, 0x42f59227, BRF_OPT }, // 15
	{ "palce16v8q-d77-11.bin",	0x000117, 0xeacc294e, BRF_OPT }, // 16
	{ "palce16v8q-d77-12.bin",	0x000117, 0xe9920cfe, BRF_OPT }, // 17
	{ "palce16v8q-d77-13.bin",	0x000117, 0x66e32e73, BRF_OPT }, // 18
};

STD_ROM_PICK(spcinv95)
STD_ROM_FN(spcinv95)

static INT32 spcinv95Init()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, SPCINV95, 1);
}

struct BurnDriver BurnDrvSpcinv95 = {
	"spcinv95", NULL, NULL, NULL, "1995",
	"Space Invaders '95: The Attack Of Lunar Loonies (Ver 2.5O 1995/06/14)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, spcinv95RomInfo, spcinv95RomName, NULL, NULL, F3InputInfo, NULL,
	spcinv95Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Space Invaders '95: The Attack Of Lunar Loonies (Ver 2.5A 1995/06/14)

static struct BurnRomInfo spcinv95uRomDesc[] = {
	{ "e06-14.20",		0x020000, 0x71ba7f00, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e06-13.19",		0x020000, 0xf506ba4b, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e06-12.18",		0x020000, 0x06cbd72b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e06-15.17",		0x020000, 0xa6ec0103, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e06-03",		0x100000, 0xa24070ef, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e06-02",		0x100000, 0x8f646dea, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e06-01",		0x100000, 0x51721b15, TAITO_SPRITESA },           //  6

	{ "e06-08",		0x100000, 0x72ae2fbf, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e06-07",		0x100000, 0x4b02e8f5, TAITO_CHARS_BYTESWAP },     //  8
	{ "e06-06",		0x100000, 0x9380db3c, TAITO_CHARS },              //  9

	{ "e06-09.32",		0x040000, 0x9bcafc87, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e06-10.33",		0x040000, 0xb752b61f, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e06-04",		0x200000, 0x1dac29df, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e06-05",		0x200000, 0xf370ff15, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.bin",	0x000104, 0xb371532b, BRF_OPT }, // 14 plds
	{ "pal16l8a-d77-10.bin",	0x000104, 0x42f59227, BRF_OPT }, // 15
	{ "palce16v8q-d77-11.bin",	0x000117, 0xeacc294e, BRF_OPT }, // 16
	{ "palce16v8q-d77-12.bin",	0x000117, 0xe9920cfe, BRF_OPT }, // 17
	{ "palce16v8q-d77-13.bin",	0x000117, 0x66e32e73, BRF_OPT }, // 18
};

STD_ROM_PICK(spcinv95u)
STD_ROM_FN(spcinv95u)

struct BurnDriver BurnDrvSpcinv95u = {
	"spcinv95u", "spcinv95", NULL, NULL, "1995",
	"Space Invaders '95: The Attack Of Lunar Loonies (Ver 2.5A 1995/06/14)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, spcinv95uRomInfo, spcinv95uRomName, NULL, NULL, F3InputInfo, NULL,
	spcinv95Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Akkanbeder (Ver 2.5J 1995/06/14)

static struct BurnRomInfo akkanvdrRomDesc[] = {
	{ "e06-14.20",		0x020000, 0x71ba7f00, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e06-13.19",		0x020000, 0xf506ba4b, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e06-12.18",		0x020000, 0x06cbd72b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e06-11.17",		0x020000, 0x3fe550b9, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e06-03",		0x100000, 0xa24070ef, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e06-02",		0x100000, 0x8f646dea, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e06-01",		0x100000, 0x51721b15, TAITO_SPRITESA },           //  6

	{ "e06-08",		0x100000, 0x72ae2fbf, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e06-07",		0x100000, 0x4b02e8f5, TAITO_CHARS_BYTESWAP },     //  8
	{ "e06-06",		0x100000, 0x9380db3c, TAITO_CHARS },              //  9

	{ "e06-09.32",		0x040000, 0x9bcafc87, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e06-10.33",		0x040000, 0xb752b61f, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e06-04",		0x200000, 0x1dac29df, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e06-05",		0x200000, 0xf370ff15, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(akkanvdr)
STD_ROM_FN(akkanvdr)

struct BurnDriver BurnDrvAkkanvdr = {
	"akkanvdr", "spcinv95", NULL, NULL, "1995",
	"Akkanbeder (Ver 2.5J 1995/06/14)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, akkanvdrRomInfo, akkanvdrRomName, NULL, NULL, F3InputInfo, NULL,
	spcinv95Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	224, 320, 3, 4
};


// Elevator Action Returns (Ver 2.2O 1995/02/20)

static struct BurnRomInfo elvactrRomDesc[] = {
	{ "e02-12.20",		0x080000, 0xea5f5a32, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e02-11.19",		0x080000, 0xbcced8ff, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e02-10.18",		0x080000, 0x72f1b952, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e02-16.17",		0x080000, 0xcd97182b, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e02-03.12",		0x200000, 0xc884ebb5, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e02-02.8",		0x200000, 0xc8e06cfb, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e02-01.4",		0x200000, 0x2ba94726, TAITO_SPRITESA },           //  6

	{ "e02-08.47",		0x200000, 0x29c9bd02, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e02-07.45",		0x200000, 0x5eeee925, TAITO_CHARS_BYTESWAP },     //  8
	{ "e02-06.43",		0x200000, 0x4c8726e9, TAITO_CHARS },              //  9

	{ "e02-13.32",		0x040000, 0x80932702, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e02-14.33",		0x040000, 0x706671a5, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e02-04.38",		0x200000, 0xb74307af, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e02-05.39",		0x200000, 0xeb729855, TAITO_ES5505_BYTESWAP },    // 13

	{ "ampal20l10a.a12",	0x0000cc, 0xe719542f, BRF_OPT }, // 14 plds
	{ "pal20l10b.a24",	0x0000cc, 0x00000000, BRF_OPT | BRF_NODUMP }, // 15
	{ "pal16l8b.b24",	0x000104, 0x0b73a7d1, BRF_OPT }, // 16
	{ "pal16l8b.b57",	0x000104, 0x74b4d8be, BRF_OPT }, // 17
	{ "pal16l8b.b58",	0x000104, 0x17e2c9b8, BRF_OPT }, // 18
	{ "pal16l8b.b59",	0x000104, 0xdc0db200, BRF_OPT }, // 19
	{ "pal16l8b.b64",	0x000104, 0x3aed3d98, BRF_OPT }, // 20
};

STD_ROM_PICK(elvactr)
STD_ROM_FN(elvactr)

static INT32 elvactrInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, EACTION2, 2);
}

struct BurnDriver BurnDrvElvactr = {
	"elvactr", NULL, NULL, NULL, "1994",
	"Elevator Action Returns (Ver 2.2O 1995/02/20)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, elvactrRomInfo, elvactrRomName, NULL, NULL, F3InputInfo, NULL,
	elvactrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Elevator Action Returns (Ver 2.2J 1995/02/20)

static struct BurnRomInfo elvactrjRomDesc[] = {
	{ "e02-12.20",		0x080000, 0xea5f5a32, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e02-11.19",		0x080000, 0xbcced8ff, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e02-10.18",		0x080000, 0x72f1b952, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e02-09.17",		0x080000, 0x23997907, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e02-03.12",		0x200000, 0xc884ebb5, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e02-02.8",		0x200000, 0xc8e06cfb, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e02-01.4",		0x200000, 0x2ba94726, TAITO_SPRITESA },           //  6

	{ "e02-08.47",		0x200000, 0x29c9bd02, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e02-07.45",		0x200000, 0x5eeee925, TAITO_CHARS_BYTESWAP },     //  8
	{ "e02-06.43",		0x200000, 0x4c8726e9, TAITO_CHARS },              //  9

	{ "e02-13.32",		0x040000, 0x80932702, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e02-14.33",		0x040000, 0x706671a5, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e02-04.38",		0x200000, 0xb74307af, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e02-05.39",		0x200000, 0xeb729855, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(elvactrj)
STD_ROM_FN(elvactrj)

struct BurnDriver BurnDrvElvactrj = {
	"elvactrj", "elvactr", NULL, NULL, "1994",
	"Elevator Action Returns (Ver 2.2J 1995/02/20)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, elvactrjRomInfo, elvactrjRomName, NULL, NULL, F3InputInfo, NULL,
	elvactrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Elevator Action II (Ver 2.2A 1995/02/20)

static struct BurnRomInfo elvact2uRomDesc[] = {
	{ "e02-12.20",		0x080000, 0xea5f5a32, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e02-11.19",		0x080000, 0xbcced8ff, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e02-10.18",		0x080000, 0x72f1b952, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e02-15.17",		0x080000, 0xba9028bd, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e02-03.12",		0x200000, 0xc884ebb5, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e02-02.8",		0x200000, 0xc8e06cfb, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e02-01.4",		0x200000, 0x2ba94726, TAITO_SPRITESA },           //  6

	{ "e02-08.47",		0x200000, 0x29c9bd02, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e02-07.45",		0x200000, 0x5eeee925, TAITO_CHARS_BYTESWAP },     //  8
	{ "e02-06.43",		0x200000, 0x4c8726e9, TAITO_CHARS },     //  9

	{ "e02-13.32",		0x040000, 0x80932702, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e02-14.33",		0x040000, 0x706671a5, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e02-04.38",		0x200000, 0xb74307af, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e02-05.39",		0x200000, 0xeb729855, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(elvact2u)
STD_ROM_FN(elvact2u)

struct BurnDriver BurnDrvElvact2u = {
	"elvact2u", "elvactr", NULL, NULL, "1994",
	"Elevator Action II (Ver 2.2A 1995/02/20)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, elvact2uRomInfo, elvact2uRomName, NULL, NULL, F3InputInfo, NULL,
	elvactrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Twin Qix (Ver 1.0A 1995/01/17) (Prototype)

static struct BurnRomInfo twinqixRomDesc[] = {
	{ "mpr0-3.b60",		0x40000, 0x1a63d0de, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "mpr0-2.b61",		0x40000, 0x45a70987, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "mpr0-1.b62",		0x40000, 0x531f9447, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "mpr0-0.b63",		0x40000, 0xa4c44c11, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "obj0-0.a08",		0x80000, 0xc6ea845c, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "obj0-1.a20",		0x80000, 0x8c12b7fb, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "scr0-0.b07",		0x80000, 0x9a1b9b34, TAITO_CHARS_BYTESWAP32 },   //  6 Layer Tiles
	{ "scr0-2.b05",		0x80000, 0xcac6854b, TAITO_CHARS_BYTESWAP32 },   //  7
	{ "scr0-1.b06",		0x80000, 0xe9bef879, TAITO_CHARS_BYTESWAP32 },   //  8
	{ "scr0-3.b04",		0x80000, 0xce063034, TAITO_CHARS_BYTESWAP32 },   //  9
	{ "scr0-4.b03",		0x80000, 0xd32280fe, TAITO_CHARS_BYTESWAP },     // 10
	{ "scr0-5.b02",		0x80000, 0xfdd1a85b, TAITO_CHARS_BYTESWAP },     // 11

	{ "spr0-1.b66",		0x40000, 0x4b20e99d, TAITO_68KROM2_BYTESWAP },   // 12 68k Code
	{ "spr0-0.b65",		0x40000, 0x2569eb30, TAITO_68KROM2_BYTESWAP },   // 13

	{ "snd-0.b43",		0x80000, 0xad5405a9, TAITO_ES5505_BYTESWAP },    // 14 Ensoniq Samples
	{ "snd-1.b44",		0x80000, 0x274864af, TAITO_ES5505_BYTESWAP },    // 15
	{ "snd-14.b10",		0x80000, 0x26312451, TAITO_ES5505_BYTESWAP },    // 16
	{ "snd-15.b11",		0x80000, 0x2edaa9dc, TAITO_ES5505_BYTESWAP },    // 17

	{ "pal20l10a.a12",	0x0cc, 0x00000000, 0 },       		   // 18 plds
	{ "pal20l10a.a24",	0x0cc, 0x00000000, 0 },       		   // 19
	{ "pal16l8b.b24",	0x104, 0x0b73a7d1, 0 },                    // 20
	{ "pal16l8b.b57",	0x104, 0x74b4d8be, 0 },                    // 21
	{ "pal16l8b.b58",	0x104, 0x17e2c9b8, 0 },                    // 22
	{ "pal16l8b.b59",	0x104, 0xdc0db200, 0 },                    // 23
	{ "pal16l8b.b64",	0x104, 0x3aed3d98, 0 },                    // 24
};

STD_ROM_PICK(twinqix)
STD_ROM_FN(twinqix)

/*
static INT32 twinqixRomCallback()
{
	if (BurnLoadRom(Taito68KRom1	+ 0x000001,  0, 4)) return 1;
	if (BurnLoadRom(Taito68KRom1	+ 0x000000,  1, 4)) return 1;
	if (BurnLoadRom(Taito68KRom1	+ 0x000003,  2, 4)) return 1;
	if (BurnLoadRom(Taito68KRom1	+ 0x000002,  3, 4)) return 1;

	if (BurnLoadRom(TaitoSpritesA   + 0x000000,  4, 2)) return 1;
	if (BurnLoadRom(TaitoSpritesA   + 0x000001,  5, 2)) return 1;

	if (BurnLoadRom(TaitoChars      + 0x000000,  6, 4)) return 1;
	if (BurnLoadRom(TaitoChars      + 0x000002,  7, 4)) return 1;
	if (BurnLoadRom(TaitoChars      + 0x000001,  8, 4)) return 1;
	if (BurnLoadRom(TaitoChars      + 0x000003,  9, 4)) return 1;
	if (BurnLoadRom(TaitoChars      + 0x300000, 10, 2)) return 1;
	if (BurnLoadRom(TaitoChars      + 0x300001, 11, 2)) return 1;

	if (BurnLoadRom(Taito68KRom2	+ 0x000001, 12, 2)) return 1;
	if (BurnLoadRom(Taito68KRom2	+ 0x000000, 13, 2)) return 1;

	if (BurnLoadRom(TaitoES5505Rom	+ 0x000001, 14, 2)) return 1;
	if (BurnLoadRom(TaitoES5505Rom	+ 0x100001, 15, 2)) return 1;
	if (BurnLoadRom(TaitoES5505Rom	+ 0x200001, 16, 2)) return 1;
	if (BurnLoadRom(TaitoES5505Rom	+ 0x300001, 17, 2)) return 1;

	tile_decode(0x200000, 0x400000);

	return 0;
}
*/

static INT32 twinqixInit()
{
	return DrvInit(NULL/*twinqixRomCallback*/, f3_21bit_typeB_palette_update, 1, TWINQIX, 1);
}

struct BurnDriver BurnDrvTwinqix = {
	"twinqix", NULL, NULL, NULL, "1995",
	"Twin Qix (Ver 1.0A 1995/01/17) (Prototype)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, twinqixRomInfo, twinqixRomName, NULL, NULL, F3InputInfo, NULL,
	twinqixInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Moriguchi Hiroko no Quiz de Hyuu!Hyuu! (Ver 2.2J 1995/05/25)

static struct BurnRomInfo quizhuhuRomDesc[] = {
	{ "e08-16.20",		0x080000, 0xfaa8f373, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e08-15.19",		0x080000, 0x23acf231, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e08-14.18",		0x080000, 0x33a4951d, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e08-13.17",		0x080000, 0x0936fd2a, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e08-06.12",		0x200000, 0x8dadc9ac, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e08-04.8",		0x200000, 0x5423721d, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e08-05.11",		0x100000, 0x79d2e516, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "e08-03.7",		0x100000, 0x07b9ab6a, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "e08-02.4",		0x200000, 0xd89eb067, TAITO_SPRITESA },           //  8
	{ "e08-01.3",		0x100000, 0x90223c06, TAITO_SPRITESA },           //  9

	{ "e08-12.47",		0x100000, 0x6c711d36, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "e08-11.45",		0x100000, 0x56775a60, TAITO_CHARS_BYTESWAP },     // 11
	{ "e08-10.43",		0x100000, 0x60abc71b, TAITO_CHARS },              // 12

	{ "e08-18.32",		0x020000, 0xe695497e, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "e08-17.33",		0x020000, 0xfafc7e4e, TAITO_68KROM2_BYTESWAP },   // 14

	{ "e08-07.38",		0x200000, 0xc05dc85b, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "e08-08.39",		0x200000, 0x3eb94a99, TAITO_ES5505_BYTESWAP },    // 16
	{ "e08-09.41",		0x200000, 0x200b26ee, TAITO_ES5505_BYTESWAP },    // 17
};

STD_ROM_PICK(quizhuhu)
STD_ROM_FN(quizhuhu)

static INT32 quizhuhuInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, QUIZHUHU, 1);
}

struct BurnDriver BurnDrvQuizhuhu = {
	"quizhuhu", NULL, NULL, NULL, "1995",
	"Moriguchi Hiroko no Quiz de Hyuu!Hyuu! (Ver 2.2J 1995/05/25)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_QUIZ, 0,
	NULL, quizhuhuRomInfo, quizhuhuRomName, NULL, NULL, F3InputInfo, NULL,
	quizhuhuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 2 (Ver 2.3O 1995/07/31)

static struct BurnRomInfo pbobble2RomDesc[] = {
	{ "e10-24.bin",		0x040000, 0xf9d0794b, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e10-23.bin",		0x040000, 0x56a66435, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e10-22.bin",		0x040000, 0x7b12105d, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e10-25.bin",		0x040000, 0xff0407d3, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e10-02.rom",		0x100000, 0xc0564490, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e10-01.rom",		0x100000, 0x8c26ff49, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e10-07.rom",		0x100000, 0xdcb3c29b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e10-06.rom",		0x100000, 0x1b0f20e2, TAITO_CHARS_BYTESWAP },     //  7
	{ "e10-05.rom",		0x100000, 0x81266151, TAITO_CHARS },              //  8

	{ "e10-12.32",		0x040000, 0xb92dc8ad, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e10-13.33",		0x040000, 0x87842c13, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e10-04.rom",		0x200000, 0x5c0862a6, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e10-03.rom",		0x200000, 0x46d68ac8, TAITO_ES5505_BYTESWAP },    // 12

	{ "e10-21.bin",		0x000117, 0x458499b7, BRF_OPT }, // 13 extra
};

STD_ROM_PICK(pbobble2)
STD_ROM_FN(pbobble2)

static INT32 pbobble2Init()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, PBOBBLE2, 1);
}

static INT32 pbobble23OCallback()
{
	UINT32 *ROM = (UINT32 *)Taito68KRom1;

	ROM[0x40090/4] = 0x4e71815c;
	ROM[0x40094/4] = 0x4e714e71;

	return 0;
}

static INT32 pbobble23OInit()
{
	return DrvInit(pbobble23OCallback, f3_24bit_palette_update, 0, PBOBBLE2, 1);
}

struct BurnDriver BurnDrvPbobble2 = {
	"pbobble2", NULL, NULL, NULL, "1995",
	"Puzzle Bobble 2 (Ver 2.3O 1995/07/31)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble2RomInfo, pbobble2RomName, NULL, NULL, F3InputInfo, NULL,
	pbobble23OInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 2 (Ver 2.2O 1995/07/20)

static struct BurnRomInfo pbobble2oRomDesc[] = {
	{ "e10-11.20",		0x040000, 0xb82f81da, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e10-10.19",		0x040000, 0xf432267a, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e10-09.18",		0x040000, 0xe0b1b599, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e10-15.17",		0x040000, 0xa2c0a268, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e10-02.rom",		0x100000, 0xc0564490, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e10-01.rom",		0x100000, 0x8c26ff49, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e10-07.rom",		0x100000, 0xdcb3c29b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e10-06.rom",		0x100000, 0x1b0f20e2, TAITO_CHARS_BYTESWAP },     //  7
	{ "e10-05.rom",		0x100000, 0x81266151, TAITO_CHARS },              //  8

	{ "e10-12.32",		0x040000, 0xb92dc8ad, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e10-13.33",		0x040000, 0x87842c13, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e10-04.rom",		0x200000, 0x5c0862a6, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e10-03.rom",		0x200000, 0x46d68ac8, TAITO_ES5505_BYTESWAP },    // 12
};

STD_ROM_PICK(pbobble2o)
STD_ROM_FN(pbobble2o)

struct BurnDriver BurnDrvPbobble2o = {
	"pbobble2o", "pbobble2", NULL, NULL, "1995",
	"Puzzle Bobble 2 (Ver 2.2O 1995/07/20)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble2oRomInfo, pbobble2oRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 2 (Ver 2.2J 1995/07/20)

static struct BurnRomInfo pbobble2jRomDesc[] = {
	{ "e10-11.20",		0x040000, 0xb82f81da, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e10-10.19",		0x040000, 0xf432267a, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e10-09.18",		0x040000, 0xe0b1b599, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e10-08.17",		0x040000, 0x4ccec344, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e10-02.rom",		0x100000, 0xc0564490, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e10-01.rom",		0x100000, 0x8c26ff49, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e10-07.rom",		0x100000, 0xdcb3c29b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e10-06.rom",		0x100000, 0x1b0f20e2, TAITO_CHARS_BYTESWAP },     //  7
	{ "e10-05.rom",		0x100000, 0x81266151, TAITO_CHARS },              //  8

	{ "e10-12.32",		0x040000, 0xb92dc8ad, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e10-13.33",		0x040000, 0x87842c13, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e10-04.rom",		0x200000, 0x5c0862a6, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e10-03.rom",		0x200000, 0x46d68ac8, TAITO_ES5505_BYTESWAP },    // 12
};

STD_ROM_PICK(pbobble2j)
STD_ROM_FN(pbobble2j)

struct BurnDriver BurnDrvPbobble2j = {
	"pbobble2j", "pbobble2", NULL, NULL, "1995",
	"Puzzle Bobble 2 (Ver 2.2J 1995/07/20)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble2jRomInfo, pbobble2jRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Bust-A-Move Again (Ver 2.3A 1995/07/31)

static struct BurnRomInfo pbobble2uRomDesc[] = {
	{ "e10-20.20",		0x040000, 0x97eb15c6, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e10-19.19",		0x040000, 0x7082d796, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e10-18.18",		0x040000, 0x2ffa3ef2, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e10-14.17",		0x040000, 0x4a19ed67, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e10-02.rom",		0x100000, 0xc0564490, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e10-01.rom",		0x100000, 0x8c26ff49, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e10-07.rom",		0x100000, 0xdcb3c29b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e10-06.rom",		0x100000, 0x1b0f20e2, TAITO_CHARS_BYTESWAP },     //  7
	{ "e10-05.rom",		0x100000, 0x81266151, TAITO_CHARS },              //  8

	{ "e10-16.32",		0x040000, 0x765ce77a, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e10-17.33",		0x040000, 0x0aec3b1e, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e10-04.rom",		0x200000, 0x5c0862a6, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e10-03.rom",		0x200000, 0x46d68ac8, TAITO_ES5505_BYTESWAP },    // 12
};

STD_ROM_PICK(pbobble2u)
STD_ROM_FN(pbobble2u)

struct BurnDriver BurnDrvPbobble2u = {
	"pbobble2u", "pbobble2", NULL, NULL, "1995",
	"Bust-A-Move Again (Ver 2.3A 1995/07/31)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble2uRomInfo, pbobble2uRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 2X (Ver 2.2J 1995/11/11)

static struct BurnRomInfo pbobble2xRomDesc[] = {
	{ "e10-29.20",		0x040000, 0xf1e9ad3f, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e10-28.19",		0x040000, 0x412a3602, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e10-27.18",		0x040000, 0x88cc0b5c, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e10-26.17",		0x040000, 0xa5c24047, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e10-02.rom",		0x100000, 0xc0564490, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e10-01.rom",		0x100000, 0x8c26ff49, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e10-07.rom",		0x100000, 0xdcb3c29b, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e10-06.rom",		0x100000, 0x1b0f20e2, TAITO_CHARS_BYTESWAP },     //  7
	{ "e10-05.rom",		0x100000, 0x81266151, TAITO_CHARS },              //  8

	{ "e10-30.32",		0x040000, 0xbb090c1e, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e10-31.33",		0x040000, 0xf4b88d65, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e10-04.rom",		0x200000, 0x5c0862a6, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e10-03.rom",		0x200000, 0x46d68ac8, TAITO_ES5505_BYTESWAP },    // 12

	{ "pal16l8a-d77-09.bin",	0x000104, 0xb371532b, BRF_OPT }, // 13 plds
	{ "pal16l8a-d77-10.bin",	0x000104, 0x42f59227, BRF_OPT }, // 14
	{ "palce16v8q-d77-11.bin",	0x000117, 0xeacc294e, BRF_OPT }, // 15
	{ "palce16v8q-d77-12.bin",	0x000117, 0xe9920cfe, BRF_OPT }, // 16
	{ "palce16v8q-d77-14.bin",	0x000117, 0x7427e777, BRF_OPT }, // 17
};

STD_ROM_PICK(pbobble2x)
STD_ROM_FN(pbobble2x)

struct BurnDriver BurnDrvPbobble2x = {
	"pbobble2x", "pbobble2", NULL, NULL, "1995",
	"Puzzle Bobble 2X (Ver 2.2J 1995/11/11)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble2xRomInfo, pbobble2xRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Gekirindan (Ver 2.3O 1995/09/21)

static struct BurnRomInfo gekiridnRomDesc[] = {
	{ "e11-12.ic20",	0x040000, 0x6a7aaacf, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e11-11.ic19",	0x040000, 0x2284a08e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e11-10.ic18",	0x040000, 0x8795e6ba, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e11-15.ic17",	0x040000, 0x5aef1fd8, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e11-03.ic12",	0x200000, 0xf73877c5, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e11-02.ic8",		0x200000, 0x5722a83b, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e11-01.ic4",		0x200000, 0xc2cd1069, TAITO_SPRITESA },           //  6

	{ "e11-08.ic47",	0x200000, 0x907f69d3, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e11-07.ic45",	0x200000, 0xef018607, TAITO_CHARS_BYTESWAP },     //  8
	{ "e11-06.ic43",	0x200000, 0x200ce305, TAITO_CHARS },              //  9

	{ "e11-13.ic32",	0x040000, 0xf5c5486a, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e11-14.ic33",	0x040000, 0x7fa10f96, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e11-04.ic38",	0x200000, 0xe0ff4fb1, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e11-05.ic41",	0x200000, 0xa4d08cf1, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(gekiridn)
STD_ROM_FN(gekiridn)

static INT32 gekiridnInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, GEKIRIDO, 1);
}

struct BurnDriver BurnDrvGekiridn = {
	"gekiridn", NULL, NULL, NULL, "1995",
	"Gekirindan (Ver 2.3O 1995/09/21)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gekiridnRomInfo, gekiridnRomName, NULL, NULL, F3InputInfo, NULL,
	gekiridnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	232, 320, 3, 4
};


// Gekirindan (Ver 2.3J 1995/09/21)

static struct BurnRomInfo gekiridnjRomDesc[] = {
	{ "e11-12.ic20",	0x040000, 0x6a7aaacf, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e11-11.ic19",	0x040000, 0x2284a08e, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e11-10.ic18",	0x040000, 0x8795e6ba, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e11-09.ic17",	0x040000, 0xb4e17ef4, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e11-03.ic12",	0x200000, 0xf73877c5, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e11-02.ic8",		0x200000, 0x5722a83b, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e11-01.ic4",		0x200000, 0xc2cd1069, TAITO_SPRITESA },           //  6

	{ "e11-08.ic47",	0x200000, 0x907f69d3, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e11-07.ic45",	0x200000, 0xef018607, TAITO_CHARS_BYTESWAP },     //  8
	{ "e11-06.ic43",	0x200000, 0x200ce305, TAITO_CHARS },              //  9

	{ "e11-13.ic32",	0x040000, 0xf5c5486a, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e11-14.ic33",	0x040000, 0x7fa10f96, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e11-04.ic38",	0x200000, 0xe0ff4fb1, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e11-05.ic41",	0x200000, 0xa4d08cf1, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(gekiridnj)
STD_ROM_FN(gekiridnj)

struct BurnDriver BurnDrvGekiridnj = {
	"gekiridnj", "gekiridn", NULL, NULL, "1995",
	"Gekirindan (Ver 2.3J 1995/09/21)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gekiridnjRomInfo, gekiridnjRomName, NULL, NULL, F3InputInfo, NULL,
	gekiridnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	232, 320, 3, 4
};


// Twin Cobra II (Ver 2.1O 1995/11/30)

static struct BurnRomInfo tcobra2RomDesc[] = {
	{ "e15-14.ic20",	0x040000, 0xb527b733, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e15-13.ic19",	0x040000, 0x0f03daf7, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e15-12.ic18",	0x040000, 0x59d832f2, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e15-18.ic17",	0x040000, 0x4908c3aa, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e15-04.ic12",	0x200000, 0x6ea8d9bd, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e15-02.ic8",		0x200000, 0xbf1232aa, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e15-03.ic11",	0x100000, 0xbe45a52f, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "e15-01.ic7",		0x100000, 0x85421aac, TAITO_SPRITESA_BYTESWAP },  //  7

	{ "e15-10.ic47",	0x200000, 0xd8c96b00, TAITO_CHARS_BYTESWAP },     //  8 Layer Tiles
	{ "e15-08.ic45",	0x200000, 0x4bdb2bf3, TAITO_CHARS_BYTESWAP },     //  9
	{ "e15-09.ic46",	0x100000, 0x07c29f60, TAITO_CHARS_BYTESWAP },     // 10
	{ "e15-07.ic44",	0x100000, 0x8164f7ee, TAITO_CHARS_BYTESWAP },     // 11

	{ "e15-15.ic32",	0x020000, 0x22126dfb, TAITO_68KROM2_BYTESWAP },   // 12 68k Code
	{ "e15-16.ic33",	0x020000, 0xf8b58ea0, TAITO_68KROM2_BYTESWAP },   // 13

	{ "e15-05.ic38",	0x200000, 0x3e5da5f6, TAITO_ES5505_BYTESWAP },    // 14 Ensoniq Samples
	{ "e15-06.ic41",	0x200000, 0xb182a3e1, TAITO_ES5505_BYTESWAP },    // 15
	
	{ "d77-12.ic48.bin", 0x117, 0x6f93a4d8, BRF_OPT },
	{ "d77-14.ic21.bin", 0x117, 0xf2264f51, BRF_OPT },
	{ "palce16v8.ic37.bin", 0x117, 0x6ccd8168, BRF_OPT },
	{ "d77-09.ic14.bin", 0x001, 0x00000000, BRF_NODUMP },
	{ "d77-10.ic28.bin", 0x001, 0x00000000, BRF_NODUMP },
};

STD_ROM_PICK(tcobra2)
STD_ROM_FN(tcobra2)

static INT32 tcobra2Init()
{
	INT32 rc = DrvInit(NULL, f3_24bit_palette_update, 0, KTIGER2, 0);

	if (!rc) {
		ES550X_twincobra2_pan_fix = 1;
	}

	return rc;
}

struct BurnDriver BurnDrvTcobra2 = {
	"tcobra2", NULL, NULL, NULL, "1995",
	"Twin Cobra II (Ver 2.1O 1995/11/30)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, tcobra2RomInfo, tcobra2RomName, NULL, NULL, F3InputInfo, NULL,
	tcobra2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	232, 320, 3, 4
};


// Twin Cobra II (Ver 2.1A 1995/11/30)

static struct BurnRomInfo tcobra2uRomDesc[] = {
	{ "e15-14.ic20",	0x040000, 0xb527b733, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e15-13.ic19",	0x040000, 0x0f03daf7, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e15-12.ic18",	0x040000, 0x59d832f2, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e15-17.ic17",	0x040000, 0x3e0ff33c, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e15-04.ic12",	0x200000, 0x6ea8d9bd, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e15-02.ic8",		0x200000, 0xbf1232aa, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e15-03.ic11",	0x100000, 0xbe45a52f, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "e15-01.ic7",		0x100000, 0x85421aac, TAITO_SPRITESA_BYTESWAP },  //  7

	{ "e15-10.ic47",	0x200000, 0xd8c96b00, TAITO_CHARS_BYTESWAP },     //  8 Layer Tiles
	{ "e15-08.ic45",	0x200000, 0x4bdb2bf3, TAITO_CHARS_BYTESWAP },     //  9
	{ "e15-09.ic46",	0x100000, 0x07c29f60, TAITO_CHARS_BYTESWAP },     // 10
	{ "e15-07.ic44",	0x100000, 0x8164f7ee, TAITO_CHARS_BYTESWAP },     // 11

	{ "e15-15.ic32",	0x020000, 0x22126dfb, TAITO_68KROM2_BYTESWAP },   // 12 68k Code
	{ "e15-16.ic33",	0x020000, 0xf8b58ea0, TAITO_68KROM2_BYTESWAP },   // 13

	{ "e15-05.ic38",	0x200000, 0x3e5da5f6, TAITO_ES5505_BYTESWAP },    // 14 Ensoniq Samples
	{ "e15-06.ic41",	0x200000, 0xb182a3e1, TAITO_ES5505_BYTESWAP },    // 15
	
	{ "d77-12.ic48.bin", 0x117, 0x6f93a4d8, BRF_OPT },
	{ "d77-14.ic21.bin", 0x117, 0xf2264f51, BRF_OPT },
	{ "palce16v8.ic37.bin", 0x117, 0x6ccd8168, BRF_OPT },
	{ "d77-09.ic14.bin", 0x001, 0x00000000, BRF_NODUMP },
	{ "d77-10.ic28.bin", 0x001, 0x00000000, BRF_NODUMP },
};

STD_ROM_PICK(tcobra2u)
STD_ROM_FN(tcobra2u)

struct BurnDriver BurnDrvTcobra2u = {
	"tcobra2u", "tcobra2", NULL, NULL, "1995",
	"Twin Cobra II (Ver 2.1A 1995/11/30)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, tcobra2uRomInfo, tcobra2uRomName, NULL, NULL, F3InputInfo, NULL,
	tcobra2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	232, 320, 3, 4
};


// Kyukyoku Tiger II (Ver 2.1J 1995/11/30)

static struct BurnRomInfo ktiger2RomDesc[] = {
	{ "e15-14.ic20",	0x040000, 0xb527b733, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e15-13.ic19",	0x040000, 0x0f03daf7, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e15-12.ic18",	0x040000, 0x59d832f2, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e15-11.ic17",	0x040000, 0xa706a286, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e15-04.ic12",	0x200000, 0x6ea8d9bd, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e15-02.ic8",		0x200000, 0xbf1232aa, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e15-03.ic11",	0x100000, 0xbe45a52f, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "e15-01.ic7",		0x100000, 0x85421aac, TAITO_SPRITESA_BYTESWAP },  //  7

	{ "e15-10.ic47",	0x200000, 0xd8c96b00, TAITO_CHARS_BYTESWAP },     //  8 Layer Tiles
	{ "e15-08.ic45",	0x200000, 0x4bdb2bf3, TAITO_CHARS_BYTESWAP },     //  9
	{ "e15-09.ic46",	0x100000, 0x07c29f60, TAITO_CHARS_BYTESWAP },     // 10
	{ "e15-07.ic44",	0x100000, 0x8164f7ee, TAITO_CHARS_BYTESWAP },     // 11

	{ "e15-15.ic32",	0x020000, 0x22126dfb, TAITO_68KROM2_BYTESWAP },   // 12 68k Code
	{ "e15-16.ic33",	0x020000, 0xf8b58ea0, TAITO_68KROM2_BYTESWAP },   // 13

	{ "e15-05.ic38",	0x200000, 0x3e5da5f6, TAITO_ES5505_BYTESWAP },    // 14 Ensoniq Samples
	{ "e15-06.ic41",	0x200000, 0xb182a3e1, TAITO_ES5505_BYTESWAP },    // 15
	
	{ "d77-12.ic48.bin", 0x117, 0x6f93a4d8, BRF_OPT },
	{ "d77-14.ic21.bin", 0x117, 0xf2264f51, BRF_OPT },
	{ "palce16v8.ic37.bin", 0x117, 0x6ccd8168, BRF_OPT },
	{ "d77-09.ic14.bin", 0x001, 0x00000000, BRF_NODUMP },
	{ "d77-10.ic28.bin", 0x001, 0x00000000, BRF_NODUMP },
};

STD_ROM_PICK(ktiger2)
STD_ROM_FN(ktiger2)

struct BurnDriver BurnDrvKtiger2 = {
	"ktiger2", "tcobra2", NULL, NULL, "1995",
	"Kyukyoku Tiger II (Ver 2.1J 1995/11/30)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, ktiger2RomInfo, ktiger2RomName, NULL, NULL, F3InputInfo, NULL,
	tcobra2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	232, 320, 3, 4
};


// Bubble Memories: The Story Of Bubble Bobble III (Ver 2.4O 1996/02/15)

static struct BurnRomInfo bubblemRomDesc[] = {
	{ "e21-21.20",		0x080000, 0xcac4169c, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e21-20.19",		0x080000, 0x7727c673, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e21-19.18",		0x080000, 0xbe0b907d, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e21-18.17",		0x080000, 0xd14e313a, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e21-02.rom",		0x200000, 0xb7cb9232, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e21-01.rom",		0x200000, 0xa11f2f99, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e21-07.rom",		0x100000, 0x7789bf7c, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e21-06.rom",		0x100000, 0x997fc0d7, TAITO_CHARS_BYTESWAP },     //  7
	{ "e21-05.rom",		0x100000, 0x07eab58f, TAITO_CHARS },              //  8

	{ "e21-12.32",		0x040000, 0x34093de1, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e21-13.33",		0x040000, 0x9e9ec437, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e21-03.rom",		0x200000, 0x54c5f83d, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e21-04.rom",		0x200000, 0xe5af2a2d, TAITO_ES5505_BYTESWAP },    // 12

	{ "bubblem.nv",		0x000080, 0x9a59326e, TAITO_DEFAULT_EEPROM },     // 13 eeprom
};

STD_ROM_PICK(bubblem)
STD_ROM_FN(bubblem)

static INT32 bubblemInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, BUBBLEM, 1);
}

struct BurnDriver BurnDrvBubblem = {
	"bubblem", NULL, NULL, NULL, "1995",
	"Bubble Memories: The Story Of Bubble Bobble III (Ver 2.4O 1996/02/15)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubblemRomInfo, bubblemRomName, NULL, NULL, F3InputInfo, NULL,
	bubblemInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Bubble Memories: The Story Of Bubble Bobble III (Ver 2.3J 1996/02/07)

static struct BurnRomInfo bubblemjRomDesc[] = {
	{ "e21-11.20",		0x080000, 0xdf0eeae4, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e21-10.19",		0x080000, 0xcdfb58f6, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e21-09.18",		0x080000, 0x6c305f17, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e21-08.17",		0x080000, 0x27381ae2, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e21-02.rom",		0x200000, 0xb7cb9232, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e21-01.rom",		0x200000, 0xa11f2f99, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e21-07.rom",		0x100000, 0x7789bf7c, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e21-06.rom",		0x100000, 0x997fc0d7, TAITO_CHARS_BYTESWAP },     //  7
	{ "e21-05.rom",		0x100000, 0x07eab58f, TAITO_CHARS },              //  8

	{ "e21-12.32",		0x040000, 0x34093de1, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e21-13.33",		0x040000, 0x9e9ec437, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e21-03.rom",		0x200000, 0x54c5f83d, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e21-04.rom",		0x200000, 0xe5af2a2d, TAITO_ES5505_BYTESWAP },    // 12

	{ "bubblemj.nv",	0x000080, 0xcb4ef35c, TAITO_DEFAULT_EEPROM },     // 13 eeprom
};

STD_ROM_PICK(bubblemj)
STD_ROM_FN(bubblemj)

struct BurnDriver BurnDrvBubblemj = {
	"bubblemj", "bubblem", NULL, NULL, "1995",
	"Bubble Memories: The Story Of Bubble Bobble III (Ver 2.3J 1996/02/07)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubblemjRomInfo, bubblemjRomName, NULL, NULL, F3InputInfo, NULL,
	bubblemInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Cleopatra Fortune (Ver 2.1J 1996/09/05)

static struct BurnRomInfo cleopatrRomDesc[] = {
	{ "e28-10.bin",		0x080000, 0x013fbc39, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e28-09.bin",		0x080000, 0x1c48a1f9, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e28-08.bin",		0x080000, 0x7564f199, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e28-07.bin",		0x080000, 0xa507797b, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e28-02.bin",		0x080000, 0xb20d47cb, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e28-01.bin",		0x080000, 0x4440e659, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e28-06.bin",		0x100000, 0x21d0c454, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e28-05.bin",		0x100000, 0x2870dbbc, TAITO_CHARS_BYTESWAP },     //  7
	{ "e28-04.bin",		0x100000, 0x57aef029, TAITO_CHARS },              //  8

	{ "e28-11.bin",		0x020000, 0x01a06950, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e28-12.bin",		0x020000, 0xdc19260f, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e28-03.bin",		0x200000, 0x15c7989d, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
};

STD_ROM_PICK(cleopatr)
STD_ROM_FN(cleopatr)

static INT32 cleopatrInit()
{
	return DrvInit(NULL, f3_21bit_typeA_palette_update, 0, CLEOPATR, 1);
}

struct BurnDriver BurnDrvCleopatr = {
	"cleopatr", NULL, NULL, NULL, "1996",
	"Cleopatra Fortune (Ver 2.1J 1996/09/05)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, cleopatrRomInfo, cleopatrRomName, NULL, NULL, F3InputInfo, NULL,
	cleopatrInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Puzzle Bobble 3 (Ver 2.1O 1996/09/27)

static struct BurnRomInfo pbobble3RomDesc[] = {
	{ "e29-12.rom",		0x080000, 0x9eb19a00, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e29-11.rom",		0x080000, 0xe54ada97, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e29-10.rom",		0x080000, 0x1502a122, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e29-16.rom",		0x080000, 0xaac293da, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e29-02.rom",		0x100000, 0x437391d3, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e29-01.rom",		0x100000, 0x52547c77, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e29-08.rom",		0x100000, 0x7040a3d5, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e29-07.rom",		0x100000, 0xfca2ea9b, TAITO_CHARS_BYTESWAP },     //  7
	{ "e29-06.rom",		0x100000, 0xc16184f8, TAITO_CHARS },              //  8

	{ "e29-13.rom",		0x040000, 0x1ef551ef, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e29-14.rom",		0x040000, 0x7ee7e688, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e29-03.rom",		0x200000, 0xa4371658, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e29-04.rom",		0x200000, 0xd1f42457, TAITO_ES5505_BYTESWAP },    // 12
	{ "e29-05.rom",		0x200000, 0xe33c1234, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(pbobble3)
STD_ROM_FN(pbobble3)

static INT32 pbobble3Init()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, PBOBBLE3, 1);
}

struct BurnDriver BurnDrvPbobble3 = {
	"pbobble3", NULL, NULL, NULL, "1996",
	"Puzzle Bobble 3 (Ver 2.1O 1996/09/27)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble3RomInfo, pbobble3RomName, NULL, NULL, F3InputInfo, NULL,
	pbobble3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 3 (Ver 2.1A 1996/09/27)

static struct BurnRomInfo pbobble3uRomDesc[] = {
	{ "e29-12.rom",		0x080000, 0x9eb19a00, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e29-11.rom",		0x080000, 0xe54ada97, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e29-10.rom",		0x080000, 0x1502a122, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e29-15.bin",		0x080000, 0xddc5a34c, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e29-02.rom",		0x100000, 0x437391d3, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e29-01.rom",		0x100000, 0x52547c77, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e29-08.rom",		0x100000, 0x7040a3d5, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e29-07.rom",		0x100000, 0xfca2ea9b, TAITO_CHARS_BYTESWAP },     //  7
	{ "e29-06.rom",		0x100000, 0xc16184f8, TAITO_CHARS },              //  8

	{ "e29-13.rom",		0x040000, 0x1ef551ef, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e29-14.rom",		0x040000, 0x7ee7e688, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e29-03.rom",		0x200000, 0xa4371658, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e29-04.rom",		0x200000, 0xd1f42457, TAITO_ES5505_BYTESWAP },    // 12
	{ "e29-05.rom",		0x200000, 0xe33c1234, TAITO_ES5505_BYTESWAP },    // 13
};

STD_ROM_PICK(pbobble3u)
STD_ROM_FN(pbobble3u)

struct BurnDriver BurnDrvPbobble3u = {
	"pbobble3u", "pbobble3", NULL, NULL, "1996",
	"Puzzle Bobble 3 (Ver 2.1A 1996/09/27)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble3uRomInfo, pbobble3uRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 3 (Ver 2.1J 1996/09/27)

static struct BurnRomInfo pbobble3jRomDesc[] = {
	{ "e29-12.ic20",	0x080000, 0x9eb19a00, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e29-11.ic19",	0x080000, 0xe54ada97, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e29-10.ic18",	0x080000, 0x1502a122, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e29-09.ic17",	0x080000, 0x44ccf2f6, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e29-02.ic8",		0x100000, 0x437391d3, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e29-01.ic12",	0x100000, 0x52547c77, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e29-08.ic47",	0x100000, 0x7040a3d5, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e29-07.ic45",	0x100000, 0xfca2ea9b, TAITO_CHARS_BYTESWAP },     //  7
	{ "e29-06.ic43",	0x100000, 0xc16184f8, TAITO_CHARS },              //  8

	{ "e29-13.ic32",	0x040000, 0x1ef551ef, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e29-14.ic33",	0x040000, 0x7ee7e688, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e29-03.ic38",	0x200000, 0xa4371658, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e29-04.ic39",	0x200000, 0xd1f42457, TAITO_ES5505_BYTESWAP },    // 12
	{ "e29-05.ic41",	0x200000, 0xe33c1234, TAITO_ES5505_BYTESWAP },    // 13
	
	{ "d77-12.ic48.bin", 0x000001, 0x00000000, BRF_OPT },
	{ "d77-14.ic21.bin", 0x000001, 0x00000000, BRF_OPT },
	{ "d77-11.ic37.bin", 0x000001, 0x00000000, BRF_OPT },
	{ "d77-09.ic14.bin", 0x000001, 0x00000000, BRF_OPT },
	{ "d77-10.ic28.bin", 0x000001, 0x00000000, BRF_OPT },
};

STD_ROM_PICK(pbobble3j)
STD_ROM_FN(pbobble3j)

struct BurnDriver BurnDrvPbobble3j = {
	"pbobble3j", "pbobble3", NULL, NULL, "1996",
	"Puzzle Bobble 3 (Ver 2.1J 1996/09/27)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble3jRomInfo, pbobble3jRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Arkanoid Returns (Ver 2.02O 1997/02/10)

static struct BurnRomInfo arkretrnRomDesc[] = {
	{ "e36-11.20",		0x040000, 0xb50cfb92, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e36-10.19",		0x040000, 0xc940dba1, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e36-09.18",		0x040000, 0xf16985e0, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e36-15.17",		0x040000, 0x4467ff37, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e36-03.12",		0x040000, 0x1ea8558b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e36-02.8",		0x040000, 0x694eda31, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e36-01.4",		0x040000, 0x54b9b2cd, TAITO_SPRITESA },           //  6

	{ "e36-07.47",		0x080000, 0x266bf1c1, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e36-06.45",		0x080000, 0x110ab729, TAITO_CHARS_BYTESWAP },     //  8
	{ "e36-05.43",		0x080000, 0xdb18bce2, TAITO_CHARS },              //  9

	{ "e36-12.32",		0x040000, 0x3bae39be, TAITO_68KROM2_BYTESWAP},    // 10 68k Code
	{ "e36-13.33",		0x040000, 0x94448e82, TAITO_68KROM2_BYTESWAP},    // 11

	{ "e36-04.38",		0x200000, 0x2250959b, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples

	{ "pal16l8a-d77-09.bin",	0x000104, 0xb371532b, 0 },           // 13 plds
	{ "pal16l8a-d77-10.bin",	0x000104, 0x42f59227, 0 },           // 14
	{ "palce16v8-d77-11.bin",	0x000117, 0xeacc294e, 0 },           // 15
	{ "palce16v8-d77-12.bin",	0x000117, 0xe9920cfe, 0 },           // 16
	{ "palce16v8-d77-14.bin",	0x000117, 0x7427e777, 0 },           // 17
};

STD_ROM_PICK(arkretrn)
STD_ROM_FN(arkretrn)

static INT32 arkretrnInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, ARKRETRN, 1);
}

struct BurnDriver BurnDrvArkretrn = {
	"arkretrn", NULL, NULL, NULL, "1997",
	"Arkanoid Returns (Ver 2.02O 1997/02/10)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arkretrnRomInfo, arkretrnRomName, NULL, NULL, F3InputInfo, NULL,
	arkretrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Arkanoid Returns (Ver 2.02A 1997/02/10)

static struct BurnRomInfo arkretrnuRomDesc[] = {
	{ "e36-11.20",		0x040000, 0xb50cfb92, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e36-10.19",		0x040000, 0xc940dba1, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e36-09.18",		0x040000, 0xf16985e0, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e36-14.17",		0x040000, 0x3360cfa1, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e36-03.12",		0x040000, 0x1ea8558b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e36-02.8",		0x040000, 0x694eda31, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e36-01.4",		0x040000, 0x54b9b2cd, TAITO_SPRITESA },  //  6

	{ "e36-07.47",		0x080000, 0x266bf1c1, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e36-06.45",		0x080000, 0x110ab729, TAITO_CHARS_BYTESWAP },     //  8
	{ "e36-05.43",		0x080000, 0xdb18bce2, TAITO_CHARS },     //  9

	{ "e36-12.32",		0x040000, 0x3bae39be, TAITO_68KROM2_BYTESWAP},    // 10 68k Code
	{ "e36-13.33",		0x040000, 0x94448e82, TAITO_68KROM2_BYTESWAP},    // 11

	{ "e36-04.38",		0x200000, 0x2250959b, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples

	{ "pal16l8a-d77-09.bin",	0x000104, 0xb371532b, 0 },           // 13 plds
	{ "pal16l8a-d77-10.bin",	0x000104, 0x42f59227, 0 },           // 14
	{ "palce16v8-d77-11.bin",	0x000117, 0xeacc294e, 0 },           // 15
	{ "palce16v8-d77-12.bin",	0x000117, 0xe9920cfe, 0 },           // 16
	{ "palce16v8-d77-14.bin",	0x000117, 0x7427e777, 0 },           // 17
};

STD_ROM_PICK(arkretrnu)
STD_ROM_FN(arkretrnu)

struct BurnDriver BurnDrvArkretrnu = {
	"arkretrnu", "arkretrn", NULL, NULL, "1997",
	"Arkanoid Returns (Ver 2.02A 1997/02/10)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arkretrnuRomInfo, arkretrnuRomName, NULL, NULL, F3InputInfo, NULL,
	arkretrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Arkanoid Returns (Ver 2.02J 1997/02/10)

static struct BurnRomInfo arkretrnjRomDesc[] = {
	{ "e36-11.20",		0x040000, 0xb50cfb92, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e36-10.19",		0x040000, 0xc940dba1, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e36-09.18",		0x040000, 0xf16985e0, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e36-08.17",		0x040000, 0xaa699e1b, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e36-03.12",		0x040000, 0x1ea8558b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e36-02.8",		0x040000, 0x694eda31, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e36-01.4",		0x040000, 0x54b9b2cd, TAITO_SPRITESA },  //  6

	{ "e36-07.47",		0x080000, 0x266bf1c1, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e36-06.45",		0x080000, 0x110ab729, TAITO_CHARS_BYTESWAP },     //  8
	{ "e36-05.43",		0x080000, 0xdb18bce2, TAITO_CHARS },     //  9

	{ "e36-12.32",		0x040000, 0x3bae39be, TAITO_68KROM2_BYTESWAP},    // 10 68k Code
	{ "e36-13.33",		0x040000, 0x94448e82, TAITO_68KROM2_BYTESWAP},    // 11

	{ "e36-04.38",		0x200000, 0x2250959b, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples

	{ "pal16l8a-d77-09.bin",	0x000104, 0xb371532b, 0 },           // 13 plds
	{ "pal16l8a-d77-10.bin",	0x000104, 0x42f59227, 0 },           // 14
	{ "palce16v8-d77-11.bin",	0x000117, 0xeacc294e, 0 },           // 15
	{ "palce16v8-d77-12.bin",	0x000117, 0xe9920cfe, 0 },           // 16
	{ "palce16v8-d77-14.bin",	0x000117, 0x7427e777, 0 },           // 17
};

STD_ROM_PICK(arkretrnj)
STD_ROM_FN(arkretrnj)

struct BurnDriver BurnDrvArkretrnj = {
	"arkretrnj", "arkretrn", NULL, NULL, "1997",
	"Arkanoid Returns (Ver 2.02J 1997/02/10)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arkretrnjRomInfo, arkretrnjRomName, NULL, NULL, F3InputInfo, NULL,
	arkretrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Kirameki Star Road (Ver 2.10J 1997/08/29)

static struct BurnRomInfo kiramekiRomDesc[] = {
	{ "e44-19.20",		0x080000, 0x2f3b239a, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e44-18.19",		0x080000, 0x3137c8bc, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e44-17.18",		0x080000, 0x5905cd20, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e44-16.17",		0x080000, 0x5e9ac3fd, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e44-06.12",		0x400000, 0x80526d58, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e44-04.8",		0x400000, 0x28c7c295, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e44-05.11",		0x200000, 0x0fbc2b26, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "e44-03.7",		0x200000, 0xd9e63fb0, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "e44-02.4",		0x400000, 0x5481efde, TAITO_SPRITESA },           //  8
	{ "e44-01.3",		0x200000, 0xc4bdf727, TAITO_SPRITESA },           //  9

	{ "e44-14.47",		0x200000, 0x4597c034, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "e44-12.45",		0x200000, 0x7160a181, TAITO_CHARS_BYTESWAP },     // 11
	{ "e44-13.46",		0x100000, 0x0b016c4e, TAITO_CHARS_BYTESWAP },     // 12
	{ "e44-11.44",		0x100000, 0x34d84143, TAITO_CHARS_BYTESWAP },     // 13
	{ "e44-10.43",		0x200000, 0x326f738e, TAITO_CHARS },              // 14
	{ "e44-09.42",		0x100000, 0xa8e68eb7, TAITO_CHARS },              // 15

	{ "e44-20.51",		0x080000, 0x4df7e051, TAITO_68KROM2_BYTESWAP },   // 16 68k Code
	{ "e44-21.52",		0x080000, 0xd31b94b8, TAITO_68KROM2_BYTESWAP },   // 17
	{ "e44-15.53",		0x200000, 0x5043b608, TAITO_68KROM2 },            // 18

	{ "e44-07.38",		0x400000, 0xa9e28544, TAITO_ES5505_BYTESWAP },    // 19 Ensoniq Samples
	{ "e44-08.39",		0x400000, 0x33ba3037, TAITO_ES5505_BYTESWAP },    // 20
};

STD_ROM_PICK(kirameki)
STD_ROM_FN(kirameki)

static INT32 kiramekiInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, KIRAMEKI, 1);
}

struct BurnDriver BurnDrvKirameki = {
	"kirameki", NULL, NULL, NULL, "1997",
	"Kirameki Star Road (Ver 2.10J 1997/08/29)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_QUIZ, 0,
	NULL, kiramekiRomInfo, kiramekiRomName, NULL, NULL, F3InputInfo, NULL,
	kiramekiInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &DrvRecalc, 0x2000,
	320, 224, 4, 3
};


// Puchi Carat (Ver 2.02O 1997/10/29)

static struct BurnRomInfo puchicarRomDesc[] = {
	{ "e46-16",		0x080000, 0xcf2accdf, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e46-15",		0x080000, 0xc32c6ed8, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e46-14",		0x080000, 0xa154c300, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e46-18",		0x080000, 0x396e3122, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e46-06",		0x200000, 0xb74336f2, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e46-04",		0x200000, 0x463ecc4c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e46-05",		0x200000, 0x83a32eee, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "e46-03",		0x200000, 0xeb768193, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "e46-02",		0x200000, 0xfb046018, TAITO_SPRITESA },           //  8
	{ "e46-01",		0x200000, 0x34fc2103, TAITO_SPRITESA },           //  9

	{ "e46-12",		0x100000, 0x1f3a9851, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "e46-11",		0x100000, 0xe9f10bf1, TAITO_CHARS_BYTESWAP },     // 11
	{ "e46-10",		0x100000, 0x1999b76a, TAITO_CHARS },              // 12

	{ "e46-21",		0x040000, 0xb466cff6, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "e46-22",		0x040000, 0xc67b767e, TAITO_68KROM2_BYTESWAP },   // 14

	{ "e46-07",		0x200000, 0xf20af91e, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "e46-08",		0x200000, 0xf7f96e1d, TAITO_ES5505_BYTESWAP },    // 16
	{ "e46-09",		0x200000, 0x824135f8, TAITO_ES5505_BYTESWAP },    // 17

	{ "pal16l8a-d77-09a.bin",	0x104, 0xb371532b, BRF_OPT },             // 14 plds
	{ "pal16l8a-d77-10a.bin",	0x104, 0x42f59227, BRF_OPT },             // 15
	{ "palce16v8q-d77-11a.bin",	0x117, 0xeacc294e, BRF_OPT },             // 16
	{ "palce16v8q-d77-12a.bin",	0x117, 0xe9920cfe, BRF_OPT },             // 17
	{ "palce16v8q-d77-15a.bin",	0x117, 0x0edf820a, BRF_OPT },// 18
};

STD_ROM_PICK(puchicar)
STD_ROM_FN(puchicar)

static INT32 puchicarInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, PUCHICAR, 1);
}

struct BurnDriver BurnDrvPuchicar = {
	"puchicar", NULL, NULL, NULL, "1997",
	"Puchi Carat (Ver 2.02O 1997/10/29)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puchicarRomInfo, puchicarRomName, NULL, NULL, F3InputInfo, NULL,
	puchicarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puchi Carat (Ver 2.02J 1997/10/29)

static struct BurnRomInfo puchicarjRomDesc[] = {
	{ "e46-16",		0x080000, 0xcf2accdf, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e46-15",		0x080000, 0xc32c6ed8, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e46-14",		0x080000, 0xa154c300, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e46.13",		0x080000, 0x59fbdf3a, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e46-06",		0x200000, 0xb74336f2, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e46-04",		0x200000, 0x463ecc4c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e46-05",		0x200000, 0x83a32eee, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "e46-03",		0x200000, 0xeb768193, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "e46-02",		0x200000, 0xfb046018, TAITO_SPRITESA },           //  8
	{ "e46-01",		0x200000, 0x34fc2103, TAITO_SPRITESA },           //  9

	{ "e46-12",		0x100000, 0x1f3a9851, TAITO_CHARS_BYTESWAP },     // 10 Layer Tiles
	{ "e46-11",		0x100000, 0xe9f10bf1, TAITO_CHARS_BYTESWAP },     // 11
	{ "e46-10",		0x100000, 0x1999b76a, TAITO_CHARS },              // 12

	{ "e46-19",		0x040000, 0x2624eba0, TAITO_68KROM2_BYTESWAP },   // 13 68k Code
	{ "e46-20",		0x040000, 0x065e934f, TAITO_68KROM2_BYTESWAP },   // 14

	{ "e46-07",		0x200000, 0xf20af91e, TAITO_ES5505_BYTESWAP },    // 15 Ensoniq Samples
	{ "e46-08",		0x200000, 0xf7f96e1d, TAITO_ES5505_BYTESWAP },    // 16
	{ "e46-09",		0x200000, 0x824135f8, TAITO_ES5505_BYTESWAP },    // 17

	{ "pal16l8a-d77-09a.bin",	0x104, 0xb371532b, BRF_OPT },             // 14 plds
	{ "pal16l8a-d77-10a.bin",	0x104, 0x42f59227, BRF_OPT },             // 15
	{ "palce16v8q-d77-11a.bin",	0x117, 0xeacc294e, BRF_OPT },             // 16
	{ "palce16v8q-d77-12a.bin",	0x117, 0xe9920cfe, BRF_OPT },             // 17
	{ "palce16v8q-d77-15a.bin",	0x117, 0x0edf820a, BRF_OPT },// 18
};

STD_ROM_PICK(puchicarj)
STD_ROM_FN(puchicarj)

struct BurnDriver BurnDrvPuchicarj = {
	"puchicarj", "puchicar", NULL, NULL, "1997",
	"Puchi Carat (Ver 2.02J 1997/10/29)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puchicarjRomInfo, puchicarjRomName, NULL, NULL, F3InputInfo, NULL,
	puchicarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 4 (Ver 2.04O 1997/12/19)

static struct BurnRomInfo pbobble4RomDesc[] = {
	{ "e49-12.20",		0x080000, 0xfffea203, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e49-11.19",		0x080000, 0xbf69a087, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e49-10.18",		0x080000, 0x0307460b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e49-16.17",		0x080000, 0x0a021624, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e49-02",		0x100000, 0xc7d2f40b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e49-01",		0x100000, 0xa3dd5f85, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e49-08",		0x100000, 0x87408106, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e49-07",		0x100000, 0x1e1e8e1c, TAITO_CHARS_BYTESWAP },     //  7
	{ "e49-06",		0x100000, 0xec85f7ce, TAITO_CHARS },              //  8

	{ "e49-13.32",		0x040000, 0x83536f7f, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e49-14.33",		0x040000, 0x19815bdb, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e49-03",		0x200000, 0xf64303e0, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e49-04",		0x200000, 0x09be229c, TAITO_ES5505_BYTESWAP },    // 12
	{ "e49-05",		0x200000, 0x5ce90ee2, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.bin",	0x104, 0xb371532b, BRF_OPT },             // 14 plds
	{ "pal16l8a-d77-10.bin",	0x104, 0x42f59227, BRF_OPT },             // 15
	{ "palce16v8q-d77-11.bin",	0x117, 0xeacc294e, BRF_OPT },             // 16
	{ "palce16v8q-d77-12.bin",	0x117, 0xe9920cfe, BRF_OPT },             // 17
	{ "palce16v8q-d77-15.bin",	0x117, 0x00000000, BRF_NODUMP },// 18
};

STD_ROM_PICK(pbobble4)
STD_ROM_FN(pbobble4)

static INT32 pbobble4Init()
{
	return DrvInit(NULL, f3_24bit_palette_update, 0, PBOBBLE4, 1);
}

struct BurnDriver BurnDrvPbobble4 = {
	"pbobble4", NULL, NULL, NULL, "1997",
	"Puzzle Bobble 4 (Ver 2.04O 1997/12/19)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble4RomInfo, pbobble4RomName, NULL, NULL, F3InputInfo, NULL,
	pbobble4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 4 (Ver 2.04J 1997/12/19)

static struct BurnRomInfo pbobble4jRomDesc[] = {
	{ "e49-12.20",		0x080000, 0xfffea203, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e49-11.19",		0x080000, 0xbf69a087, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e49-10.18",		0x080000, 0x0307460b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e49-09.17",		0x080000, 0xe40c7708, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e49-02",		0x100000, 0xc7d2f40b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e49-01",		0x100000, 0xa3dd5f85, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e49-08",		0x100000, 0x87408106, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e49-07",		0x100000, 0x1e1e8e1c, TAITO_CHARS_BYTESWAP },     //  7
	{ "e49-06",		0x100000, 0xec85f7ce, TAITO_CHARS },              //  8

	{ "e49-13.32",		0x040000, 0x83536f7f, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e49-14.33",		0x040000, 0x19815bdb, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e49-03",		0x200000, 0xf64303e0, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e49-04",		0x200000, 0x09be229c, TAITO_ES5505_BYTESWAP },    // 12
	{ "e49-05",		0x200000, 0x5ce90ee2, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.bin",	0x104, 0xb371532b, BRF_OPT },             // 14 plds
	{ "pal16l8a-d77-10.bin",	0x104, 0x42f59227, BRF_OPT },             // 15
	{ "palce16v8q-d77-11.bin",	0x117, 0xeacc294e, BRF_OPT },             // 16
	{ "palce16v8q-d77-12.bin",	0x117, 0xe9920cfe, BRF_OPT },             // 17
	{ "palce16v8q-d77-15.bin",	0x117, 0x00000000, BRF_NODUMP },// 18
};

STD_ROM_PICK(pbobble4j)
STD_ROM_FN(pbobble4j)

struct BurnDriver BurnDrvPbobble4j = {
	"pbobble4j", "pbobble4", NULL, NULL, "1997",
	"Puzzle Bobble 4 (Ver 2.04J 1997/12/19)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble4jRomInfo, pbobble4jRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Puzzle Bobble 4 (Ver 2.04A 1997/12/19)

static struct BurnRomInfo pbobble4uRomDesc[] = {
	{ "e49-12.20",		0x080000, 0xfffea203, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e49-11.19",		0x080000, 0xbf69a087, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e49-10.18",		0x080000, 0x0307460b, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e49-15.17",		0x080000, 0x7d0526b2, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e49-02",		0x100000, 0xc7d2f40b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e49-01",		0x100000, 0xa3dd5f85, TAITO_SPRITESA_BYTESWAP },  //  5

	{ "e49-08",		0x100000, 0x87408106, TAITO_CHARS_BYTESWAP },     //  6 Layer Tiles
	{ "e49-07",		0x100000, 0x1e1e8e1c, TAITO_CHARS_BYTESWAP },     //  7
	{ "e49-06",		0x100000, 0xec85f7ce, TAITO_CHARS },              //  8

	{ "e49-13.32",		0x040000, 0x83536f7f, TAITO_68KROM2_BYTESWAP },   //  9 68k Code
	{ "e49-14.33",		0x040000, 0x19815bdb, TAITO_68KROM2_BYTESWAP },   // 10

	{ "e49-03",		0x200000, 0xf64303e0, TAITO_ES5505_BYTESWAP },    // 11 Ensoniq Samples
	{ "e49-04",		0x200000, 0x09be229c, TAITO_ES5505_BYTESWAP },    // 12
	{ "e49-05",		0x200000, 0x5ce90ee2, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.bin",	0x104, 0xb371532b, BRF_OPT },             // 14 plds
	{ "pal16l8a-d77-10.bin",	0x104, 0x42f59227, BRF_OPT },             // 15
	{ "palce16v8q-d77-11.bin",	0x117, 0xeacc294e, BRF_OPT },             // 16
	{ "palce16v8q-d77-12.bin",	0x117, 0xe9920cfe, BRF_OPT },             // 17
	{ "palce16v8q-d77-15.bin",	0x117, 0x00000000, BRF_NODUMP },// 18
};

STD_ROM_PICK(pbobble4u)
STD_ROM_FN(pbobble4u)

struct BurnDriver BurnDrvPbobble4u = {
	"pbobble4u", "pbobble4", NULL, NULL, "1997",
	"Puzzle Bobble 4 (Ver 2.04A 1997/12/19)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble4uRomInfo, pbobble4uRomName, NULL, NULL, F3InputInfo, NULL,
	pbobble4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Pop'n Pop (Ver 2.07O 1998/02/09)

static struct BurnRomInfo popnpopRomDesc[] = {
	{ "e51-12.20",		0x080000, 0x86a237d5, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e51-11.19",		0x080000, 0x8a49f34f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e51-10.18",		0x080000, 0x4bce68f8, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e51-16.17",		0x080000, 0x2a9d8e0f, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e51-03.12",		0x100000, 0xa24c4607, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e51-02.8",		0x100000, 0x6aa8b96c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e51-01.4",		0x100000, 0x70347e24, TAITO_SPRITESA },           //  6

	{ "e51-08.47",		0x200000, 0x3ad41f02, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e51-07.45",		0x200000, 0x95873e46, TAITO_CHARS_BYTESWAP },     //  8
	{ "e51-06.43",		0x200000, 0xc240d6c8, TAITO_CHARS },              //  9

	{ "e51-13.32",		0x040000, 0x3b9e3986, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e51-14.33",		0x040000, 0x1f9a5015, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e51-04.38",		0x200000, 0x66790f55, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e51-05.41",		0x200000, 0x4d08b26d, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.ic14",	0x104, 0xb371532b, 0 },             // 14 plds
	{ "pal16l8a-d77-10.ic28",	0x104, 0x42f59227, 0 },             // 15
	{ "palce16v8q-d77-11.ic37",	0x117, 0xeacc294e, 0 },             // 16
	{ "palce16v8q-d77-12.ic48",	0x117, 0xe9920cfe, 0 },             // 17
	{ "palce16v8q-d77-15.ic21",	0x117, 0x00000000, 0 | BRF_NODUMP },// 18
};

STD_ROM_PICK(popnpop)
STD_ROM_FN(popnpop)

static INT32 popnpopInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, POPNPOP, 1);
}

struct BurnDriver BurnDrvPopnpop = {
	"popnpop", NULL, NULL, NULL, "1997",
	"Pop'n Pop (Ver 2.07O 1998/02/09)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, popnpopRomInfo, popnpopRomName, NULL, NULL, F3InputInfo, NULL,
	popnpopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Pop'n Pop (Ver 2.07J 1998/02/09)

static struct BurnRomInfo popnpopjRomDesc[] = {
	{ "e51-12.20",		0x080000, 0x86a237d5, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e51-11.19",		0x080000, 0x8a49f34f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e51-10.18",		0x080000, 0x4bce68f8, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e51-09.17",		0x080000, 0x4a086017, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e51-03.12",		0x100000, 0xa24c4607, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e51-02.8",		0x100000, 0x6aa8b96c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e51-01.4",		0x100000, 0x70347e24, TAITO_SPRITESA },           //  6

	{ "e51-08.47",		0x200000, 0x3ad41f02, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e51-07.45",		0x200000, 0x95873e46, TAITO_CHARS_BYTESWAP },     //  8
	{ "e51-06.43",		0x200000, 0xc240d6c8, TAITO_CHARS },              //  9

	{ "e51-13.32",		0x040000, 0x3b9e3986, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e51-14.33",		0x040000, 0x1f9a5015, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e51-04.38",		0x200000, 0x66790f55, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e51-05.41",		0x200000, 0x4d08b26d, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.ic14",	0x104, 0xb371532b, 0 },             // 14 plds
	{ "pal16l8a-d77-10.ic28",	0x104, 0x42f59227, 0 },             // 15
	{ "palce16v8q-d77-11.ic37",	0x117, 0xeacc294e, 0 },             // 16
	{ "palce16v8q-d77-12.ic48",	0x117, 0xe9920cfe, 0 },             // 17
	{ "palce16v8q-d77-15.ic21",	0x117, 0x00000000, 0 | BRF_NODUMP },// 18
};

STD_ROM_PICK(popnpopj)
STD_ROM_FN(popnpopj)

struct BurnDriver BurnDrvPopnpopj = {
	"popnpopj", "popnpop", NULL, NULL, "1997",
	"Pop'n Pop (Ver 2.07J 1998/02/09)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, popnpopjRomInfo, popnpopjRomName, NULL, NULL, F3InputInfo, NULL,
	popnpopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Pop'n Pop (Ver 2.07A 1998/02/09)

static struct BurnRomInfo popnpopuRomDesc[] = {
	{ "e51-12.20",		0x080000, 0x86a237d5, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e51-11.19",		0x080000, 0x8a49f34f, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e51-10.18",		0x080000, 0x4bce68f8, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e51-15.17",		0x080000, 0x1ad77903, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e51-03.12",		0x100000, 0xa24c4607, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e51-02.8",		0x100000, 0x6aa8b96c, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e51-01.4",		0x100000, 0x70347e24, TAITO_SPRITESA },           //  6

	{ "e51-08.47",		0x200000, 0x3ad41f02, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e51-07.45",		0x200000, 0x95873e46, TAITO_CHARS_BYTESWAP },     //  8
	{ "e51-06.43",		0x200000, 0xc240d6c8, TAITO_CHARS },              //  9

	{ "e51-13.32",		0x040000, 0x3b9e3986, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e51-14.33",		0x040000, 0x1f9a5015, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e51-04.38",		0x200000, 0x66790f55, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e51-05.41",		0x200000, 0x4d08b26d, TAITO_ES5505_BYTESWAP },    // 13

	{ "pal16l8a-d77-09.ic14",	0x104, 0xb371532b, 0 },             // 14 plds
	{ "pal16l8a-d77-10.ic28",	0x104, 0x42f59227, 0 },             // 15
	{ "palce16v8q-d77-11.ic37",	0x117, 0xeacc294e, 0 },             // 16
	{ "palce16v8q-d77-12.ic48",	0x117, 0xe9920cfe, 0 },             // 17
	{ "palce16v8q-d77-15.ic21",	0x117, 0x00000000, 0 | BRF_NODUMP },// 18
};

STD_ROM_PICK(popnpopu)
STD_ROM_FN(popnpopu)

struct BurnDriver BurnDrvPopnpopu = {
	"popnpopu", "popnpop", NULL, NULL, "1997",
	"Pop'n Pop (Ver 2.07A 1998/02/09)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, popnpopuRomInfo, popnpopuRomName, NULL, NULL, F3InputInfo, NULL,
	popnpopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Land Maker (Ver 2.01J 1998/06/01)

static struct BurnRomInfo landmakrRomDesc[] = {
	{ "e61-13.20",		0x080000, 0x0af756a2, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "e61-12.19",		0x080000, 0x636b3df9, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "e61-11.18",		0x080000, 0x279a0ee4, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "e61-10.17",		0x080000, 0xdaabf2b2, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "e61-03.12",		0x200000, 0xe8abfc46, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "e61-02.08",		0x200000, 0x1dc4a164, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "e61-01.04",		0x200000, 0x6cdd8311, TAITO_SPRITESA },           //  6

	{ "e61-09.47",		0x200000, 0x6ba29987, TAITO_CHARS_BYTESWAP },     //  7 Layer Tiles
	{ "e61-08.45",		0x200000, 0x76c98e14, TAITO_CHARS_BYTESWAP },     //  8
	{ "e61-07.43",		0x200000, 0x4a57965d, TAITO_CHARS },              //  9

	{ "e61-14.32",		0x020000, 0xb905f4a7, TAITO_68KROM2_BYTESWAP },   // 10 68k Code
	{ "e61-15.33",		0x020000, 0x87909869, TAITO_68KROM2_BYTESWAP },   // 11

	{ "e61-04.38",		0x200000, 0xc27aec0c, TAITO_ES5505_BYTESWAP },    // 12 Ensoniq Samples
	{ "e61-05.39",		0x200000, 0x83920d9d, TAITO_ES5505_BYTESWAP },    // 13
	{ "e61-06.40",		0x200000, 0x2e717bfe, TAITO_ES5505_BYTESWAP },    // 14
};

STD_ROM_PICK(landmakr)
STD_ROM_FN(landmakr)

static INT32 landmakrInit()
{
	return DrvInit(NULL, f3_24bit_palette_update, 1, LANDMAKR, 1);
}

struct BurnDriver BurnDrvLandmakr = {
	"landmakr", NULL, NULL, NULL, "1998",
	"Land Maker (Ver 2.01J 1998/06/01)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, landmakrRomInfo, landmakrRomName, NULL, NULL, F3InputInfo, NULL,
	landmakrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};


// Land Maker (Ver 2.02O 1998/06/02) (Prototype)

static struct BurnRomInfo landmakrpRomDesc[] = {
	{ "mpro-3.60",		0x80000, 0xf92eccd0, TAITO_68KROM1_BYTESWAP32 }, //  0 68ec20 Code
	{ "mpro-2.61",		0x80000, 0x5a26c9e0, TAITO_68KROM1_BYTESWAP32 }, //  1
	{ "mpro-1.62",		0x80000, 0x710776a8, TAITO_68KROM1_BYTESWAP32 }, //  2
	{ "mpro-0.63",		0x80000, 0xbc71dd2f, TAITO_68KROM1_BYTESWAP32 }, //  3

	{ "obj0-0.8",		0x80000, 0x4b862c1b, TAITO_SPRITESA_BYTESWAP },  //  4 Sprites
	{ "obj0-1.20",		0x80000, 0x1dc6e1ae, TAITO_SPRITESA_BYTESWAP },  //  5
	{ "obj1-0.7",		0x80000, 0x90502355, TAITO_SPRITESA_BYTESWAP },  //  6
	{ "obj1-1.19",		0x80000, 0xa24edb24, TAITO_SPRITESA_BYTESWAP },  //  7
	{ "obj2-0.6",		0x80000, 0x3bffe4b2, TAITO_SPRITESA_BYTESWAP },  //  8
	{ "obj2-1.18",		0x80000, 0x1b2a87f3, TAITO_SPRITESA_BYTESWAP },  //  9
	{ "obj3-0.5",		0x80000, 0x3a0e1479, TAITO_SPRITESA_BYTESWAP },  // 10
	{ "obj3-1.17",		0x80000, 0xc7e91180, TAITO_SPRITESA_BYTESWAP },  // 11
	{ "obj0-2.32",		0x80000, 0x94cc01d0, TAITO_SPRITESA },           // 12
	{ "obj1-2.31",		0x80000, 0xc2757722, TAITO_SPRITESA },           // 13
	{ "obj2-2.30",		0x80000, 0x934556ff, TAITO_SPRITESA },           // 14
	{ "obj3-2.29",		0x80000, 0x97f0f777, TAITO_SPRITESA },           // 15

	{ "scr0-0.7",		0x80000, 0xda6ba562, TAITO_CHARS_BYTESWAP32 },   // 16 Layer Tiles
	{ "scr0-2.5",		0x80000, 0x36756b9c, TAITO_CHARS_BYTESWAP32 },   // 17
	{ "scr0-1.6",		0x80000, 0x8c201d27, TAITO_CHARS_BYTESWAP32 },   // 18
	{ "scr0-3.4",		0x80000, 0x4e0274f3, TAITO_CHARS_BYTESWAP32 },   // 19
	{ "scr1-0.19",		0x80000, 0x2689f716, TAITO_CHARS_BYTESWAP32 },   // 20
	{ "scr1-2.17",		0x80000, 0x7841468a, TAITO_CHARS_BYTESWAP32 },   // 21
	{ "scr1-1.18",		0x80000, 0xf3086949, TAITO_CHARS_BYTESWAP32 },   // 22
	{ "scr1-3.16",		0x80000, 0x926ad229, TAITO_CHARS_BYTESWAP32 },   // 23
	{ "scr0-4.3",		0x80000, 0x5b3cf564, TAITO_CHARS_BYTESWAP },     // 24
	{ "scr0-5.2",		0x80000, 0x8e1ea0fe, TAITO_CHARS_BYTESWAP },     // 25
	{ "scr1-4.15",		0x80000, 0x783b6d10, TAITO_CHARS_BYTESWAP },     // 26
	{ "scr1-5.14",		0x80000, 0x24aba128, TAITO_CHARS_BYTESWAP },     // 27

	{ "spro-1.66",		0x40000, 0x18961bbb, TAITO_68KROM2_BYTESWAP },   // 28 68k Code
	{ "spro-0.65",		0x40000, 0x2c64557a, TAITO_68KROM2_BYTESWAP },   // 29

	{ "snd-0.43",		0x80000, 0x0e5ef5c8, TAITO_ES5505_BYTESWAP },    // 30 Ensoniq Samples
	{ "snd-1.44",		0x80000, 0x2998fd65, TAITO_ES5505_BYTESWAP },    // 31
	{ "snd-2.45",		0x80000, 0xda7477ad, TAITO_ES5505_BYTESWAP },    // 32
	{ "snd-3.46",		0x80000, 0x141670b9, TAITO_ES5505_BYTESWAP },    // 33
	{ "snd-4.32",		0x80000, 0xe9dc18f6, TAITO_ES5505_BYTESWAP },    // 34
	{ "snd-5.33",		0x80000, 0x8af91ca8, TAITO_ES5505_BYTESWAP },    // 35
	{ "snd-6.34",		0x80000, 0x6f520b82, TAITO_ES5505_BYTESWAP },    // 36
	{ "snd-7.35",		0x80000, 0x69410f0f, TAITO_ES5505_BYTESWAP },    // 37
	{ "snd-8.20",		0x80000, 0xd98c275e, TAITO_ES5505_BYTESWAP },    // 38
	{ "snd-9.21",		0x80000, 0x82a76cfc, TAITO_ES5505_BYTESWAP },    // 39
	{ "snd-10.22",		0x80000, 0x0345f585, TAITO_ES5505_BYTESWAP },    // 40
	{ "snd-11.23",		0x80000, 0x4caf571a, TAITO_ES5505_BYTESWAP },    // 41
};

STD_ROM_PICK(landmakrp)
STD_ROM_FN(landmakrp)

static INT32 landmakrpRomCallback()
{
	UINT32 *ROM = (UINT32 *)Taito68KRom1;

	ROM[0x1ffff8 / 4] = 0xffffffff;
	ROM[0x1ffffc / 4] = 0x0003ffff;

	return 0;
}

static INT32 landmakrpInit()
{
	return DrvInit(landmakrpRomCallback, f3_24bit_palette_update, 1, LANDMAKR, 1);
}

struct BurnDriver BurnDrvLandmakrp = {
	"landmakrp", "landmakr", NULL, NULL, "1998",
	"Land Maker (Ver 2.02O 1998/06/02) (Prototype)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, landmakrpRomInfo, landmakrpRomName, NULL, NULL, F3InputInfo, NULL,
	landmakrpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	320, 232, 4, 3
};
