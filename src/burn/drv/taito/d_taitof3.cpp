// FB Alpha Taito F3 driver module
// Based on MAME driver by Bryan McPhail and MANY others.

/*
    version .00001c ;)


	no attempt at speed-ups
	no attempt at cleaning at all
	ROM sets might be out of date!!!

*/

#define USE_CPU_SPEEDHACKS

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "taito.h"
#include "taitof3_snd.h"
#include "taitof3_video.h"
#include "es5506.h"
#include "eeprom.h"
#include "msm6295.h"

static UINT8 *DrvVRAMRAM;
static UINT8 *DrvPivotRAM;
static UINT16 *DrvCoinWord;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5[16];
static UINT8 DrvSrv[1];
static UINT8 DrvReset;
static UINT16 DrvInputs[5];
static UINT8 previous_coin;

static INT32 sound_cpu_in_reset = 0;
static INT32 watchdog;

INT32 f3_game = 0;

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
	{"Dip",	        BIT_DIPSWITCH,  TaitoDip + 0,	"dip"},
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

static struct BurnDIPInfo gseekerDIPList[]=
{
	{0x2e, 0xff, 0xff, 0x01, NULL },

	{0   , 0xfe, 0   , 2   , "Stage 5 Graphic Glitch Fix" },
	{0x2e, 0x01, 0x01, 0x01, "Yes" },
	{0x2e, 0x01, 0x01, 0x00, "No" },
};

STDDIPINFO(gseeker)

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

		case 0x1c:
		{
			if (BurnDrvGetFlags() & BDF_BOOTLEG) {
				if ((offset & 3) == 3) { MSM6295Command(0, d); return; }
			//	bprintf (0, _T("Sound Command: %x, %x %d\n"), offset & 0x1f, d, b); 
				if ((offset & 3) == 0) { } // banking
			}
		}
		return;
	}
}

static void __fastcall f3_main_write_long(UINT32 a, UINT32 d)
{
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

static UINT32 control_r(INT32 offset, INT32 b)
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

		case 0x1c:
			ret = MSM6295ReadStatus(0);
		break;
	}

	return ret;
}

static UINT32 __fastcall f3_main_read_long(UINT32 a)
{
	if ((a & 0xffffe0) == 0x4a0000) {
		return control_r(a, 4);
	}

	return 0;
}

static UINT16 __fastcall f3_main_read_word(UINT32 a)
{
	if ((a & 0xffffe0) == 0x4a0000) {
		return control_r(a, 2) >> ((~a & 2) * 8);
	}

	return 0;
}

static UINT8 __fastcall f3_main_read_byte(UINT32 a)
{
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
	if ((a & 0xffe000) == 0x61c000) {
		*((UINT32*)(TaitoVideoRam + (a & 0x1ffc))) = (d << 16) | (d >> 16);
		dirty_tile_count[9] = 1;
		return;
	}

	if ((a & 0xffe000) == 0x61e000) {
		*((UINT32*)(DrvVRAMRAM + (a & 0x1ffc))) = (d << 16) | (d >> 16);
		DrvVRAMExpand(a);
		return;
	}
}

static void __fastcall f3_VRAM_write_word(UINT32 a, UINT16 d)
{
	if ((a & 0xffe000) == 0x61c000) {
		*((UINT16*)(TaitoVideoRam + (a & 0x1ffe))) = d;
		dirty_tile_count[9] = 1;
		return;
	}

	if ((a & 0xffe000) == 0x61e000) {
		*((UINT16*)(DrvVRAMRAM + (a & 0x1ffe))) = d;
		DrvVRAMExpand(a);
		return;
	}
}

static void __fastcall f3_VRAM_write_byte(UINT32 a, UINT8 d)
{
	if ((a & 0xffe000) == 0x61c000) {
		TaitoVideoRam[(a & 0x1fff) ^ 1] = d;
		dirty_tile_count[9] = 1;
		return;
	}

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
		dirty_tile_count[9] = 1;
		return;
	}
}

static void __fastcall f3_pivot_write_word(UINT32 a, UINT16 d)
{
	if ((a & 0xff0000) == 0x630000) {
		*((UINT16*)(DrvPivotRAM + (a & 0xfffe))) = d;
		DrvPivotExpand(a);
		dirty_tile_count[9] = 1;
		return;
	}
}

static void __fastcall f3_pivot_write_byte(UINT32 a, UINT8 d)
{
	if ((a & 0xff0000) == 0x630000) {
		DrvPivotRAM[(a & 0xffff) ^ 1] = d;
		DrvPivotExpand(a);
		dirty_tile_count[9] = 1;
		return;
	}
}

static void __fastcall f3_playfield_write_long(UINT32 a, UINT32 d)
{
	if ((a & 0xff8000) == 0x610000) {
		UINT32 *ram = (UINT32*)(TaitoF3PfRAM + (a & 0x7ffc));

		if (ram[0] != ((d << 16) | (d >> 16))) {
			ram[0] = (d << 16) | (d >> 16);
			dirty_tiles[(a & 0x7ffc)/4] = 1;
			dirty_tile_count[((a & 0x7000)/0x1000)] = 1;
		}
		return;
	}
}

static void __fastcall f3_playfield_write_word(UINT32 a, UINT16 d)
{
	if ((a & 0xff8000) == 0x610000) {
		UINT16 *ram = (UINT16*)(TaitoF3PfRAM + (a & 0x7ffe));

		if (ram[0] != d) {
			ram[0] = d;
			dirty_tiles[(a & 0x7ffc)/4] = 1;
			dirty_tile_count[((a & 0x7000)/0x1000)] = 1;
		}
		return;
	}
}

