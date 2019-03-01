// FB Alpha Taito Under Fire / Chase Bombers
// Based on MAME driver by Bryan McPhail and David Graves

// do-to:
// cbombers priority mess :(

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "taito.h"
#include "taito_ic.h"
#include "taitof3_snd.h"
#include "eeprom.h"
#include "watchdog.h"
#include "burn_shift.h"
#include "burn_gun.h"

static INT32 subcpu_in_reset;
static INT32 interrupt5_timer;

static UINT8 ReloadGun[2] = { 0, 0 };

static INT32 has_subcpu = 0;
static UINT8 DrvRecalc;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo UndrfireInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	TaitoInputPort3 + 2,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	TaitoInputPort3 + 3,	"p2 coin"	},

	{"P1 Start",		BIT_DIGITAL,	TaitoInputPort2 + 4,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	TaitoInputPort1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TaitoInputPort1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	ReloadGun + 0,          "p1 fire 3"	},
	A("P1 Gun X",       BIT_ANALOG_REL, &TaitoAnalogPort0,      "mouse x-axis"),
	A("P1 Gun Y",       BIT_ANALOG_REL, &TaitoAnalogPort1,      "mouse y-axis"),

	{"P2 Start",		BIT_DIGITAL,	TaitoInputPort2 + 5,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	TaitoInputPort1 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	TaitoInputPort1 + 7,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	ReloadGun + 1,          "p2 fire 3"	},
	A("P2 Gun X",       BIT_ANALOG_REL, &TaitoAnalogPort2,      "p2 x-axis"),
	A("P2 Gun Y",       BIT_ANALOG_REL, &TaitoAnalogPort3,      "p2 y-axis"),

	{"Reset",			BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	TaitoInputPort3 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	TaitoInputPort3 + 0,	"diag"	    },
};

STDINPUTINFO(Undrfire)

static struct BurnInputInfo CbombersInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	TaitoInputPort3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TaitoInputPort2 + 4,    "p1 start"	},

	{"P1 Accelerator",	BIT_DIGITAL,	TaitoInputPort0 + 2,    "p1 fire 1"	},
	{"P1 Brake",	    BIT_DIGITAL,	TaitoInputPort0 + 3,    "p1 fire 2"	},
	{"P1 Nitro",		BIT_DIGITAL,	TaitoInputPort0 + 1,    "p1 fire 3"	},
	{"P1 Gear Shift",	BIT_DIGITAL,	TaitoInputPort0 + 0,    "p1 fire 4"	},

	A("P1 Steering",	BIT_ANALOG_REL, &TaitoAnalogPort0,		"p1 x-axis" ),

	{"P2 Coin",			BIT_DIGITAL,	TaitoInputPort3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TaitoInputPort2 + 4,    "p2 start"	},

	{"Freeze",			BIT_DIGITAL,	TaitoInputPort1 + 3,	"p1 fire 5"	},
	{"Reset",			BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	TaitoInputPort3 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	TaitoInputPort3 + 0,	"diag"	    },
};

STDINPUTINFO(Cbombers)
#undef A


static void __fastcall undrfire_main_write_long(UINT32 a, UINT32 d)
{
	TC0100SCN0LongWrite_Map(0x900000, 0x90ffff)

	switch (a)
	{
		case 0x400000:
		{
			if (!has_subcpu) return; // for chase bomber

			INT32 previous = subcpu_in_reset;
			subcpu_in_reset = (~d >> 12) & 1;
			if (!subcpu_in_reset && previous) {
				SekClose();
				SekOpen(2);
				SekReset();
				SekClose();
				SekOpen(0);
			}
		}
		return;

		case 0x304000:
		case 0x304400:
		return; // ??
	}

	bprintf (0, _T("WL: %5.5x, %8.8x\n"), a, d);
}

static void __fastcall undrfire_main_write_word(UINT32 a, UINT16 d)
{
	TC0100SCN0WordWrite_Map(0x900000, 0x90ffff)
	TC0480SCPCtrlWordWrite_Map(0x830000)
	TC0100SCN0CtrlWordWrite_Map(0x920000)

	switch (a)
	{
		case 0xd00000:
		case 0xd00002:
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), a, d);
}