static void __fastcall f3_playfield_write_byte(UINT32 a, UINT8 d)
{
	if ((a & 0xff8000) == 0x610000) {
		if (TaitoF3PfRAM[(a & 0x7fff) ^ 1] != d) {
			TaitoF3PfRAM[(a & 0x7fff) ^ 1] = d;
			dirty_tiles[(a & 0x7ffc)/4] = 1;
			dirty_tile_count[((a & 0x7000)/0x1000)] = 1;
		}
		return;
	}
}

static UINT32 speedhack_address;

static void __fastcall f3_speedhack_write_long(UINT32 a, UINT32 d)
{
	a &= 0x1fffe;
	*((UINT32*)(Taito68KRam1 + a)) = (d << 16) | (d >> 16);
	if (a == (speedhack_address & ~3)) {
	//	SekIdle(100);
		SekRunEnd(); // kill until next loop
	}
}

static void __fastcall f3_speedhack_write_word(UINT32 a, UINT16 d)
{
	a &= 0x1fffe;
	*((UINT16*)(Taito68KRam1 + (a & 0x1fffe))) = d;
	if (a == speedhack_address) {
	//	SekIdle(100);
		SekRunEnd(); // kill until next loop
	}
}

static void __fastcall f3_speedhack_write_byte(UINT32 a, UINT8 d)
{
	Taito68KRam1[(a & 0x1ffff) ^ 1] = d;
}

static void f3_speedhack_init(UINT32 address)
{
	if (address == 0) return;

#ifndef USE_CPU_SPEEDHACKS
	return;
#endif

	SekOpen(0);

	address &= ~0x20000;

	speedhack_address = address & 0x1fffe;

	SekMapHandler(5,		address & ~0x3ff, address | 0x3ff, MAP_WRITE);
	SekSetWriteLongHandler(5,	f3_speedhack_write_long);
	SekSetWriteWordHandler(5,	f3_speedhack_write_word);
	SekSetWriteByteHandler(5,	f3_speedhack_write_byte);

	address |= 0x20000;

	SekMapHandler(6,		address & ~0x3ff, address | 0x3ff, MAP_WRITE);
	SekSetWriteLongHandler(6,	f3_speedhack_write_long);
	SekSetWriteWordHandler(6,	f3_speedhack_write_word);
	SekSetWriteByteHandler(6,	f3_speedhack_write_byte);
	SekClose();
}

static void f3_reset_dirtybuffer()
{
	memset (dirty_tiles, 1, 0x8000/4);
	memset (dirty_tile_count, 1, 10);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (TaitoRamStart, 0, TaitoRamEnd - TaitoRamStart);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	if (BurnDrvGetFlags() & BDF_BOOTLEG) {
		MSM6295Reset(0);
	} else {
		TaitoF3SoundReset();
	}

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		if (TaitoDefaultEEProm[0] != 0 && f3_game != SCFINALS) {
			EEPROMFill((const UINT8*)TaitoDefaultEEProm, 0, 128);
		} else if (f3_game == RECALH || f3_game == GSEEKER ) {
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
		} else if (f3_game == ARKRETRN) {
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
		} else if (f3_game == PUCHICAR) {
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
		} else if (f3_game == SCFINALS) {
			static const UINT8 scfinals_eeprom[128] = {
				0x54,0x41,0x49,0x54,0x4f,0x03,0x50,0x01,0xe0,0x30,0xe0,0x01,0x11,0x12,0x30,0x00,
				0x00,0x00,0xff,0x00,0x00,0x32,0x00,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0xc3,0xf2,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
			};
			static const UINT8 scfinalso_eeprom[128] = {
				0x54,0x41,0x49,0x54,0x4f,0x03,0x50,0x01,0xe0,0x30,0xe0,0x01,0x11,0x12,0x30,0x00,
				0x00,0x00,0xff,0x00,0x00,0x31,0x00,0x00,0xff,0x00,0x00,0x00,0x00,0x00,0xc3,0xf3,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
				0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
			};
			if (strstr(BurnDrvGetTextA(DRV_NAME), "scfinalso")) {
				EEPROMFill(scfinalso_eeprom, 0, 128);
			} else {
				EEPROMFill(scfinals_eeprom, 0, 128);
			}
		}
	}

	f3_reset_dirtybuffer();

	TaitoF3VideoReset();

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
	if (f3_game == KIRAMEKI) Next += 0x200000;

	TaitoSpritesA		= Next; Next += TaitoSpriteARomSize;
	TaitoChars		= Next; Next += TaitoCharRomSize;

	tile_opaque_sp		= Next; Next += TaitoSpriteARomSize / 0x100;

	tile_opaque_pf[0]	= Next; Next += TaitoCharRomSize / 0x100;
	tile_opaque_pf[1]	= Next; Next += TaitoCharRomSize / 0x100;
	tile_opaque_pf[2]	= Next; Next += TaitoCharRomSize / 0x100;
	tile_opaque_pf[3]	= Next; Next += TaitoCharRomSize / 0x100;
	tile_opaque_pf[4]	= Next; Next += TaitoCharRomSize / 0x100;
	tile_opaque_pf[5]	= Next; Next += TaitoCharRomSize / 0x100;
	tile_opaque_pf[6]	= Next; Next += TaitoCharRomSize / 0x100;
	tile_opaque_pf[7]	= Next; Next += TaitoCharRomSize / 0x100;

	MSM6295ROM		= Next;
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
	TaitoF3PfRAM		= Next; Next += 0x000c000;
	TaitoVideoRam		= Next; Next += 0x0002000;
	DrvVRAMRAM		= Next; Next += 0x0002000;
	TaitoF3LineRAM		= Next; Next += 0x0010000;
	DrvPivotRAM		= Next; Next += 0x0010000;
	TaitoF3CtrlRAM		= Next; Next += 0x0000400;

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

static void DrvCalculateTransTable(INT32 sprlen, INT32 len)
{
	{
		UINT8 *sprite_gfx = TaitoSpritesA;

		memset (tile_opaque_sp, 1, sprlen/0x100);

		for (INT32 i = 0; i < sprlen; i++) {
			if (sprite_gfx[i] == 0) {
				tile_opaque_sp[i/0x100] = 0;
				i|=0xff;
			}
		}
	}

	{
		UINT8 *pf_gfx = TaitoChars;
	
		for (INT32 c = 0;c < len/0x100;c++)
		{
			INT32 x,y;
			INT32 extra_planes; /* 0 = 4bpp, 1=5bpp, 2=?, 3=6bpp */
	
			for (extra_planes=0; extra_planes<4; extra_planes++)
			{
				INT32 chk_trans_or_opa=0;
				UINT8 extra_mask = ((extra_planes << 4) | 0x0f);
				const UINT8 *dp = pf_gfx + c * 0x100;
	
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
	
				tile_opaque_pf[extra_planes][c]=chk_trans_or_opa;
			}
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

	TaitoSpriteAModulo = (spr_len / 0x100);
	TaitoCharModulo = (tile_len / 0x100);

	BurnFree (tmp);
}

static void tile_decode(INT32 spr_len, INT32 tile_len)
{
	UINT8 lsb,msb;
	UINT8 *gfx = TaitoChars;
	INT32 size=tile_len;
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

	DrvCalculateTransTable(spr_len, tile_len);
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

			if (f3_game == GSEEKER)
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


static INT32 DrvInit(INT32 (*pRomLoadCB)(), void (*pPalUpdateCB)(UINT16), INT32 extend, INT32 kludge, INT32 spritelag, UINT32 speedhack_addr)
{
	f3_game = kludge;

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
	SekMapMemory(TaitoF3PfRAM,		0x610000, 0x617fff, MAP_ROM); // write through handler
	SekMapMemory(TaitoF3PfRAM + 0x8000,	0x618000, 0x61bfff, MAP_RAM);
	SekMapMemory(TaitoVideoRam,	0x61c000, 0x61dfff, MAP_ROM); // write through
	SekMapMemory(DrvVRAMRAM,	0x61e000, 0x61ffff, MAP_ROM); // write through handler
	SekMapMemory(TaitoF3LineRAM,	0x620000, 0x62ffff, MAP_RAM);
	SekMapMemory(DrvPivotRAM,	0x630000, 0x63ffff, MAP_ROM); // write through handler
	SekMapMemory(TaitoF3CtrlRAM,	0x660000, 0x6603ff, MAP_WRITE); // 0-1f
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

	SekMapHandler(2,		0x61c000, 0x61ffff, MAP_WRITE); // VRAM and TaitoVideoRam
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

	f3_speedhack_init(speedhack_addr);

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

	if (BurnDrvGetFlags() & BDF_BOOTLEG) {
		MSM6295Exit(0);
	}

	EEPROMExit();

	BurnFree (TaitoMem);

	TaitoF3VideoExit();

	TaitoClearVariables(); // from taito.cpp

	pPaletteUpdateCallback = NULL;

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

static INT32 DrvDraw224A_Flipped() // 224A, w/ flipscreen
{
	TaitoF3DrawCommon(0x1234);

	return 0;
}

static INT32 DrvDraw224A()
{
	TaitoF3DrawCommon(31);

	return 0;
}

static INT32 DrvDraw224B()
{
	TaitoF3DrawCommon(32);

	return 0;
}

static INT32 DrvDraw()
{
	TaitoF3DrawCommon(24);

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

		if ((BurnDrvGetFlags() & BDF_BOOTLEG) == 0) {
			if (sound_cpu_in_reset == 0)
				TaitoF3CpuUpdate(nInterleave, i);
		}
	}

	if (BurnDrvGetFlags() & BDF_BOOTLEG) {
		if (pBurnSoundOut) {
			MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		}
	} else {
		TaitoF3SoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

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

		ba.Data		= TaitoF3PfRAM;
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
		
		ba.Data		= TaitoF3LineRAM;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x620000;
		ba.szName	= "Line RAM";
		BurnAcb(&ba);
		
		ba.Data		= DrvPivotRAM;
		ba.nLen		= 0x0010000;
		ba.nAddress	= 0x620000;
		ba.szName	= "Pivot RAM";
		BurnAcb(&ba);

		ba.Data		= TaitoF3CtrlRAM;
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
		SCAN_VAR(sound_cpu_in_reset);

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
	return DrvInit(NULL, f3_12bit_palette_update, 0, RINGRAGE, 2, 0);
}

struct BurnDriver BurnDrvRingrage = {
	"ringrage", NULL, NULL, NULL, "1992",
	"Ring Rage (Ver 2.3O 1992/08/09)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, ringrageRomInfo, ringrageRomName, NULL, NULL, F3InputInfo, NULL,
	ringrageInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	ringrageInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	ringrageInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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

	{ "D29-11.IC15.bin",	0x000157, 0x5dd5c8f9, BRF_OPT }, // 14 plds
	{ "pal20l8b.2",		0x000144, 0xc91437e2, BRF_OPT }, // 15
	{ "D29-13.IC14.bin",	0x000157, 0x74d61d36, BRF_OPT }, // 16
	{ "palce16v8h.11",	0x000117, 0x51088324, BRF_OPT }, // 17
	{ "pal16l8b.22",	0x000104, 0x3e01e854, BRF_OPT }, // 18
	{ "palce16v8h.31",	0x000117, 0xe0789727, BRF_OPT }, // 19
	{ "pal16l8b.62",	0x000104, 0x7093e2f3, BRF_OPT }, // 20
	{ "D29-14.IC28.bin",	0x000157, 0x25d205d5, BRF_OPT }, // 21
	{ "pal20l8b.70",	0x000144, 0x92b5b97c, BRF_OPT }, // 22
};

STD_ROM_PICK(arabianm)
STD_ROM_FN(arabianm)

static INT32 arabianmInit()
{
	return DrvInit(NULL, f3_12bit_palette_update, 0, ARABIANM, 2, 0x408100);
}

struct BurnDriver BurnDrvArabianm = {
	"arabianm", NULL, NULL, NULL, "1992",
	"Arabian Magic (Ver 1.0O 1992/07/06)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, arabianmRomInfo, arabianmRomName, NULL, NULL, F3InputInfo, NULL,
	arabianmInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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

	{ "D29-11.IC15.bin",	0x000157, 0x5dd5c8f9, BRF_OPT }, // 14 plds
	{ "pal20l8b.2",		0x000144, 0xc91437e2, BRF_OPT }, // 15
	{ "D29-13.IC14.bin",	0x000157, 0x74d61d36, BRF_OPT }, // 16
	{ "palce16v8h.11",	0x000117, 0x51088324, BRF_OPT }, // 17
	{ "pal16l8b.22",	0x000104, 0x3e01e854, BRF_OPT }, // 18
	{ "palce16v8h.31",	0x000117, 0xe0789727, BRF_OPT }, // 19
	{ "pal16l8b.62",	0x000104, 0x7093e2f3, BRF_OPT }, // 20
	{ "D29-14.IC28.bin",	0x000157, 0x25d205d5, BRF_OPT }, // 21
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
	arabianmInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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

	{ "D29-11.IC15.bin",	0x000157, 0x5dd5c8f9, BRF_OPT }, // 14 plds
	{ "pal20l8b.2",		0x000144, 0xc91437e2, BRF_OPT }, // 15
	{ "D29-13.IC14.bin",	0x000157, 0x74d61d36, BRF_OPT }, // 16
	{ "palce16v8h.11",	0x000117, 0x51088324, BRF_OPT }, // 17
	{ "pal16l8b.22",	0x000104, 0x3e01e854, BRF_OPT }, // 18
	{ "palce16v8h.31",	0x000117, 0xe0789727, BRF_OPT }, // 19
	{ "pal16l8b.62",	0x000104, 0x7093e2f3, BRF_OPT }, // 20
	{ "D29-14.IC28.bin",	0x000157, 0x25d205d5, BRF_OPT }, // 21
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
	arabianmInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_12bit_palette_update, 1, RIDINGF, 1, 0x417FE4);
}

struct BurnDriver BurnDrvRidingf = {
	"ridingf", NULL, NULL, NULL, "1992",
	"Riding Fight (Ver 1.0O)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, ridingfRomInfo, ridingfRomName, NULL, NULL, F3InputInfo, NULL,
	ridingfInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	ridingfInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	ridingfInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, GSEEKER, 1, 0x40A85C);
}

struct BurnDriver BurnDrvGseeker = {
	"gseeker", NULL, NULL, NULL, "1992",
	"Grid Seeker: Project Storm Hammer (Ver 1.3O)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gseekerRomInfo, gseekerRomName, NULL, NULL, F3InputInfo, gseekerDIPInfo,
	gseekerInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	NULL, gseekerjRomInfo, gseekerjRomName, NULL, NULL, F3InputInfo, gseekerDIPInfo,
	gseekerInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	NULL, gseekeruRomInfo, gseekeruRomName, NULL, NULL, F3InputInfo, gseekerDIPInfo,
	gseekerInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, COMMANDW, 1, 0x417FE4);
}

struct BurnDriver BurnDrvCommandw = {
	"commandw", NULL, NULL, NULL, "1992",
	"Command War - Super Special Battle & War Game (Ver 0.0J) (Prototype)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_MISC, 0,
	NULL, commandwRomInfo, commandwRomName, NULL, NULL, F3InputInfo, NULL,
	commandwInit, DrvExit, DrvFrame, DrvDraw224B, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, SCFINALS, 1, 0x408100);
}