static void __fastcall undrfire_main_write_byte(UINT32 a, UINT8 d)
{
	TC0100SCN0ByteWrite_Map(0x900000, 0x90ffff)

	switch (a)
	{
		case 0x400000:
		case 0x400001:
		case 0x400002:
		case 0x400003:
		{
			// leds
			bprintf (0, _T("WB: %5.5x, %2.2x\n"), a, d);
		}
		return;

		case 0x500000:	
			BurnWatchdogWrite();
		return;

		case 0x500001:
		case 0x500002:
		return;

		case 0x500003:
			EEPROMSetCSLine((~d & 0x10) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((d & 0x20) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit((d >> 6) & 1);
		return;

		case 0x500004:
			// coin counter / lockout
		return;

		case 0x500005:
		case 0x500006:
		case 0x500007:
		return;

		case 0x600000:
		case 0x600001:
		case 0x600002:
		case 0x600003:
		case 0x600004:
		case 0x600005:
		case 0x600006:
		case 0x600007:
			if (has_subcpu) {
				interrupt5_timer = 10;
			}
		return;

		case 0xc00000:
		case 0xc00001:
		case 0xc00002:
		case 0xc00003:
		case 0xc00004:
		case 0xc00005:
		case 0xc00006:
		case 0xc00007:
		return;

		case 0xd00000:
		case 0xd00001:
		case 0xd00002:
		case 0xd00003:
			// rotate_control_w
		return;
	}
	if (a != 0x500000) bprintf (0, _T("WB: %5.5x, %2.2x\n"), a, d);
}

static UINT32 __fastcall undrfire_main_read_long(UINT32 address)
{
	bprintf (0, _T("RL: %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall undrfire_main_read_word(UINT32 address)
{
	if ((address & 0xffffc0) == 0x830000) {
		return TC0480SCPCtrl[(address / 2) & 0x1f];
	}

	if ((address & 0xfffff0) == 0x920000) {
		return TC0100SCNCtrl[0][(address / 2) & 7];
	}

	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall undrfire_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x500000:
			return TaitoInput[0]; // inputs0

		case 0x500001:
			return TaitoInput[1]; // inputs1

		case 0x500002:
			return TaitoInput[2]; // inputs2

		case 0x500003:
			return (EEPROMRead() ? 0x80 : 0) | (nCurrentFrame & 1) | 0x7e;

		case 0x500004:
			return 0xff;

		case 0x500005:
			return 0xff;

		case 0x500006:
			return 0xff;

		case 0x500007:
			return TaitoInput[3]; // system

		case 0x600000:
				return (has_subcpu) ? ProcessAnalog(TaitoAnalogPort0, 1, INPUT_DEADZONE, 0x00, 0xff) : 0;
		case 0x600001:
		case 0x600002:
		case 0x600003:
		case 0x600004:
		case 0x600005:
		case 0x600006:
		case 0x600007: // cbombers steering (unused bytes)
			return 0;

		case 0xc00000:
		case 0xc00001:
		case 0xc00002:
		case 0xc00003:
		case 0xc00004:
		case 0xc00005:
		case 0xc00006:
		case 0xc00007: // nop
			return 0;
	}

	if ((address & 0xfffff8) == 0xf00000) {
		if (has_subcpu) return 0;

		INT32 x = (ReloadGun[(address & 7) >> 2]) ? 0xff : 0xff - BurnGunReturnX((address & 7) >> 2);
		INT32 y = (ReloadGun[(address & 7) >> 2]) ? 0x00 : BurnGunReturnY((address & 7) >> 2);
		UINT32 res = ((x << 30) & 0xff000000) | ((x << 14) & 0xff0000) | ((y << 14) & 0xff00) | ((y >> 2) & 0xff);

		return res >> (8 * (~address & 0x3));
	}

	bprintf (0, _T("RB: %5.5x\n"), address);
	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (TaitoRamStart, 0, TaitoRamEnd - TaitoRamStart);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	SekOpen(2);
	SekReset();
	SekClose();

	TaitoICReset();
	TaitoF3SoundReset();

	BurnWatchdogReset();

	BurnShiftReset();

	EEPROMReset();
	if (EEPROMAvailable() == 0) {
		EEPROMFill(TaitoDefaultEEProm, 0, 0x80);
	}

	subcpu_in_reset = 0;
	interrupt5_timer = -1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1		= Next; Next += 0x200000;

	Taito68KRom2		= Next;
	TaitoF3SoundRom		= Next; Next += 0x100000;

	Taito68KRom3		= Next; Next += 0x040000;

	TaitoSpritesA		= Next; Next += 0x2000000;
	TaitoChars			= Next; Next += 0x800000;
	TaitoCharsPivot		= Next; Next += 0x800000;

	TaitoSpriteMapRom	= Next; Next += 0x100000;

	TaitoDefaultEEProm	= Next; Next += 0x000080;

	TaitoES5505Rom		= Next;
	TaitoF3ES5506Rom	= Next; Next += 0x1000000;

	TaitoPalette		= (UINT32*)Next; Next += 0x4000 * sizeof(UINT32);

	TaitoF2SpriteList	= (TaitoF2SpriteEntry*)Next; Next += 0x4000 * sizeof(TaitoF2SpriteEntry);

	TaitoRamStart		= Next;

	TaitoSharedRam		= Next; Next += 0x010000;
	TaitoSpriteRam		= Next; Next += 0x004000;
	TaitoSpriteRam2		= Next; Next += 0x000400;
	Taito68KRam1		= Next; Next += 0x020000;
	Taito68KRam3		= Next; Next += 0x010000;
	TaitoPaletteRam		= Next; Next += 0x010000;

//	TC0100SCNRam[0]		= Next; Next += 0x010000; // allocated in init
//	TC0480SCPRam		= Next; Next += 0x010000; // allocated in init
	
	TaitoF3SoundRam		= Next; Next += 0x010000;	// 64 KB
	TaitoF3SharedRam	= Next; Next += 0x000800;	// 2 KB
	TaitoES5510DSPRam	= Next; Next += 0x000200;	// 512 Bytes
	TaitoES5510GPR		= (UINT32 *)Next; Next += 0x000300;	// 192x4 Bytes
	TaitoES5510DRAM		= (UINT16 *)Next; Next += 0x400000;	// 4 MB

	TaitoRamEnd		= Next;

	TaitoMemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 sprite_size)
{
	INT32 Plane0[4]  = { STEP4(0,1) };
	INT32 XOffs0[16] = { 1*4, 0*4, 5*4, 4*4, 3*4, 2*4, 7*4, 6*4, 9*4, 8*4, 13*4, 12*4, 11*4, 10*4, 15*4, 14*4 };
	INT32 YOffs0[16] = { STEP16(0,64) };

	INT32 Plane1[5]  = { (sprite_size/2)*8, STEP4(0,8) };
	INT32 XOffs1[16] = { STEP8(32,1), STEP8(0,1) };
	INT32 YOffs1[16] = { STEP16(0,64) };

	INT32 Plane2[6] = { 0x200000*8, 0x200000*8+1, STEP4(0,1) };
	INT32 XOffs2[8] = { 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 };
	INT32 YOffs2[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(sprite_size);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, TaitoChars, 0x400000);

	GfxDecode(0x08000, 4, 16, 16, Plane0, XOffs0, YOffs0, 0x400, tmp, TaitoChars);

	memcpy (tmp, TaitoSpritesA, sprite_size);

	GfxDecode(sprite_size / (16*16), 5, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, TaitoSpritesA);

	memcpy (tmp, TaitoCharsPivot, 0x400000);

	GfxDecode(0x10000, 6,  8,  8, Plane2, XOffs2, YOffs2, 0x100, tmp, TaitoCharsPivot);

	BurnFree(tmp);

	return 0;
}

static void DrvGfxReorder(INT32 size)
{
	UINT32 offset = size/2;

	for (INT32 i = size/2+size/4; i < size; i++)
	{
		INT32 data = TaitoCharsPivot[i];
		INT32 d1 = (data >> 0) & 3;
		INT32 d2 = (data >> 2) & 3;
		INT32 d3 = (data >> 4) & 3;
		INT32 d4 = (data >> 6) & 3;

		TaitoCharsPivot[offset] = (d1<<2) | (d2<<6);
		offset++;

		TaitoCharsPivot[offset] = (d3<<2) | (d4<<6);
		offset++;
	}
}

static INT32 CommonInit(INT32 game_select)
{
	TaitoMem = NULL;
	MemIndex();
	INT32 nLen = TaitoMemEnd - (UINT8 *)0;
	if ((TaitoMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(TaitoMem, 0, nLen);
	MemIndex();

	if (game_select == 0) // under fire
	{
		INT32 k = 0;
		if (BurnLoadRom(Taito68KRom1 + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000002, k++, 4)) return 1;

		if (BurnLoadRom(TaitoF3SoundRom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3SoundRom + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(TaitoChars + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoChars + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoSpritesA + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800000, k++, 4)) return 1;

		if (BurnLoadRom(TaitoCharsPivot + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x300000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoSpriteMapRom + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoF3ES5506Rom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0xc00001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoDefaultEEProm  + 0x000000, k++, 1)) return 1;

		DrvGfxReorder(0x400000);
		DrvGfxDecode(0x1000000);
	}
	else if (game_select == 1) // chase bomber
	{
		INT32 k = 0;
		if (BurnLoadRom(Taito68KRom1 + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000002, k++, 4)) return 1;

		if (BurnLoadRom(TaitoF3SoundRom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3SoundRom + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(Taito68KRom3 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Taito68KRom3 + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoChars + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoChars + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoSpritesA + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800003, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800002, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800001, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0xc00000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x1400000, k++, 4)) return 1;

		if (BurnLoadRom(TaitoCharsPivot + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x300000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoSpriteMapRom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoSpriteMapRom + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoSpriteMapRom + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoF3ES5506Rom + 0xc00001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x000001, k,   2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x400001, k++, 2)) return 1; // reload
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x800001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoDefaultEEProm  + 0x000000, k++, 1)) return 1;

		DrvGfxReorder(0x400000);
		DrvGfxDecode(0x1800000);
	}
	else if (game_select == 2) // chase bomber prototype
	{
		INT32 k = 0;
		if (BurnLoadRom(Taito68KRom1 + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000002, k++, 4)) return 1;

		if (BurnLoadRom(TaitoF3SoundRom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3SoundRom + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(Taito68KRom3 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Taito68KRom3 + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(TaitoChars + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoChars + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(TaitoChars + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(TaitoChars + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(TaitoChars + 0x200000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoChars + 0x200001, k++, 4)) return 1;
		if (BurnLoadRom(TaitoChars + 0x200002, k++, 4)) return 1;
		if (BurnLoadRom(TaitoChars + 0x200003, k++, 4)) return 1;

		if (BurnLoadRom(TaitoSpritesA + 0x000003, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000002, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000001, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000000, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000007, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000006, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000005, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000004, k++, 8)) return 1;

		if (BurnLoadRom(TaitoSpritesA + 0xc00000, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0xc00004, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400003, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400002, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400001, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400000, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400007, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400006, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400005, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x400004, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x1000000, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x1000004, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800003, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800002, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800001, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800000, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800007, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800006, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800005, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x800004, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x1400000, k++, 8)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x1400004, k++, 8)) return 1;

		if (BurnLoadRom(TaitoCharsPivot + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x300000, k++, 1)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x100001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x100000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x380000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoSpriteMapRom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoSpriteMapRom + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoSpriteMapRom + 0x080000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoF3ES5506Rom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x100001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x300001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x400001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0xe00001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0xf00001, k++, 2)) return 1;

		DrvGfxReorder(0x400000);
		DrvGfxDecode(0x1800000);
	}

	GenericTilesInit();
	TC0100SCNInit(0, 0x10000, 50, 24, 0, NULL);
	TC0100SCNSetColourDepth(0, 6);
	TC0480SCPInit(0x8000, 0, 36, 0, -1, 0, 24);
	TC0480SCPSetColourBase(game_select ? (0x1000 >> 4) : 0);
	TC0480SCPSetPriMap(pPrioDraw);

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,			0x000000, 0x1fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(TaitoSpriteRam,		0x300000, 0x303fff, MAP_RAM);
	SekMapMemory(TaitoF3SharedRam,		0x700000, 0x7007ff, MAP_RAM);
	SekMapMemory(TC0480SCPRam,			0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(TC0100SCNRam[0],		0x900000, 0x90ffff, MAP_READ);
	SekMapMemory(TaitoPaletteRam,		0xa00000, 0xa0ffff, MAP_RAM);
	SekMapMemory(TaitoSpriteRam2,		0xb00000, 0xb003ff, MAP_RAM); // Unknown
	SekMapMemory(TaitoSharedRam,		0xe00000, 0xe0ffff, MAP_RAM); // not on undrfire
	SekSetWriteLongHandler(0,			undrfire_main_write_long);
	SekSetWriteWordHandler(0,			undrfire_main_write_word);
	SekSetWriteByteHandler(0,			undrfire_main_write_byte);
	SekSetReadLongHandler(0,			undrfire_main_read_long);
	SekSetReadWordHandler(0,			undrfire_main_read_word);
	SekSetReadByteHandler(0,			undrfire_main_read_byte);
	SekClose();

	TaitoF3ES5506RomSize = 0x1000000;
	TaitoF3SoundInit(1); // 68k #1 initialized here
	TaitoF3SoundIRQConfig(1);

	has_subcpu = (game_select == 0) ? 0 : 1;

	SekInit(2, 0x68000);
	SekOpen(2);
	SekMapMemory(Taito68KRom3,			0x000000, 0x03ffff, MAP_ROM);
	SekMapMemory(Taito68KRam3,			0x400000, 0x40ffff, MAP_RAM);
	SekMapMemory(TaitoSharedRam,		0x800000, 0x80ffff, MAP_RAM); // not on undrfire
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	EEPROMInit(&eeprom_interface_93C46);
	EEPROMIgnoreErrMessage(1); // chase bombers!

	BurnShiftInit(SHIFT_POSITION_BOTTOM_RIGHT, SHIFT_COLOR_GREEN, 80); // cbombers
	BurnGunInit(2, true); // undrfire

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	EEPROMExit();

	TaitoF3SoundExit();
	TaitoICExit();

	BurnShiftExit();
	BurnGunExit();

	BurnFree(TaitoMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT32 *pal = (UINT32*)TaitoPaletteRam;

	for (INT32 i = 0; i < 0x10000/4; i++)
	{
		UINT32 color = pal[i];
		color = (color << 16) | (color >> 16);

		INT32 r = color >> 16;
		INT32 g = color >> 8;
		INT32 b = color;

		TaitoPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 *primasks,INT32 x_offs,INT32 y_offs)
{
	UINT32 *spriteram32 = (UINT32*)TaitoSpriteRam;
	UINT16 *spritemap = (UINT16*)TaitoSpriteMapRom;
	INT32 offs, tilenum, color, flipx, flipy;
	INT32 x, y, priority, dblsize, curx, cury;
	INT32 sprites_flipscreen = 0;
	INT32 zoomx, zoomy, zx, zy;
	INT32 sprite_chunk,map_offset,code,j,k,px,py;
	INT32 dimension,total_chunks;

	struct TaitoF2SpriteEntry *sprite_ptr = &TaitoF2SpriteList[0];

	for (offs = (0x4000/4-4);offs >= 0;offs -= 4)
	{
		UINT32 data = spriteram32[offs+0];
		data = (data << 16) | (data >> 16);
		flipx =    (data & 0x00800000) >> 23;
		zoomx =    (data & 0x007f0000) >> 16;
		tilenum =  (data & 0x00007fff);

		data = spriteram32[offs+2];
		data = (data << 16) | (data >> 16);
		priority = (data & 0x000c0000) >> 18;
		color =    (data & 0x0003fc00) >> 10;
		x =        (data & 0x000003ff);

		data = spriteram32[offs+3];
		data = (data << 16) | (data >> 16);
		dblsize =  (data & 0x00040000) >> 18;
		flipy =    (data & 0x00020000) >> 17;
		zoomy =    (data & 0x0001fc00) >> 10;
		y =        (data & 0x000003ff);

		color |= (0x100 + (priority << 6));     /* priority bits select color bank */
		color /= 2;     /* as sprites are 5bpp */
		flipy = !flipy;
		y = (-y &0x3ff);

		if (!tilenum) continue;

		flipy = !flipy;
		zoomx += 1;
		zoomy += 1;

		y += y_offs;

		if (x>0x340) x -= 0x400;
		if (y>0x340) y -= 0x400;

		x -= x_offs;

		dimension = ((dblsize*2) + 2);  // 2 or 4
		total_chunks = ((dblsize*3) + 1) << 2;  // 4 or 16
		map_offset = tilenum << 2;

		{
			for (sprite_chunk=0;sprite_chunk<total_chunks;sprite_chunk++)
			{
				j = sprite_chunk / dimension;   /* rows */
				k = sprite_chunk % dimension;   /* chunks per row */

				px = k;
				py = j;
				/* pick tiles back to front for x and y flips */
				if (flipx)  px = dimension-1-k;
				if (flipy)  py = dimension-1-j;

				code = spritemap[map_offset + px + (py<<(dblsize+1))];

				if (code==0xffff)
				{
					continue;
				}

				curx = x + ((k*zoomx)/dimension);
				cury = y + ((j*zoomy)/dimension);

				zx= x + (((k+1)*zoomx)/dimension) - curx;
				zy= y + (((j+1)*zoomy)/dimension) - cury;

				if (sprites_flipscreen)
				{
					curx = 320 - curx - zx;
					cury = 256 - cury - zy;
					flipx = !flipx;
					flipy = !flipy;
				}

				sprite_ptr->Code = code;
				sprite_ptr->Colour = color << 5;
				sprite_ptr->xFlip = !flipx;
				sprite_ptr->yFlip = flipy;
				sprite_ptr->x = curx;
				sprite_ptr->y = cury - 0;
				sprite_ptr->xZoom = zx << 12;
				sprite_ptr->yZoom = zy << 12;
				sprite_ptr->Priority = priority;
				sprite_ptr++;
			}
		}
	}

	while (sprite_ptr != &TaitoF2SpriteList[0])
	{
		sprite_ptr--;

		RenderZoomedPrioSprite(pTransDraw, TaitoSpritesA, 
			sprite_ptr->Code,
			sprite_ptr->Colour, 0,
			sprite_ptr->x, sprite_ptr->y-24,
			sprite_ptr->xFlip,sprite_ptr->yFlip,
			16, 16,
			sprite_ptr->xZoom,sprite_ptr->yZoom, primasks[sprite_ptr->Priority]);
	}
}

static void draw_sprites_cbombers(const INT32 *primasks,INT32 x_offs,INT32 y_offs)
{
	UINT32 *spriteram32 = (UINT32*)TaitoSpriteRam;
	UINT16 *spritemap = (UINT16*)TaitoSpriteMapRom;
	UINT8 *spritemapHibit = TaitoSpriteMapRom + 0x80000;

	INT32 offs, tilenum, color, flipx, flipy;
	INT32 x, y, priority, dblsize, curx, cury;
	INT32 sprites_flipscreen = 0;
	INT32 zoomx, zoomy, zx, zy;
	INT32 sprite_chunk,map_offset,code,j,k,px,py;
	INT32 dimension,total_chunks;

	struct TaitoF2SpriteEntry *sprite_ptr = &TaitoF2SpriteList[0];

	for (offs = (0x4000/4-4);offs >= 0;offs -= 4)
	{
		UINT32 data = spriteram32[offs+0];
		data = (data << 16) | (data >> 16);
		flipx =    (data & 0x00800000) >> 23;
		zoomx =    (data & 0x007f0000) >> 16;
		tilenum =  (data & 0x0000ffff);

		data = spriteram32[offs+2];
		data = (data << 16) | (data >> 16);
		priority = (data & 0x000c0000) >> 18;
		color =    (data & 0x0003fc00) >> 10;
		x =        (data & 0x000003ff);

		data = spriteram32[offs+3];
		data = (data << 16) | (data >> 16);
		dblsize =  (data & 0x00040000) >> 18;
		flipy =    (data & 0x00020000) >> 17;
		zoomy =    (data & 0x0001fc00) >> 10;
		y =        (data & 0x000003ff);

		color |= (/*0x100 +*/ (priority << 6));     /* priority bits select color bank */

		color /= 2;     /* as sprites are 5bpp */
		flipy = !flipy;

		if (!tilenum) continue;

		zoomx += 1;
		zoomy += 1;

		y += y_offs;

		/* treat coords as signed */
		if (x>0x340) x -= 0x400;
		if (y>0x340) y -= 0x400;

		x -= x_offs;

		dimension = ((dblsize*2) + 2);  // 2 or 4
		total_chunks = ((dblsize*3) + 1) << 2;  // 4 or 16
		map_offset = tilenum << 2;

		for (sprite_chunk = 0; sprite_chunk < total_chunks; sprite_chunk++)
		{
			INT32 map_addr;

			j = sprite_chunk / dimension;   /* rows */
			k = sprite_chunk % dimension;   /* chunks per row */

			px = k;
			py = j;
			/* pick tiles back to front for x and y flips */
			if (flipx)  px = dimension-1-k;
			if (flipy)  py = dimension-1-j;

			map_addr = map_offset + px + (py << (dblsize + 1));
			code =  (spritemapHibit[map_addr] << 16) | spritemap[map_addr];

			curx = x + ((k*zoomx)/dimension);
			cury = y + ((j*zoomy)/dimension);

			zx= x + (((k+1)*zoomx)/dimension) - curx;
			zy= y + (((j+1)*zoomy)/dimension) - cury;

			if (sprites_flipscreen)
			{
				curx = 320 - curx - zx;
				cury = 256 - cury - zy;
				flipx = !flipx;
				flipy = !flipy;
			}

			sprite_ptr->Code = code;
			sprite_ptr->Colour = color;
			sprite_ptr->xFlip = !flipx;
			sprite_ptr->yFlip = flipy;
			sprite_ptr->x = curx;
			sprite_ptr->y = cury;
			sprite_ptr->xZoom = zx << 12;
			sprite_ptr->yZoom = zy << 12;
			sprite_ptr->Priority = priority;
			sprite_ptr++;
		}
	}

	while (sprite_ptr != &TaitoF2SpriteList[0])
	{
		sprite_ptr--;

		RenderZoomedPrioSprite(pTransDraw, TaitoSpritesA, 
			sprite_ptr->Code & 0x1ffff,
			sprite_ptr->Colour << 5, 0,
			sprite_ptr->x, sprite_ptr->y-24,
			sprite_ptr->xFlip,sprite_ptr->yFlip,
			16, 16,
			sprite_ptr->xZoom,sprite_ptr->yZoom, primasks[sprite_ptr->Priority]);
	}
}

static INT32 UndrfireDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
//	}

	UINT8 layer[5];
	UINT16 priority = TC0480SCPGetBgPriority();

	layer[0] = (priority & 0xf000) >> 12;
	layer[1] = (priority & 0x0f00) >>  8;
	layer[2] = (priority & 0x00f0) >>  4;
	layer[3] = (priority & 0x000f) >>  0;
	layer[4] = 4;

	BurnTransferClear();

	INT32 Disable = TC0100SCNCtrl[0][6] & 0x3;
	if (TC0100SCNBottomLayer(0)) {
		if (nSpriteEnable & 8) if (!(Disable & 0x02)) TC0100SCNRenderFgLayer(0, 1, TaitoCharsPivot);
		if (nSpriteEnable & 4) if (!(Disable & 0x01)) TC0100SCNRenderBgLayer(0, 0, TaitoCharsPivot);
	} else {
		if (nSpriteEnable & 4) if (!(Disable & 0x01)) TC0100SCNRenderBgLayer(0, 1, TaitoCharsPivot);
		if (nSpriteEnable & 8) if (!(Disable & 0x02)) TC0100SCNRenderFgLayer(0, 0, TaitoCharsPivot);
	}

	if (nBurnLayer & 1) TC0480SCPTilemapRenderPrio(layer[0], 0, 1, TaitoChars);
	if (nBurnLayer & 2) TC0480SCPTilemapRenderPrio(layer[1], 0, 2, TaitoChars);
	if (nBurnLayer & 4) TC0480SCPTilemapRenderPrio(layer[2], 0, 4, TaitoChars);
	if (nBurnLayer & 8) TC0480SCPTilemapRenderPrio(layer[3], 0, 8, TaitoChars);

	if ((TC0480SCPCtrl[0xf] & 0x3) == 3) // priority control
	{
		INT32 primasks[4] = {0xfff0, 0xff00, 0x0000, 0x0};

		if (nSpriteEnable & 16) draw_sprites(primasks, 44, -574);
	}
	else
	{
		INT32 primasks[4] = {0xfffc, 0xfff0, 0xff00, 0x0};

		if (nSpriteEnable & 16) draw_sprites(primasks, 44, -574);
	}

	if (nSpriteEnable & 4) TC0100SCNRenderCharLayer(0);
	if (nSpriteEnable & 8) TC0480SCPRenderCharLayer();

	BurnTransferCopy(TaitoPalette);
	BurnGunDrawTargets();
	return 0;
}

static INT32 CbombersDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
//	}


	UINT8 layer[5];
	UINT16 priority = TC0480SCPGetBgPriority();

	layer[0] = (priority & 0xf000) >> 12;
	layer[1] = (priority & 0x0f00) >>  8;
	layer[2] = (priority & 0x00f0) >>  4;
	layer[3] = (priority & 0x000f) >>  0;
	layer[4] = 4;

	BurnTransferClear();

	if (TC0100SCNBottomLayer(0)) {
		if (nSpriteEnable & 2) TC0100SCNRenderFgLayer(0, 1, TaitoCharsPivot);
		if (nSpriteEnable & 1) TC0100SCNRenderBgLayer(0, 0, TaitoCharsPivot);
	} else {
		if (nSpriteEnable & 1) TC0100SCNRenderBgLayer(0, 1, TaitoCharsPivot);
		if (nSpriteEnable & 2) TC0100SCNRenderFgLayer(0, 0, TaitoCharsPivot);
	}

	if (nBurnLayer & 1) TC0480SCPTilemapRenderPrio(layer[0], 0, 1, TaitoChars);
	if (nBurnLayer & 2) TC0480SCPTilemapRenderPrio(layer[1], 0, 2, TaitoChars);
	if (nBurnLayer & 4) TC0480SCPTilemapRenderPrio(layer[2], 0, 4, TaitoChars);
	if (nBurnLayer & 8) TC0480SCPTilemapRenderPrio(layer[3], 0, 8, TaitoChars);

	if ((TC0480SCPCtrl[0xf] & 0x3) == 3) // priority control
	{
		INT32 primasks[4] = {0xfff0, 0xff00, 0x0, 0x0};
		if (nSpriteEnable & 16) draw_sprites_cbombers(primasks, 80, -208);
	}
	else
	{
		INT32 primasks[4] = {0xfffc, 0xfff0, 0xff00, 0x0};
		if (nSpriteEnable & 16) draw_sprites_cbombers(primasks, 80, -208);
	}

	if (nSpriteEnable & 4) TC0100SCNRenderCharLayer(0);
	if (nSpriteEnable & 8) TC0480SCPRenderCharLayer();

	BurnTransferCopy(TaitoPalette);

	//BurnShiftRender(); (game has its own shift indicator)

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

//	SekNewFrame();

	if (TaitoReset) {
		DrvDoReset(1);
	}

	{
		TaitoInput[0] = 0xff;
		TaitoInput[1] = 0xf7;
		TaitoInput[2] = 0xff;
		TaitoInput[3] = 0xff;

		if (ReloadGun[0]) {
			TaitoInputPort1[4] = 1;
		}
		if (ReloadGun[1]) {
			TaitoInputPort1[6] = 1;
		}

		for (INT32 i = 0; i < 8; i++) {
			TaitoInput[0] ^= (TaitoInputPort0[i] & 1) << i;
			TaitoInput[1] ^= (TaitoInputPort1[i] & 1) << i;
			TaitoInput[2] ^= (TaitoInputPort2[i] & 1) << i;
			TaitoInput[3] ^= (TaitoInputPort3[i] & 1) << i;
		}

		{
			if (has_subcpu) {
				// gear shifter stuff (cbombers)
				BurnShiftInputCheckToggle(TaitoInputPort0[0]);

				TaitoInput[0] &= ~1;
				TaitoInput[0] |= !bBurnShiftStatus;
			} else {
				// lightgun stuff (undrfire)
				BurnGunMakeInputs(0, TaitoAnalogPort0, TaitoAnalogPort1);
				BurnGunMakeInputs(1, TaitoAnalogPort2, TaitoAnalogPort3);
			}
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 20000000 / 60, 16000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if (i == (nInterleave - 1))
		{
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}
		else if (interrupt5_timer >= 0)
		{
			if (interrupt5_timer == 0)
			{
				SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
			}

			interrupt5_timer--;
		}

		SekClose();

		TaitoF3CpuUpdate(nInterleave, i);

		if (has_subcpu) { // cbombers only
			SekOpen(2);

			if (subcpu_in_reset == 0)
			{
				nCyclesDone[1] += SekRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

				if (i == (nInterleave - 1))
				{
					SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
				}
			}
			else
			{
				SekIdle(nCyclesTotal[1] / nInterleave);
			}

			SekClose();
		}
	}

	if (pBurnSoundOut) {
		TaitoF3SoundUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
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

		ba.Data	  = TaitoRamStart;
		ba.nLen	  = TaitoRamEnd - TaitoRamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		TaitoICScan(nAction);
		TaitoF3SoundScan(nAction, pnMin);
		EEPROMScan(nAction, pnMin);
		BurnWatchdogScan(nAction);
		BurnShiftScan(nAction);
		BurnGunScan();

		SCAN_VAR(subcpu_in_reset);
		SCAN_VAR(interrupt5_timer);
	}

	return 0;
}


// Under Fire (World)

static struct BurnRomInfo undrfireRomDesc[] = {
	{ "d67-19",					0x080000, 0x1d88fa5a, TAITO_68KROM1_BYTESWAP32 },	//  0 M68EC020 Code
	{ "d67-18",					0x080000, 0xf41ae7fd, TAITO_68KROM1_BYTESWAP32 },	//  1
	{ "d67-17",					0x080000, 0x34e030b7, TAITO_68KROM1_BYTESWAP32 },	//  2
	{ "d67-23",					0x080000, 0x28e84e0a, TAITO_68KROM1_BYTESWAP32 },	//  3

	{ "d67-20",					0x020000, 0x974ebf69, TAITO_68KROM2_BYTESWAP },		//  4 Sound 68K Code
	{ "d67-21",					0x020000, 0x8fc6046f, TAITO_68KROM2_BYTESWAP },		//  5

	{ "d67-08",					0x200000, 0x56730d44, TAITO_CHARS_BYTESWAP },		//  6 Background Tiles
	{ "d67-09",					0x200000, 0x3c19f9e3, TAITO_CHARS_BYTESWAP },		//  7

	{ "d67-03",					0x200000, 0x3b6e99a9, TAITO_SPRITESA_BYTESWAP32 },	//  8 Sprites
	{ "d67-04",					0x200000, 0x8f2934c9, TAITO_SPRITESA_BYTESWAP32 },	//  9
	{ "d67-05",					0x200000, 0xe2e7dcf3, TAITO_SPRITESA_BYTESWAP32 },	// 10
	{ "d67-06",					0x200000, 0xa2a63488, TAITO_SPRITESA_BYTESWAP32 },	// 11
	{ "d67-07",					0x200000, 0x189c0ee5, TAITO_SPRITESA },				// 12

	{ "d67-10",					0x100000, 0xd79e6ce9, TAITO_CHARS_PIVOT },			// 13 PIV Tiles
	{ "d67-11",					0x100000, 0x7a401bb3, TAITO_CHARS_PIVOT },			// 14
	{ "d67-12",					0x100000, 0x67b16fec, TAITO_CHARS_PIVOT },			// 15

	{ "d67-13",					0x080000, 0x42e7690d, TAITO_SPRITEMAP },			// 16 Sprite Map

	{ "d67-01",					0x200000, 0xa2f18122, TAITO_ES5505_BYTESWAP },		// 17 Ensoniq Samples/Data
	{ "d67-02",					0x200000, 0xfceb715e, TAITO_ES5505_BYTESWAP },		// 18

	{ "eeprom-undrfire.bin",	0x000080, 0x9f7368f4, TAITO_DEFAULT_EEPROM },		// 19 Default EEPROM
};

STD_ROM_PICK(undrfire)
STD_ROM_FN(undrfire)

static INT32 UndrfireInit()
{
	return CommonInit(0);
}

struct BurnDriver BurnDrvUndrfire = {
	"undrfire", NULL, NULL, NULL, "1993",
	"Under Fire (World)\0", NULL, "Taito Corporation Japan", "K1100744A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, undrfireRomInfo, undrfireRomName, NULL, NULL, NULL, NULL, UndrfireInputInfo, NULL,
	UndrfireInit, DrvExit, DrvFrame, UndrfireDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 232, 4, 3
};


// Under Fire (Japan)

static struct BurnRomInfo undrfirejRomDesc[] = {
	{ "d67-19",					0x080000, 0x1d88fa5a, TAITO_68KROM1_BYTESWAP32 },	//  0 M68EC020 Code
	{ "d67-18",					0x080000, 0xf41ae7fd, TAITO_68KROM1_BYTESWAP32 },	//  1
	{ "d67-17",					0x080000, 0x34e030b7, TAITO_68KROM1_BYTESWAP32 },	//  2
	{ "d67-16",					0x080000, 0xc6e62f26, TAITO_68KROM1_BYTESWAP32 },	//  3

	{ "d67-20",					0x020000, 0x974ebf69, TAITO_68KROM2_BYTESWAP },		//  4 Sound 68K Code
	{ "d67-21",					0x020000, 0x8fc6046f, TAITO_68KROM2_BYTESWAP },		//  5

	{ "d67-08",					0x200000, 0x56730d44, TAITO_CHARS_BYTESWAP },		//  6 Background Tiles
	{ "d67-09",					0x200000, 0x3c19f9e3, TAITO_CHARS_BYTESWAP },		//  7

	{ "d67-03",					0x200000, 0x3b6e99a9, TAITO_SPRITESA_BYTESWAP32 },	//  8 Sprites
	{ "d67-04",					0x200000, 0x8f2934c9, TAITO_SPRITESA_BYTESWAP32 },	//  9
	{ "d67-05",					0x200000, 0xe2e7dcf3, TAITO_SPRITESA_BYTESWAP32 },	// 10
	{ "d67-06",					0x200000, 0xa2a63488, TAITO_SPRITESA_BYTESWAP32 },	// 11
	{ "d67-07",					0x200000, 0x189c0ee5, TAITO_SPRITESA },				// 12

	{ "d67-10",					0x100000, 0xd79e6ce9, TAITO_CHARS_PIVOT },			// 13 PIV Tiles
	{ "d67-11",					0x100000, 0x7a401bb3, TAITO_CHARS_PIVOT },			// 14
	{ "d67-12",					0x100000, 0x67b16fec, TAITO_CHARS_PIVOT },			// 15

	{ "d67-13",					0x080000, 0x42e7690d, TAITO_SPRITEMAP },			// 16 Sprite Map

	{ "d67-01",					0x200000, 0xa2f18122, TAITO_ES5505_BYTESWAP },		// 17 Ensoniq Samples/Data
	{ "d67-02",					0x200000, 0xfceb715e, TAITO_ES5505_BYTESWAP },		// 18

	{ "eeprom-undrfire.bin",	0x000080, 0x9f7368f4, TAITO_DEFAULT_EEPROM },		// 19 Default EEPROM
};

STD_ROM_PICK(undrfirej)
STD_ROM_FN(undrfirej)

struct BurnDriver BurnDrvUndrfirej = {
	"undrfirej", "undrfire", NULL, NULL, "1993",
	"Under Fire (Japan)\0", NULL, "Taito Corporation", "K1100744A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, undrfirejRomInfo, undrfirejRomName, NULL, NULL, NULL, NULL, UndrfireInputInfo, NULL,
	UndrfireInit, DrvExit, DrvFrame, UndrfireDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 232, 4, 3
};


// Chase Bombers (World)

static struct BurnRomInfo cbombersRomDesc[] = {
	{ "d83_39.ic17",			0x080000, 0xb9f48284, TAITO_68KROM1_BYTESWAP32 },	//  0 M68EC020 Code
	{ "d83_41.ic4",				0x080000, 0xa2f4c8be, TAITO_68KROM1_BYTESWAP32 },	//  1
	{ "d83_40.ic3",				0x080000, 0xb05f59ea, TAITO_68KROM1_BYTESWAP32 },	//  2
	{ "d83_38.ic16",			0x080000, 0x0a10616c, TAITO_68KROM1_BYTESWAP32 },	//  3

	{ "d83_26.ic37",			0x020000, 0x4f49b484, TAITO_68KROM2_BYTESWAP },		//  4 Sound 68K Code
	{ "d83_27.ic38",			0x020000, 0x2aa1a237, TAITO_68KROM2_BYTESWAP },		//  5

	{ "d83_28.ic26",			0x020000, 0x06328ef7, TAITO_68KROM3_BYTESWAP },		//  6 Sub 68k Code
	{ "d83_29.ic27",			0x020000, 0x771b4080, TAITO_68KROM3_BYTESWAP },		//  7

	{ "d83_04.ic8",				0x200000, 0x79f36cce, TAITO_CHARS_BYTESWAP },		//  8 Background Tiles
	{ "d83_05.ic7",				0x200000, 0x7787e495, TAITO_CHARS_BYTESWAP },		//  9

	{ "d83_06.ic28",			0x200000, 0x4b71944e, TAITO_SPRITESA_BYTESWAP32 },	// 10 Sprites
	{ "d83_07.ic30",			0x200000, 0x29861b61, TAITO_SPRITESA_BYTESWAP32 },	// 11
	{ "d83_08.ic32",			0x200000, 0xa0e81e01, TAITO_SPRITESA_BYTESWAP32 },	// 12
	{ "d83_09.ic45",			0x200000, 0x7e4dec50, TAITO_SPRITESA_BYTESWAP32 },	// 13
	{ "d83_11.ic41",			0x100000, 0xa790e490, TAITO_SPRITESA_BYTESWAP32 },	// 14
	{ "d83_12.ic29",			0x100000, 0x2f237b0d, TAITO_SPRITESA_BYTESWAP32 },	// 15
	{ "d83_13.ic31",			0x100000, 0xc2cceeb6, TAITO_SPRITESA_BYTESWAP32 },	// 16
	{ "d83_14.ic44",			0x100000, 0x8b6f4f12, TAITO_SPRITESA_BYTESWAP32 },	// 17
	{ "d83_10.ic43",			0x200000, 0x36c440a0, TAITO_SPRITESA },				// 18
	{ "d83_15.ic42",			0x100000, 0x1b71175e, TAITO_SPRITESA },				// 19

	{ "d83_16.ic19",			0x100000, 0xd364cf1e, TAITO_CHARS_PIVOT },			// 20 PIV Tiles
	{ "d83_17.ic5",				0x100000, 0x0ffe737c, TAITO_CHARS_PIVOT },			// 21
	{ "d83_18.ic6",				0x100000, 0x87979155, TAITO_CHARS_PIVOT },			// 22

	{ "d83_31.ic10",			0x040000, 0x85c37961, TAITO_SPRITEMAP },			// 23 Sprite Map
	{ "d83_32.ic11",			0x040000, 0xb0db2559, TAITO_SPRITEMAP },			// 24
	{ "d83_30.ic9",				0x040000, 0xeb86dc67, TAITO_SPRITEMAP },			// 25

	{ "d83_01.ic40",			0x200000, 0x912799f4, TAITO_ES5505_BYTESWAP },		// 26 Ensoniq Samples/Data
	{ "d83_02.ic39",			0x200000, 0x2abca020, TAITO_ES5505_BYTESWAP },		// 27
	{ "d83_03.ic18",			0x200000, 0x1b2d9ec3, TAITO_ES5505_BYTESWAP },		// 28

	{ "eeprom-cbombers.bin",	0x000080, 0x9f7368f4, TAITO_DEFAULT_EEPROM },		// 29 Defaults 
};

STD_ROM_PICK(cbombers)
STD_ROM_FN(cbombers)

static INT32 CbombersInit()
{
	return CommonInit(1);
}

struct BurnDriver BurnDrvCbombers = {
	"cbombers", NULL, NULL, NULL, "1994",
	"Chase Bombers (World)\0", "With graphics issues", "Taito Corporation Japan", "K1100809A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, cbombersRomInfo, cbombersRomName, NULL, NULL, NULL, NULL, CbombersInputInfo, NULL,
	CbombersInit, DrvExit, DrvFrame, CbombersDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 232, 4, 3
};


// Chase Bombers (Japan)

static struct BurnRomInfo cbombersjRomDesc[] = {
	{ "d83_34.ic17",			0x080000, 0x78e85cb1, TAITO_68KROM1_BYTESWAP32 },	//  0 M68EC020 Code
	{ "d83_36.ic4",				0x080000, 0xfdc936cf, TAITO_68KROM1_BYTESWAP32 },	//  1
	{ "d83_35.ic3",				0x080000, 0xb0379b1e, TAITO_68KROM1_BYTESWAP32 },	//  2
	{ "d83_33.ic16",			0x080000, 0x72fb42a7, TAITO_68KROM1_BYTESWAP32 },	//  3

	{ "d83_26.ic37",			0x020000, 0x4f49b484, TAITO_68KROM2_BYTESWAP },		//  4 Sound 68K Code
	{ "d83_27.ic38",			0x020000, 0x2aa1a237, TAITO_68KROM2_BYTESWAP },		//  5

	{ "d83_28.ic26",			0x020000, 0x06328ef7, TAITO_68KROM3_BYTESWAP },		//  6 Sub 68k Code
	{ "d83_29.ic27",			0x020000, 0x771b4080, TAITO_68KROM3_BYTESWAP },		//  7

	{ "d83_04.ic8",				0x200000, 0x79f36cce, TAITO_CHARS_BYTESWAP },		//  8 Background Tiles
	{ "d83_05.ic7",				0x200000, 0x7787e495, TAITO_CHARS_BYTESWAP },		//  9

	{ "d83_06.ic28",			0x200000, 0x4b71944e, TAITO_SPRITESA_BYTESWAP32 },	// 10 Sprites
	{ "d83_07.ic30",			0x200000, 0x29861b61, TAITO_SPRITESA_BYTESWAP32 },	// 11
	{ "d83_08.ic32",			0x200000, 0xa0e81e01, TAITO_SPRITESA_BYTESWAP32 },	// 12
	{ "d83_09.ic45",			0x200000, 0x7e4dec50, TAITO_SPRITESA_BYTESWAP32 },	// 13
	{ "d83_11.ic41",			0x100000, 0xa790e490, TAITO_SPRITESA_BYTESWAP32 },	// 14
	{ "d83_12.ic29",			0x100000, 0x2f237b0d, TAITO_SPRITESA_BYTESWAP32 },	// 15
	{ "d83_13.ic31",			0x100000, 0xc2cceeb6, TAITO_SPRITESA_BYTESWAP32 },	// 16
	{ "d83_14.ic44",			0x100000, 0x8b6f4f12, TAITO_SPRITESA_BYTESWAP32 },	// 17
	{ "d83_10.ic43",			0x200000, 0x36c440a0, TAITO_SPRITESA },				// 18
	{ "d83_15.ic42",			0x100000, 0x1b71175e, TAITO_SPRITESA },				// 19

	{ "d83_16.ic19",			0x100000, 0xd364cf1e, TAITO_CHARS_PIVOT },			// 20 PIV Tiles
	{ "d83_17.ic5",				0x100000, 0x0ffe737c, TAITO_CHARS_PIVOT },			// 21
	{ "d83_18.ic6",				0x100000, 0x87979155, TAITO_CHARS_PIVOT },			// 22

	{ "d83_31.ic10",			0x040000, 0x85c37961, TAITO_SPRITEMAP },			// 23 Sprite Map
	{ "d83_32.ic11",			0x040000, 0xb0db2559, TAITO_SPRITEMAP },			// 24
	{ "d83_30.ic9",				0x040000, 0xeb86dc67, TAITO_SPRITEMAP },			// 25

	{ "d83_01.ic40",			0x200000, 0x912799f4, TAITO_ES5505_BYTESWAP },		// 26 Ensoniq Samples/Data
	{ "d83_02.ic39",			0x200000, 0x2abca020, TAITO_ES5505_BYTESWAP },		// 27
	{ "d83_03.ic18",			0x200000, 0x1b2d9ec3, TAITO_ES5505_BYTESWAP },		// 28

	{ "eeprom-cbombers.bin",	0x000080, 0x9f7368f4, TAITO_DEFAULT_EEPROM },		// 29 Defaults 
};

STD_ROM_PICK(cbombersj)
STD_ROM_FN(cbombersj)

struct BurnDriver BurnDrvCbombersj = {
	"cbombersj", "cbombers", NULL, NULL, "1994",
	"Chase Bombers (Japan)\0", "With graphics issues", "Taito Corporation", "K1100809A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, cbombersjRomInfo, cbombersjRomName, NULL, NULL, NULL, NULL, CbombersInputInfo, NULL,
	CbombersInit, DrvExit, DrvFrame, CbombersDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 232, 4, 3
};


// Chase Bombers (Japan Prototype)

static struct BurnRomInfo cbomberspRomDesc[] = {
	{ "hh.bin",					0x080000, 0x8ee7b6c2, TAITO_68KROM1_BYTESWAP32 },	//  0 M68EC020 Code
	{ "hl.bin",					0x080000, 0x63683ed8, TAITO_68KROM1_BYTESWAP32 },	//  1
	{ "lh.bin",					0x080000, 0x77ad9acb, TAITO_68KROM1_BYTESWAP32 },	//  2
	{ "ll.bin",					0x080000, 0x54c060a5, TAITO_68KROM1_BYTESWAP32 },	//  3

	{ "ic47_eprg_h_up_c5c5.bin",0x020000, 0xcb0d11b1, TAITO_68KROM2_BYTESWAP },		//  4 Sound 68K Code
	{ "ic46_eprg_l_low_d5e.bin",0x020000, 0x567ae215, TAITO_68KROM2_BYTESWAP },		//  5

	{ "5-l.bin",				0x020000, 0xaed4c3c0, TAITO_68KROM3_BYTESWAP },		//  6 Sub 68k Code
	{ "5-h.bin",				0x020000, 0xc6ec60e4, TAITO_68KROM3_BYTESWAP },		//  7

	{ "scp0ll_ic7.bin",			0x080000, 0xb1af439d, TAITO_CHARS_BYTESWAP },		//  8 Background Tiles
	{ "scp0hl_ic9.bin",			0x080000, 0x5b6e413e, TAITO_CHARS_BYTESWAP },		//  9
	{ "scp0lh_ic22.bin",		0x080000, 0xd5109bca, TAITO_CHARS_BYTESWAP },		// 10
	{ "scp0aa_2b04_ic35.bin",	0x080000, 0xb8ec56bd, TAITO_CHARS_BYTESWAP },		// 11
	{ "scp1ll_ic8.bin",			0x080000, 0xeaa5839a, TAITO_CHARS_BYTESWAP },		// 12
	{ "scp1hl_ic24.bin",		0x080000, 0x46d198ba, TAITO_CHARS_BYTESWAP },		// 13
	{ "scp1lh_ic23.bin",		0x080000, 0x7c9f0035, TAITO_CHARS_BYTESWAP },		// 14
	{ "scp1hh_ic36.bin",		0x080000, 0x24f545d8, TAITO_CHARS_BYTESWAP },		// 15

	{ "obj0l_ic29.bin",			0x080000, 0x4b954950, TAITO_SPRITESA },				// 16 Sprites
	{ "obj16l_ic20.bin",		0x080000, 0xb53932c0, TAITO_SPRITESA },				// 17
	{ "obj32l_ic51.bin",		0x080000, 0xf23f7253, TAITO_SPRITESA },				// 18
	{ "obj48l_ic53.bin",		0x080000, 0x85bb6b75, TAITO_SPRITESA },				// 19
	{ "obj8l_ic4.bin",			0x080000, 0xd26140bb, TAITO_SPRITESA },				// 20
	{ "obj24l_ic50.bin",		0x080000, 0x27c76f27, TAITO_SPRITESA },				// 21
	{ "obj40l_ic52.bin",		0x080000, 0x09aaf6c5, TAITO_SPRITESA },				// 22
	{ "obj56l_ic54.bin",		0x080000, 0x8b6bacdf, TAITO_SPRITESA },				// 23
	{ "obj64l_ic55.bin",		0x080000, 0x18c976f1, TAITO_SPRITESA },				// 24
	{ "obj72l_ic56.bin",		0x080000, 0x6a1b5ebc, TAITO_SPRITESA },				// 25
	{ "obj0h_ic30.bin",			0x080000, 0x4c436ad2, TAITO_SPRITESA },				// 26
	{ "obj16h_ic21.bin",		0x080000, 0x5406b71e, TAITO_SPRITESA },				// 27
	{ "ic65_4b57.bin",			0x080000, 0x6a1a8054, TAITO_SPRITESA },				// 28
	{ "ic67_0956.bin",			0x080000, 0xabe445dd, TAITO_SPRITESA },				// 29
	{ "obj8l_ic5.bin",			0x080000, 0x46b028eb, TAITO_SPRITESA },				// 30
	{ "ic64_5aba.bin",			0x080000, 0x0912b766, TAITO_SPRITESA },				// 31
	{ "ic66_4ae9.bin",			0x080000, 0x77aafe1a, TAITO_SPRITESA },				// 32
	{ "ic68_1429.bin",			0x080000, 0x2e5857e5, TAITO_SPRITESA },				// 33
	{ "ic69_43b4.bin",			0x080000, 0xd08643be, TAITO_SPRITESA },				// 34
	{ "ic70_4504.bin",			0x080000, 0x3cf5d9d7, TAITO_SPRITESA },				// 35
	{ "obj0l_ic31.bin",			0x080000, 0x9a20d601, TAITO_SPRITESA },				// 36
	{ "obj16l_ic33.bin",		0x080000, 0xea9df360, TAITO_SPRITESA },				// 37
	{ "ic77_36ac.bin",			0x080000, 0x75628014, TAITO_SPRITESA },				// 38
	{ "ic79_ef40.bin",			0x080000, 0x6af34bbf, TAITO_SPRITESA },				// 39
	{ "obj8l_ic6.bin",			0x080000, 0x2037aad5, TAITO_SPRITESA },				// 40
	{ "ic76_443a.bin",			0x080000, 0xe5820610, TAITO_SPRITESA },				// 41
	{ "ic78_989c.bin",			0x080000, 0x23ec2896, TAITO_SPRITESA },				// 42
	{ "ic80_d511.bin",			0x080000, 0x37da5baf, TAITO_SPRITESA },				// 43
	{ "ic81_e150.bin",			0x080000, 0x48dbc4fb, TAITO_SPRITESA },				// 44
	{ "ic82_3d3d.bin",			0x080000, 0x3e62970e, TAITO_SPRITESA },				// 45

	{ "ic44_scc1.bin",			0x080000, 0x868d0d3d, TAITO_CHARS_PIVOT },			// 46 PIV Tiles
	{ "ic43_scc4.bin",			0x080000, 0x2f170ee4, TAITO_CHARS_PIVOT },			// 47
	{ "ic45_5cc2.bin",			0x080000, 0x7ae48d63, TAITO_CHARS_PIVOT },			// 48
	{ "ic58_f357.bin",			0x080000, 0x16486967, TAITO_CHARS_PIVOT },			// 49
	{ "ic57_1a62.bin",			0x080000, 0xafd45e35, TAITO_CHARS_PIVOT },			// 50
	{ "ic59_7cce.bin",			0x080000, 0xee762199, TAITO_CHARS_PIVOT },			// 51

	{ "st8_ic2.bin",			0x040000, 0xd74254d8, TAITO_SPRITEMAP },			// 52 Sprite Map
	{ "st0_ic1.bin",			0x040000, 0xc414c479, TAITO_SPRITEMAP },			// 53
	{ "st16_ic3.bin",			0x040000, 0xc4ff6b2f, TAITO_SPRITEMAP },			// 54

	{ "ic84_0816_wave0.bin",	0x080000, 0xc30c71fd, TAITO_ES5505_BYTESWAP },		// 55 Ensoniq Samples/Data
	{ "ic85_8058_wave1.bin",	0x080000, 0xfe37d544, TAITO_ES5505_BYTESWAP },		// 56
	{ "ic86_9e88_wave2.bin",	0x080000, 0xd6dcb45d, TAITO_ES5505_BYTESWAP },		// 57
	{ "ic87_42e7_wave3.bin",	0x080000, 0xfe52856b, TAITO_ES5505_BYTESWAP },		// 58
	{ "ic88_2704_wave4.bin",	0x080000, 0xcba55d36, TAITO_ES5505_BYTESWAP },		// 59
	{ "ic107_3a9c_wave14.bin",	0x080000, 0x26312451, TAITO_ES5505_BYTESWAP },		// 60
	{ "ic108_a148_wave15.bin",	0x080000, 0x2edaa9dc, TAITO_ES5505_BYTESWAP },		// 61
};

STD_ROM_PICK(cbombersp)
STD_ROM_FN(cbombersp)

static INT32 CbomberspInit()
{
	return CommonInit(2);
}

struct BurnDriver BurnDrvCbombersp = {
	"cbombersp", "cbombers", NULL, NULL, "1994",
	"Chase Bombers (Japan Prototype)\0", "With graphics issues", "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, cbomberspRomInfo, cbomberspRomName, NULL, NULL, NULL, NULL, CbombersInputInfo, NULL,
	CbomberspInit, DrvExit, DrvFrame, CbombersDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 232, 4, 3
};