struct BurnDriver BurnDrvCupfinal = {
	"cupfinal", NULL, NULL, NULL, "1993",
	"Taito Cup Finals (Ver 1.0O 1993/02/28)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, cupfinalRomInfo, cupfinalRomName, NULL, NULL, F3InputInfo, NULL,
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, TRSTAR, 0, 0x41E000);
}

struct BurnDriver BurnDrvTrstar = {
	"trstar", NULL, NULL, NULL, "1993",
	"Top Ranking Stars (Ver 2.1O 1993/05/21) (New Version)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, trstarRomInfo, trstarRomName, NULL, NULL, F3InputInfo, NULL,
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	trstarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, GUNLOCK, 2, 0x400004); // speed hack isn't great for this game
}

struct BurnDriver BurnDrvGunlock = {
	"gunlock", NULL, NULL, NULL, "1993",
	"Gunlock (Ver 2.3O 1994/01/20)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gunlockRomInfo, gunlockRomName, NULL, NULL, F3InputInfo, NULL,
	gunlockInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	gunlockInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	gunlockInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(scfinalsCallback, f3_24bit_palette_update, 0, SCFINALS, 1, 0x408100);
}

struct BurnDriver BurnDrvScfinals = {
	"scfinals", NULL, NULL, NULL, "1993",
	"Super Cup Finals (Ver 2.2O 1994/01/13)\0", "Use service coin! (game has issues)", "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, scfinalsRomInfo, scfinalsRomName, NULL, NULL, F3InputInfo, NULL,
	scfinalsInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	"Super Cup Finals (Ver 2.1O 1993/11/19)\0", "Use service coin! (game has issues)", "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, scfinalsoRomInfo, scfinalsoRomName, NULL, NULL, F3InputInfo, NULL,
	scfinalsInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, LIGHTBR, 2, 0x400118);
}

struct BurnDriver BurnDrvLightbr = {
	"lightbr", NULL, NULL, NULL, "1993",
	"Light Bringer (Ver 2.2O 1994/04/08)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SCRFIGHT, 0,
	NULL, lightbrRomInfo, lightbrRomName, NULL, NULL, F3InputInfo, NULL,
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	lightbrInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	cupfinalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_21bit_typeB_palette_update, 1, RECALH, 1, 0);
}

struct BurnDriver BurnDrvRecalh = {
	"recalh", NULL, NULL, NULL, "1994",
	"Recalhorn (Ver 1.42J 1994/5/11) (Prototype)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, recalhRomInfo, recalhRomName, NULL, NULL, F3InputInfo, NULL,
	recalhInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, KAISERKN, 2, 0x408100);
}

struct BurnDriver BurnDrvKaiserkn = {
	"kaiserkn", NULL, NULL, NULL, "1994",
	"Kaiser Knuckle (Ver 2.1O 1994/07/29)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_VSFIGHT, 0,
	NULL, kaiserknRomInfo, kaiserknRomName, NULL, NULL, KnInputInfo, NULL,
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	kaiserknInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, DARIUSG, 1, 0x406baa);
}

struct BurnDriver BurnDrvDariusg = {
	"dariusg", NULL, NULL, NULL, "1994",
	"Darius Gaiden - Silver Hawk (Ver 2.5O 1994/09/19)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_HORSHOOT, 0,
	NULL, dariusgRomInfo, dariusgRomName, NULL, NULL, F3InputInfo, NULL,
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	dariusgInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, BUBSYMPH, 1, 0x41f3fc);
}

struct BurnDriver BurnDrvBublbob2 = {
	"bublbob2", NULL, NULL, NULL, "1994",
	"Bubble Bobble II (Ver 2.6O 1994/12/16)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bublbob2RomInfo, bublbob2RomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(bublbob2pRomCallback, f3_24bit_palette_update, 1, BUBSYMPH, 1, 0);
}

struct BurnDriver BurnDrvBublbob2p = {
	"bublbob2p", "bublbob2", NULL, NULL, "1994",
	"Bubble Bobble II (Ver 0.0J 1993/12/13, prototype)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bublbob2pRomInfo, bublbob2pRomName, NULL, NULL, F3InputInfo, NULL,
	bublbob2pInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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

static void bootlegpatch()
{
	Taito68KRom1[0xed9ca] = 0x6d; // 2d
	Taito68KRom1[0xed9cb] = 0x4a; // 30
	Taito68KRom1[0xed9cc] = 0x00; // 8c
	Taito68KRom1[0xed9cd] = 0x80; // 82
	Taito68KRom1[0xed9ce] = 0x00; // 02
	Taito68KRom1[0xed9cf] = 0x66; // 6a
	Taito68KRom1[0xed9d0] = 0xcc; // 40
	Taito68KRom1[0xed9d1] = 0x00; // 44
	Taito68KRom1[0xed9d2] = 0x2d; // 40
	Taito68KRom1[0xed9d3] = 0x30; // 04
	Taito68KRom1[0xed9d4] = 0x8c; // 1a
	Taito68KRom1[0xed9d5] = 0x82; // 00
	Taito68KRom1[0xed9d6] = 0x40; // c0
	Taito68KRom1[0xed9d7] = 0x06; // 33
	Taito68KRom1[0xed9d8] = 0x29; // 66
	Taito68KRom1[0xed9da] = 0xc0; // 18
	Taito68KRom1[0xed9db] = 0x33; // 00
	Taito68KRom1[0xed9dc] = 0x66; // 2d
	Taito68KRom1[0xed9dd] = 0x00; // 30
	Taito68KRom1[0xed9de] = 0x18; // a0
	Taito68KRom1[0xed9df] = 0x00; // 82
	Taito68KRom1[0xed9e0] = 0x2d; // 02
	Taito68KRom1[0xed9e1] = 0x30; // 6a
	Taito68KRom1[0xed9e2] = 0xa0; // 40
	Taito68KRom1[0xed9e3] = 0x82; // 44
	Taito68KRom1[0xed9e5] = 0x06; // 04
	Taito68KRom1[0xed9e6] = 0x1f; // 00
	Taito68KRom1[0xed9f4] = 0xd8; // 2c
	Taito68KRom1[0xed9f5] = 0xff; // 00
	Taito68KRom1[0xeda1c] = 0xdc; // 2d
	Taito68KRom1[0xeda1d] = 0xff; // 00
	Taito68KRom1[0xeda44] = 0xe0; // 30
	Taito68KRom1[0xeda45] = 0xff; // 00
	Taito68KRom1[0xeda6c] = 0xe4; // 32
	Taito68KRom1[0xeda6d] = 0xff; // 00
	Taito68KRom1[0xedaa1] = 0x06; // 04
	Taito68KRom1[0xedaa2] = 0x9e; // 10
	Taito68KRom1[0xedba8] = 0x29; // f0
	Taito68KRom1[0xedba9] = 0x00; // ff

	Taito68KRom1[0xee1d0] = 0x3e; // 00	// black backgrounds
	Taito68KRom1[0xee1d2] = 0xc0; // 00
	Taito68KRom1[0xee1d3] = 0x01; // 00
	Taito68KRom1[0xee1d4] = 0xfa; // 75
	Taito68KRom1[0xee1d5] = 0x41; // 4e

	Taito68KRom1[0xf04e7] = 0x08; // 00	- demo logo
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[8] = { STEP8(0, 0x080000*8) };
	INT32 XOffs0[16] = { STEP8(7,-1), STEP8(15,-1) };
	INT32 YOffs0[16] = { STEP16(0,16) };

	INT32 Plane1[5] = { 0x200000*8, STEP4(0,1) };
	INT32 XOffs1[16] = { 20,16,12,8, 4,0,28,24, 52,48,44,40, 36,32,60,56 };
	INT32 YOffs1[16] = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	{
		UINT8 *gfx = TaitoChars;

		for (INT32 i=0x200000;i<0x400000; i+=4)
		{
			UINT8 byte = gfx[i];
			gfx[i+0] = (byte & 0x80)? 1<<7 : 0<<4;
			gfx[i+0]|= (byte & 0x40)? 1<<3 : 0<<0;
			gfx[i+1] = (byte & 0x20)? 1<<7 : 0<<4;
			gfx[i+1]|= (byte & 0x10)? 1<<3 : 0<<0;
			gfx[i+2] = (byte & 0x08)? 1<<7 : 0<<4;
			gfx[i+2]|= (byte & 0x04)? 1<<3 : 0<<0;
			gfx[i+3] = (byte & 0x02)? 1<<7 : 0<<4;
			gfx[i+3]|= (byte & 0x01)? 1<<3 : 0<<0;
		}
	}

	memcpy (tmp, TaitoSpritesA, 0x400000);

	GfxDecode(0x4000, 6, 16, 16, Plane0, XOffs0, YOffs0, 0x100, tmp, TaitoSpritesA);

	memcpy (tmp, TaitoChars, 0x400000);

	GfxDecode(0x4000, 5, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, TaitoChars);

	UINT8 tmp2[0x8];

	for (INT32 i = 0x200000; i < 0x400000; i+=8) {
		memcpy (tmp2, TaitoChars + i, 8);

		for (INT32 j = 0; j < 8; j++) {
			TaitoChars[i+j] &= 0xf;
			TaitoChars[i+j] |= 0x20;
			tmp2[j] &= 0x10;
		}

		TaitoChars[i+0] |= tmp2[6];
		TaitoChars[i+1] |= tmp2[7];
		TaitoChars[i+2] |= tmp2[0];
		TaitoChars[i+3] |= tmp2[1];
		TaitoChars[i+4] |= tmp2[2];
		TaitoChars[i+5] |= tmp2[3];
		TaitoChars[i+6] |= tmp2[4];
		TaitoChars[i+7] |= tmp2[5];
	}

	DrvCalculateTransTable(0x400000,0x400000);

	BurnFree(tmp);

	return 0;
}

static INT32 bubsymphbInit()
{
	f3_game = BUBSYMPH;

	TaitoSpriteARomSize = (0x300000 * 8) / 6;
	TaitoCharRomSize = 0x400000;
	TaitoF3ES5506RomSize = 0x80000;
	TaitoSpriteAModulo = (TaitoSpriteARomSize / 0x100);
	TaitoCharModulo = (TaitoCharRomSize / 0x100);

	MemIndex();
	INT32 nLen = TaitoMemEnd - (UINT8 *)0;
	if ((TaitoMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(TaitoMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Taito68KRom1  + 0x000001,  0, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1  + 0x000000,  1, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1  + 0x000003,  2, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1  + 0x000002,  3, 4)) return 1;

		if (BurnLoadRom(TaitoSpritesA + 0x080000,  4, 1)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x100000,  5, 1)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x180000,  6, 1)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x200000,  7, 1)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x280000,  8, 1)) return 1;

		if (BurnLoadRom(TaitoChars    + 0x000000,  9, 4)) return 1;
		if (BurnLoadRom(TaitoChars    + 0x000001, 10, 4)) return 1;
		if (BurnLoadRom(TaitoChars    + 0x000002, 11, 4)) return 1;
		if (BurnLoadRom(TaitoChars    + 0x000003, 12, 4)) return 1;
		if (BurnLoadRom(TaitoChars    + 0x200000, 13, 4)) return 1;

		if (BurnLoadRom(TaitoF3ES5506Rom,         14, 1)) return 1;

		DrvGfxDecode();

		bootlegpatch();
	}

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,	0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,	0x400000, 0x41ffff, MAP_RAM);
	SekMapMemory(Taito68KRam1,	0x420000, 0x43ffff, MAP_RAM); // mirror
	SekMapMemory(TaitoPaletteRam,	0x440000, 0x447fff, MAP_ROM); // write through handler
	SekMapMemory(TaitoSpriteRam,	0x600000, 0x60ffff, MAP_RAM);
	SekMapMemory(TaitoF3PfRAM,		0x610000, 0x617fff, MAP_ROM); // write through handler
	SekMapMemory(TaitoF3PfRAM + 0x8000,	0x618000, 0x61bfff, MAP_RAM);
	SekMapMemory(TaitoVideoRam,	0x61c000, 0x61dfff, MAP_RAM);
	SekMapMemory(DrvVRAMRAM,	0x61e000, 0x61ffff, MAP_ROM); // write through handler
	SekMapMemory(TaitoF3LineRAM,	0x620000, 0x62ffff, MAP_RAM);
	SekMapMemory(DrvPivotRAM,	0x630000, 0x63ffff, MAP_ROM); // write through handler
	SekMapMemory(TaitoF3CtrlRAM,	0x660000, 0x6603ff, MAP_WRITE); // 0-1f
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

	TaitoF3SoundInit(1); // keep us safe from crashes!

	MSM6295Init(0, 1000000 / 132, 0);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	EEPROMInit(&eeprom_interface_93C46);
	EEPROMIgnoreErrMessage(1);

	if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
		BurnDrvGetVisibleSize(&nScreenHeight, &nScreenWidth);
	} else {
		BurnDrvGetVisibleSize(&nScreenWidth, &nScreenHeight);
	}

	pPaletteUpdateCallback = f3_24bit_palette_update;
	extended_layers = 1;
	sprite_lag = 1;

	TaitoF3VideoInit();

	bitmap_width[0] = extended_layers ? 1024 : 512;
	bitmap_width[1] = extended_layers ? 1024 : 512;
	bitmap_width[2] = extended_layers ? 1024 : 512;
	bitmap_width[3] = extended_layers ? 1024 : 512;
	bitmap_width[4] = 512;
	bitmap_width[5] = 512;
	bitmap_width[6] = 512;
	bitmap_width[7] = 512;

	DrvDoReset(1);

	return 0;
}


struct BurnDriverD BurnDrvBubsymphb = {
	"bubsymphb", "bublbob2", NULL, NULL, "1994",
	"Bubble Symphony (bootleg with OKI6295)\0", NULL, "bootleg", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_BOOTLEG, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubsymphbRomInfo, bubsymphbRomName, NULL, NULL, F3InputInfo, NULL,
	bubsymphbInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	bublbob2Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_12bit_palette_update, 1, SPCINVDX, 1, 0x400218);
}

struct BurnDriver BurnDrvSpcinvdj = {
	"spcinvdj", "spacedx", NULL, NULL, "1994",
	"Space Invaders DX (Ver 2.6J 1994/09/14) (F3 Version)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, spcinvdjRomInfo, spcinvdjRomName, NULL, NULL, F3InputInfo, NULL,
	spcinvdjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, HTHERO95, 1, 0);
}

struct BurnDriver BurnDrvPwrgoal = {
	"pwrgoal", NULL, NULL, NULL, "1994",
	"Taito Power Goal (Ver 2.5O 1994/11/03)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SPORTSFOOTBALL, 0,
	NULL, pwrgoalRomInfo, pwrgoalRomName, NULL, NULL, F3InputInfo, NULL,
	pwrgoalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pwrgoalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pwrgoalInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, QTHEATER, 1, 0);
}

struct BurnDriver BurnDrvQtheater = {
	"qtheater", NULL, NULL, NULL, "1994",
	"Quiz Theater - 3tsu no Monogatari (Ver 2.3J 1994/11/10)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_QUIZ, 0,
	NULL, qtheaterRomInfo, qtheaterRomName, NULL, NULL, F3InputInfo, NULL,
	qtheaterInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, SPCINV95, 1, 0x408100);
}

struct BurnDriver BurnDrvSpcinv95 = {
	"spcinv95", NULL, NULL, NULL, "1995",
	"Space Invaders '95: The Attack Of Lunar Loonies (Ver 2.5O 1995/06/14)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, spcinv95RomInfo, spcinv95RomName, NULL, NULL, F3InputInfo, NULL,
	spcinv95Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	spcinv95Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	spcinv95Init, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, EACTION2, 2, 0x4007a2);
}

struct BurnDriver BurnDrvElvactr = {
	"elvactr", NULL, NULL, NULL, "1994",
	"Elevator Action Returns (Ver 2.2O 1995/02/20)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, elvactrRomInfo, elvactrRomName, NULL, NULL, F3InputInfo, NULL,
	elvactrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	elvactrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	elvactrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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

static INT32 twinqixInit()
{
	return DrvInit(NULL/*twinqixRomCallback*/, f3_21bit_typeB_palette_update, 1, TWINQIX, 1, 0x40011c);
}

struct BurnDriver BurnDrvTwinqix = {
	"twinqix", NULL, NULL, NULL, "1995",
	"Twin Qix (Ver 1.0A 1995/01/17) (Prototype)\0", NULL, "Taito America Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, twinqixRomInfo, twinqixRomName, NULL, NULL, F3InputInfo, NULL,
	twinqixInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, QUIZHUHU, 1, 0);
}

struct BurnDriver BurnDrvQuizhuhu = {
	"quizhuhu", NULL, NULL, NULL, "1995",
	"Moriguchi Hiroko no Quiz de Hyuu!Hyuu! (Ver 2.2J 1995/05/25)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_QUIZ, 0,
	NULL, quizhuhuRomInfo, quizhuhuRomName, NULL, NULL, F3InputInfo, NULL,
	quizhuhuInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, PBOBBLE2, 1, 0x40451c);
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
	return DrvInit(pbobble23OCallback, f3_24bit_palette_update, 0, PBOBBLE2, 1, 0x40451c);
}

struct BurnDriver BurnDrvPbobble2 = {
	"pbobble2", NULL, NULL, NULL, "1995",
	"Puzzle Bobble 2 (Ver 2.3O 1995/07/31)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble2RomInfo, pbobble2RomName, NULL, NULL, F3InputInfo, NULL,
	pbobble23OInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, GEKIRIDO, 1, 0x406bb0);
}

struct BurnDriver BurnDrvGekiridn = {
	"gekiridn", NULL, NULL, NULL, "1995",
	"Gekirindan (Ver 2.3O 1995/09/21)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 2, HARDWARE_TAITO_MISC, GBF_VERSHOOT, 0,
	NULL, gekiridnRomInfo, gekiridnRomName, NULL, NULL, F3InputInfo, NULL,
	gekiridnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	gekiridnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	
	{ "d77-12.ic48.bin", 	0x117, 0x6f93a4d8, BRF_OPT },
	{ "d77-14.ic21.bin", 	0x117, 0xf2264f51, BRF_OPT },
	{ "palce16v8.ic37.bin", 0x117, 0x6ccd8168, BRF_OPT },
	{ "d77-09.ic14.bin", 	0x001, 0x00000000, BRF_NODUMP },
	{ "d77-10.ic28.bin", 	0x001, 0x00000000, BRF_NODUMP },
};

STD_ROM_PICK(tcobra2)
STD_ROM_FN(tcobra2)

static INT32 tcobra2Init()
{
	INT32 rc = DrvInit(NULL, f3_24bit_palette_update, 0, KTIGER2, 0, 0);

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
	tcobra2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	tcobra2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	tcobra2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, BUBBLEM, 1, 0x40011c);
}

struct BurnDriver BurnDrvBubblem = {
	"bubblem", NULL, NULL, NULL, "1995",
	"Bubble Memories: The Story Of Bubble Bobble III (Ver 2.4O 1996/02/15)\0", NULL, "Taito Corporation Japan", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PLATFORM, 0,
	NULL, bubblemRomInfo, bubblemRomName, NULL, NULL, F3InputInfo, NULL,
	bubblemInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	bubblemInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_21bit_typeA_palette_update, 0, CLEOPATR, 1, 0);
}

struct BurnDriver BurnDrvCleopatr = {
	"cleopatr", NULL, NULL, NULL, "1996",
	"Cleopatra Fortune (Ver 2.1J 1996/09/05)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, cleopatrRomInfo, cleopatrRomName, NULL, NULL, F3InputInfo, NULL,
	cleopatrInit, DrvExit, DrvFrame, DrvDraw224A_Flipped, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, PBOBBLE3, 1, 0x4055c0);
}

struct BurnDriver BurnDrvPbobble3 = {
	"pbobble3", NULL, NULL, NULL, "1996",
	"Puzzle Bobble 3 (Ver 2.1O 1996/09/27)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble3RomInfo, pbobble3RomName, NULL, NULL, F3InputInfo, NULL,
	pbobble3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble3Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, ARKRETRN, 1, 0);
}

struct BurnDriver BurnDrvArkretrn = {
	"arkretrn", NULL, NULL, NULL, "1997",
	"Arkanoid Returns (Ver 2.02O 1997/02/10)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_BREAKOUT, 0,
	NULL, arkretrnRomInfo, arkretrnRomName, NULL, NULL, F3InputInfo, NULL,
	arkretrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	arkretrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	arkretrnInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, KIRAMEKI, 1, 0);
}

struct BurnDriver BurnDrvKirameki = {
	"kirameki", NULL, NULL, NULL, "1997",
	"Kirameki Star Road (Ver 2.10J 1997/08/29)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_QUIZ, 0,
	NULL, kiramekiRomInfo, kiramekiRomName, NULL, NULL, F3InputInfo, NULL,
	kiramekiInit, DrvExit, DrvFrame, DrvDraw224A, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, PUCHICAR, 1, 0);
}

struct BurnDriver BurnDrvPuchicar = {
	"puchicar", NULL, NULL, NULL, "1997",
	"Puchi Carat (Ver 2.02O 1997/10/29)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, puchicarRomInfo, puchicarRomName, NULL, NULL, F3InputInfo, NULL,
	puchicarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	puchicarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 0, PBOBBLE4, 1, 0x4053c0);
}

struct BurnDriver BurnDrvPbobble4 = {
	"pbobble4", NULL, NULL, NULL, "1997",
	"Puzzle Bobble 4 (Ver 2.04O 1997/12/19)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, pbobble4RomInfo, pbobble4RomName, NULL, NULL, F3InputInfo, NULL,
	pbobble4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	pbobble4Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, POPNPOP, 1, 0);
}

struct BurnDriver BurnDrvPopnpop = {
	"popnpop", NULL, NULL, NULL, "1997",
	"Pop'n Pop (Ver 2.07O 1998/02/09)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, popnpopRomInfo, popnpopRomName, NULL, NULL, F3InputInfo, NULL,
	popnpopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	popnpopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	popnpopInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(NULL, f3_24bit_palette_update, 1, LANDMAKR, 1, 0x400826);
}

struct BurnDriver BurnDrvLandmakr = {
	"landmakr", NULL, NULL, NULL, "1998",
	"Land Maker (Ver 2.01J 1998/06/01)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, landmakrRomInfo, landmakrRomName, NULL, NULL, F3InputInfo, NULL,
	landmakrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
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
	return DrvInit(landmakrpRomCallback, f3_24bit_palette_update, 1, LANDMAKR, 1, 0);
}

struct BurnDriver BurnDrvLandmakrp = {
	"landmakrp", "landmakr", NULL, NULL, "1998",
	"Land Maker (Ver 2.02O 1998/06/02) (Prototype)\0", NULL, "Taito Corporation", "F3 System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_PUZZLE, 0,
	NULL, landmakrpRomInfo, landmakrpRomName, NULL, NULL, F3InputInfo, NULL,
	landmakrpInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &TaitoF3PalRecalc, 0x2000,
	320, 232, 4, 3
};
