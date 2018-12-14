// FB Alpha Data East MLC Hardware driver module
// Based on MAME driver by Bryan Mcphail

#include "tiles_generic.h"
#include "sh2_intf.h"
#include "arm_intf.h"
#include "ymz280b.h"
#include "deco146.h"
#include "deco16ic.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *DrvPrgROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvEEPROM;
static UINT32 *DrvPalette;
static UINT8 DrvRecalc;
static UINT8 *AllRam;
static UINT8 *DrvPrgRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprBuf;
static UINT8 *DrvVRAM;
static UINT8 *DrvIRQRAM;
static UINT8 *DrvClipRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static INT32 scanline_timer;

static UINT8 DrvJoy1[32];
static UINT8 DrvJoy2[32];
static UINT8 DrvJoy3[32];
static UINT32 DrvInputs[3];
static UINT8 DrvReset;

static INT32 sprite_color_mask;
static INT32 sprite_depth;
static INT32 sprite_mask;
static INT32 use_sh2 = 0;
static UINT32 vblank_flip;
static INT32 global_scanline;
static INT32 game_select;

static struct BurnInputInfo MlcInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 24,	"p1 fire 4"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 17,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 27,	"p2 fire 4"	},

	{"P3 Coin",		BIT_DIGITAL,	DrvJoy1 + 20,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 23,	"p3 start"	},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy2 + 16,	"p3 up"		},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy2 + 17,	"p3 down"	},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy2 + 18,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 19,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 20,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 21,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 22,	"p3 fire 3"	},

	{"P4 Coin",		BIT_DIGITAL,	DrvJoy1 + 21,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy3 + 23,	"p4 start"	},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy3 + 16,	"p4 up"		},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy3 + 17,	"p4 down"	},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy3 + 18,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy3 + 19,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy3 + 20,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy3 + 21,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy3 + 22,	"p4 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 18,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 22,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 19,	"diag"	        },
};

STDINPUTINFO(Mlc)

static inline void palette_write(INT32 offset)
{
	offset &= 0x7ffc;

	if (offset >= 0x2000) return;

	offset /= 4;

	UINT32 *p = (UINT32*)DrvPalRAM;

	UINT8 b = (p[offset] >> 10) & 0x1f;
	UINT8 g = (p[offset] >>  5) & 0x1f;
	UINT8 r = (p[offset] >>  0) & 0x1f;

	r = (r << 3) | (r >> 2);
	g = (g << 3) | (g >> 2);
	b = (b << 3) | (b >> 2);

	DrvPalette[offset] = BurnHighCol(r,g,b,0);

	b = (b * 0x7f) >> 8;
	g = (g * 0x7f) >> 8;
	r = (r * 0x7f) >> 8;

	DrvPalette[0x800+offset] = BurnHighCol(r,g,b,0);
}

static void mlc_irq_write(INT32 offset)
{
	switch (offset & 0x7c)
	{
		case 0x10:
			if (use_sh2) {
				Sh2SetIRQLine(1, CPU_IRQSTATUS_NONE);
			} else {
				ArmSetIRQLine(ARM_IRQ_LINE, CPU_IRQSTATUS_NONE);
			}
		return;

		case 0x14:
			scanline_timer = *((UINT32*)(DrvIRQRAM + 0x14)) & 0xffff;
		return;
	}
}

static void decomlc_write_byte(UINT32 address, UINT8 data)
{
	if (address < 0xfffff) {
		// NOP
		return;
	}

	if ((address & 0xff8000) == 0x300000) {
		DrvPalRAM[address & 0x7fff] = data;
		palette_write(address);
		return;
	}

	if ((address & 0xffff80) == 0x200000) {
		DrvIRQRAM[address & 0x7f] = data;
		mlc_irq_write(address);
		return;
	}

	if ((address & 0xffff80) == 0x200080) {
		DrvClipRAM[address & 0x7f] = data;
		return;
	}

	Write16Byte(DrvSprRAM, 0x204000, 0x206fff)

	if ((address & 0xfff000) == 0x70f000) {
		deco146_104_prot_wb(0, ((address & 0xffc) >> 1) | (address & 1), data);
		return;
	}

	switch (address)
	{
		case 0x44001c:
		return;		// nop

		case 0x500000:
			YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, (255.00 - data) / 255.00, (game_select == 2) ? BURN_SND_ROUTE_BOTH : BURN_SND_ROUTE_LEFT);
			YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, (255.00 - data) / 255.00, (game_select == 2) ? BURN_SND_ROUTE_BOTH : BURN_SND_ROUTE_RIGHT);
		return;

		case 0x500001: 
			EEPROMSetClockLine((data & 2) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit(data & 1);
			EEPROMSetCSLine((data & 4) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x500002:
			// coin counter
		return;

		case 0x600000:
		case 0x600003:
		case 0x600004:
		case 0x600007:
			YMZ280BWrite((address / 4) & 1, data);
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static void decomlc_write_long(UINT32 address, UINT32 data)
{
	if (address < 0xfffff) {
		// NOP
		return;
	}

	if ((address & 0xff8000) == 0x300000) {
		*((UINT32*)(DrvPalRAM + (address & 0x7ffc))) = data;
		palette_write(address);
		return;
	}

	Write16Long(DrvSprRAM, 0x204000, 0x206fff)

	if ((address & 0xffff80) == 0x200000) {
		*((UINT32*)(DrvIRQRAM + (address & 0x7c))) = data;
		mlc_irq_write(address);
		return;
	}

	if ((address & 0xffff80) == 0x200080) {
		*((UINT32*)(DrvClipRAM + (address & 0x7c))) = data;
		return;
	}

	if ((address & 0xfff000) == 0x70f000) {
		deco146_104_prot_ww(0, ((address & 0xffc) >> 1), data >> 16);
		return;
	}

	switch (address & ~3)
	{
		case 0x44000c:
		case 0x44001c:
		case 0x708004:
		return;		// nop

		case 0x500000:
			data >>= 8;
			EEPROMWrite(data & 0x02, data & 0x04, data & 0x01);
		return;

		case 0x600000:
		case 0x600003:
		case 0x600004:
		case 0x600007:
			YMZ280BWrite((address / 4) & 1, data >> 24);
		return;
	}

	bprintf (0, _T("WL: %5.5x, %4.4x\n"), address, data);
}

static UINT8 decomlc_read_byte(UINT32 address)
{
	if ((address & 0xffff80) == 0x200080) {
		return DrvClipRAM[address & 0x7f];
	}

	Read16Byte(DrvSprRAM, 0x204000, 0x206fff)

	if ((address & 0xfff000) == 0x70f000) {
		return deco146_104_prot_rb(0, ((address & 0xffc) >> 1) | (address & 1));
	}

	switch (address)
	{
		case 0x200000:
		case 0x200004:
		case 0x20007c:
		case 0x321a34:
		case 0x440008:
		case 0x44001c:
			return ~0;

		case 0x200070:
			vblank_flip ^= ~0;
			return vblank_flip;

		case 0x200074:
			return global_scanline;

		case 0x400000:
			return ((DrvInputs[0] & ~0x00800000)| (EEPROMRead() << 23)) >> ((address & 3) * 8); // inputs

		case 0x440000:
			return ~0; // inputs2

		case 0x440004:
			return ~0; // inputs3

		case 0x600003:
		case 0x600007:
			return YMZ280BRead((address / 4) & 1);
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static UINT32 decomlc_read_long(UINT32 address)
{
	if ((address & 0xffff80) == 0x200080) {
		return *((UINT32*)(DrvClipRAM + (address & 0x7c)));
	}

	Read16Long(DrvSprRAM, 0x204000, 0x206fff)

	if ((address & 0xfff000) == 0x70f000) {
		UINT16 ret = deco146_104_prot_rw(0, ((address & 0xffc) >> 1));
		return ret | (ret << 16);
	}

	switch (address & ~3)
	{
		case 0x200000:
		case 0x200004:
		case 0x20007c:
		case 0x440008:
		case 0x44000c: // ?
		case 0x44001c:
		case 0x314304: // skullfang weird reads
		case 0x2f94e8:
		case 0x2d333c:
		case 0x18f690:
		case 0x206ddfc:
		case 0x39e6d4:
		case 0x342fe0:
		case 0x321a30:
		case 0x33db50:
		case 0x353718:
		case 0x2d2f6c:
		case 0x222b1c:
			return ~0;

		case 0x200070:
			vblank_flip ^= ~0;
			return vblank_flip;

		case 0x200074:
			return global_scanline;

		case 0x400000:
			return (DrvInputs[0] & ~0x00800000) | (EEPROMRead() << 23);

		case 0x440000:
			return DrvInputs[1];

		case 0x440004:
			return DrvInputs[2];

		case 0x600000:
		case 0x600004:
			return YMZ280BRead((address / 4) & 1);
	}

	bprintf (0, _T("RL: %5.5x\n"), address);

	return 0;
}

static void __fastcall mlcsh2_write_long(UINT32 address, UINT32 data)
{
	decomlc_write_long(address& 0xffffff, data);
}

static void __fastcall mlcsh2_write_word(UINT32 address, UINT16 data)
{
	bprintf (0, _T("SH2 WW: %8.8x %4.4x\n"), address & 0xffffff, data);
}

static void __fastcall mlcsh2_write_byte(UINT32 address, UINT8 data)
{
	decomlc_write_byte((address ^ 3) & 0xffffff, data);
}

static UINT32 __fastcall mlcsh2_read_long(UINT32 address)
{
	return decomlc_read_long(address & 0xffffff);
}

static UINT16 __fastcall mlcsh2_read_word(UINT32 address)
{
	bprintf (0, _T("SH2 RW: %8.8x\n"), address & 0xffffff);

	return 0;
}

static UINT8 __fastcall mlcsh2_read_byte(UINT32 address)
{
	return decomlc_read_byte((address ^ 3) & 0xffffff);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	(use_sh2) ? Sh2Reset() : ArmReset();

	deco_146_104_reset();

	EEPROMReset();

	if (!EEPROMAvailable())
		EEPROMFill(DrvEEPROM, 0, 128);

	YMZ280BReset();

	scanline_timer = -1;
	vblank_flip = ~0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvPrgROM		= Next; Next += 0x0100000;

	DrvGfxROM1		= Next; Next += 0x0080000;

	YMZ280BROM		= Next; 
	DrvSndROM		= Next; Next += YMZ280BROMSIZE;

	DrvEEPROM		= Next; Next += 0x0000080;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * 2 * sizeof(UINT32);

	AllRam			= Next;

	DrvPrgRAM		= Next; Next += 0x0020000;
	DrvPalRAM		= Next; Next += 0x0008000;
	DrvSprRAM		= Next; Next += 0x0001800;
	DrvSprBuf		= Next; Next += 0x0001800;
	DrvVRAM			= Next; Next += 0x0020000;

	DrvIRQRAM		= Next; Next += 0x0000080;
	DrvClipRAM		= Next; Next += 0x0000080;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void decode_sound()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);

	for (UINT32 i = 0; i < YMZ280BROMSIZE; i+= 0x200000)
	{
		memcpy (tmp, DrvSndROM + i, 0x200000);

		for (INT32 j = 0; j < 0x200000; j++)
		{
			INT32 k = ((j & 0x1ffffe) >> 1) | ((j & 1) << 20);

			DrvSndROM[k + i] = tmp[j];
		}
	}

	BurnFree(tmp);
}

static void set_sprite_config(INT32 len, INT32 bpp)
{
	sprite_depth = bpp;
	sprite_color_mask = 0x7f >> (bpp - 4);
	sprite_mask = (((len * 8) / ((bpp + 1) & 6)) / (16 * 16));

	for (INT32 i = 1; i < (1 << 31); i<<=1) {
		if (i >= sprite_mask) {
			sprite_mask = i;
			break;
		}
	}
}

static void decode_plane(UINT8 *src, UINT8 *dst, INT32 plane, INT32 ofst, INT32 len)
{
	for (INT32 i = 0; i < len; i+=2)
	{
		for (INT32 k = 0; k < 8; k++)
		{
			INT32  m  = ((src[i+0] >> (k & 7)) & 1) << 0;
			       m |= ((src[i+1] >> (k & 7)) & 1) << 1;

			dst[(i*8)+k+ofst] |= m << plane;
		}
	}
}

static INT32 CommonArmInit(INT32 game)
{
	game_select = game;
	use_sh2 = (game_select == 3) ? 1 : 0;
	YMZ280BROMSIZE = (game_select == 0 || game_select == 3) ? 0x600000 : 0x400000;

	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		switch (game_select)
		{	
			case 0: // skull fang
			{
				if (BurnLoadRomExt(DrvPrgROM + 0x000000, 0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(DrvPrgROM + 0x000002, 1, 4, LD_GROUP(2))) return 1;

				DrvGfxROM0 = (UINT8*)BurnMalloc(((0xc00000/6)*8));

				memset (DrvGfxROM0, 0, ((0xc00000/6)*8));

				UINT8 *tmp = DrvSndROM;

				for (INT32 i = 0; i < 6; i++)
				{
					if (BurnLoadRom(tmp,  2 + i, 1)) return 1;

					decode_plane(tmp, DrvGfxROM0, i & ~1, (i & 1) * 8, 0x200000);
				}

				if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  8, 1)) return 1;

				if (BurnLoadRom(DrvSndROM  + 0x0200000,  9, 1)) return 1;
				if (BurnLoadRom(DrvSndROM  + 0x0400000, 10, 1)) return 1;

				if (BurnLoadRom(DrvEEPROM  + 0x0000000, 11, 1)) return 1;

				deco156_decrypt(DrvPrgROM, 0x100000);
				set_sprite_config(0xc00000, 6);
			}
			break;

			case 1: // Stadium Hero '96 
			{
				if (BurnLoadRomExt(DrvPrgROM + 0x000000, 0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(DrvPrgROM + 0x000002, 1, 4, LD_GROUP(2))) return 1;

				DrvGfxROM0 = (UINT8*)BurnMalloc(((0x1800000/6)*8));

				memset (DrvGfxROM0, 0, ((0x1800000/6)*8));

				UINT8 *tmp = DrvSndROM;

				for (INT32 i = 0; i < 6; i++)
				{
					if (BurnLoadRom(tmp,  2 + i, 1)) return 1;

					decode_plane(tmp, DrvGfxROM0, i & ~1, (i & 1) * 8, 0x400000);
				}

				if (BurnLoadRom(DrvGfxROM1 + 0x0000000,  8, 1)) return 1;

				if (BurnLoadRom(DrvSndROM  + 0x0000000,  9, 1)) return 1;

				if (BurnLoadRom(DrvEEPROM  + 0x0000000, 10, 1)) return 1;

				deco156_decrypt(DrvPrgROM, 0x100000);
				set_sprite_config(0x1800000, 6);
			}
			break;

			case 2: // Hoops '96
			{
				if (BurnLoadRomExt(DrvPrgROM + 0x000000, 0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(DrvPrgROM + 0x000002, 1, 4, LD_GROUP(2))) return 1;

				DrvGfxROM0 = (UINT8*)BurnMalloc(((0xc00000/6)*8));

				memset (DrvGfxROM0, 0, ((0xc00000/6)*8));

				UINT8 *tmp = DrvSndROM;

				for (INT32 i = 0; i < 4; i++)
				{
					if (BurnLoadRom(tmp,  2 + i, 1)) return 1;

					decode_plane(tmp, DrvGfxROM0, i & ~1, (i & 1) * 8, 0x200000);
				}

				if (BurnLoadRom(tmp, 6, 1)) return 1;

				decode_plane(tmp + 0x000000, DrvGfxROM0, 4, 0, 0x200000);
				decode_plane(tmp + 0x000001, DrvGfxROM0, 4, 8, 0x200000);

				for (INT32 i = 0; i < ((0xc00000/6)*8); i++) DrvGfxROM0[i] &= 0x1f;

				memset (DrvGfxROM1, 0xff, 0x80000);
				if (BurnLoadRom(DrvGfxROM1 + 0x0020000,  7, 1)) return 1;

				if (BurnLoadRom(DrvSndROM  + 0x0000000,  8, 1)) return 1;

				if (BurnLoadRom(DrvEEPROM  + 0x0000000,  9, 1)) return 1;

				deco156_decrypt(DrvPrgROM, 0x100000);
				set_sprite_config(0x0c00000, 5);
			}
			break;

			case 3: // Avengers In Galactic Storm
			{
				if (BurnLoadRomExt(DrvPrgROM + 0x000000, 0, 4, LD_GROUP(2))) return 1;
				if (BurnLoadRomExt(DrvPrgROM + 0x000002, 1, 4, LD_GROUP(2))) return 1;

				DrvGfxROM0 = (UINT8*)BurnMalloc(((0x1800000/4)*8));

				memset (DrvGfxROM0, 0, ((0x1800000/4)*8));

				UINT8 *tmp = DrvSndROM;

				for (INT32 i = 0; i < 12; i++)
				{
					if (BurnLoadRom(tmp,  2 + i, 1)) return 1;

					decode_plane(tmp, DrvGfxROM0 + (((i % 6) / 2) * 0x1000000), (i / 6) * 2, (i & 1) * 8, 0x200000);
				}

				if (BurnLoadRom(DrvGfxROM1 + 0x0000000, 14, 1)) return 1;

				if (BurnLoadRom(DrvSndROM  + 0x0000000, 15, 1)) return 1;
				if (BurnLoadRom(DrvSndROM  + 0x0200000, 16, 1)) return 1;
				if (BurnLoadRom(DrvSndROM  + 0x0400000, 17, 1)) return 1;

				if (BurnLoadRom(DrvEEPROM  + 0x0000000, 18, 1)) return 1;

				set_sprite_config(0x1800000, 4);
			}
			break;
		}

		decode_sound();
	}

	if (use_sh2)
	{
		Sh2Init(1);
		Sh2Open(0);

		for (INT32 i = 0; i <= 0xc68; i+=8) {
			Sh2MapMemory(DrvPrgROM,		0x000000+(i<<20), 0x0fffff+(i<<20), MAP_ROM);
			Sh2MapMemory(DrvPrgRAM,		0x100000+(i<<20), 0x11ffff+(i<<20), MAP_RAM);
			Sh2MapMemory(DrvVRAM,		0x280000+(i<<20), 0x29ffff+(i<<20), MAP_RAM);
			Sh2MapMemory(DrvPalRAM,		0x300000+(i<<20), 0x307fff+(i<<20), MAP_ROM);
		}

		Sh2SetReadByteHandler (0,	mlcsh2_read_byte);
		Sh2SetReadWordHandler (0,	mlcsh2_read_word);
		Sh2SetReadLongHandler (0,	mlcsh2_read_long);
		Sh2SetWriteByteHandler(0,	mlcsh2_write_byte);
		Sh2SetWriteWordHandler(0,	mlcsh2_write_word);
		Sh2SetWriteLongHandler(0,	mlcsh2_write_long);
	}
	else
	{
		ArmInit(0);
		ArmOpen(0);	
		ArmMapMemory(DrvPrgROM,		0x000000, 0x0fffff, MAP_ROM);
		ArmMapMemory(DrvPrgRAM,		0x100000, 0x11ffff, MAP_RAM);
		ArmMapMemory(DrvVRAM,		0x280000, 0x29ffff, MAP_RAM);
		ArmMapMemory(DrvPalRAM,		0x300000, 0x307fff, MAP_ROM);
		ArmSetWriteByteHandler(decomlc_write_byte);
		ArmSetWriteLongHandler(decomlc_write_long);
		ArmSetReadByteHandler(decomlc_read_byte);
		ArmSetReadLongHandler(decomlc_read_long);
	}

	deco_146_init();
	deco_146_104_set_use_magic_read_address_xor(1);

	EEPROMInit(&eeprom_interface_93C46);

	YMZ280BInit(14000000, NULL);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 1.00, (game_select == 2) ? BURN_SND_ROUTE_BOTH : BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 1.00, (game_select == 2) ? BURN_SND_ROUTE_BOTH : BURN_SND_ROUTE_RIGHT);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	if (use_sh2) {
		Sh2Exit();
	} else {
		ArmExit();
	}

	deco_146_104_exit();

	EEPROMExit();

	YMZ280BExit();
	YMZ280BROM = NULL;

	GenericTilesExit();

	BurnFree (DrvGfxROM0);

	BurnFree (AllMem);

	use_sh2 = 0;

	return 0;
}

static void mlc_drawgfxzoomline(UINT16 *dest, INT32 *clip, UINT32 code1, UINT32 code2, UINT32 color, INT32 flipx, INT32 sx,
		INT32 transparent_color, INT32 use8bpp, INT32 scalex, INT32 alpha, INT32 srcline)
{
	if (!scalex) return;

	INT32 sprite_screen_width = (scalex*16+(sx&0xffff))>>16;

	sx>>=16;

	if (sprite_screen_width)
	{
		INT32 dx = 0x100000/sprite_screen_width;
		INT32 ex = sx + sprite_screen_width;
		INT32 x_index_base;

		if (flipx)
		{
			x_index_base = (sprite_screen_width-1)*dx;
			dx = -dx;
		}
		else
		{
			x_index_base = 0;
		}

		if (sx < clip[0])
		{
			INT32 pixels = clip[0]-sx;
			sx += pixels;
			x_index_base += pixels*dx;
		}

		if (ex > clip[1]+1)
		{
			ex -= ex-clip[1]-1;
		}

		if (ex > sx)
		{
			color = (color & sprite_color_mask) << sprite_depth;
			const UINT8 *code_base1 = DrvGfxROM0 + ((code1 % sprite_mask) * 0x100);

			if (alpha == 0xff)
			{
				const UINT8 *code_base2 = DrvGfxROM0 + ((code2 % sprite_mask) * 0x100);
				const UINT8 *source1 = code_base1 + (srcline) * 16;
				const UINT8 *source2 = code_base2 + (srcline) * 16;

				INT32 x_index = x_index_base;

				for (INT32 x=sx; x<ex; x++ )
				{
					INT32 c = source1[x_index>>16];

					if (use8bpp) c = (c << 4) | source2[x_index >> 16];

					if (c != transparent_color) dest[x] = c + color;

					x_index += dx;
				}
			}
			else
			{
				const UINT8 *source = code_base1 + (srcline) * 16;

				INT32 x_index = x_index_base;
				for(INT32 x=sx; x<ex; x++ )
				{
					INT32 c = source[x_index>>16];
					if (c != transparent_color) dest[x] |= 0x800;
					x_index += dx;
				}
			}
		}
	}
}

static void draw_sprites(INT32 scanline)
{
	UINT32 *index_ptr=NULL;
	INT32 offs,fx=0,fy=0,x,y,color,colorOffset,sprite,indx,h,w,bx,by,fx1,fy1;
	INT32 xoffs,yoffs;
	UINT8 *rom = DrvGfxROM1 + 0x20000, *index_ptr8;
	UINT8 *rawrom = DrvGfxROM1;
	INT32 blockIsTilemapIndex=0;
	INT32 sprite2=0,indx2=0,use8bppMode=0;
	INT32 yscale,xscale;
	INT32 alpha;
	INT32 useIndicesInRom=0;
	INT32 hibits=0;
	INT32 tileFormat=0;
	INT32 rasterMode=0;

	INT32 clipper=0;

	UINT16 *dest = pTransDraw + ((scanline - 8) * nScreenWidth);

	INT32 clip[4];
	UINT16* mlc_spriteram=(UINT16*)DrvSprBuf;
	UINT32 *m_mlc_clip_ram = (UINT32*)DrvClipRAM;
	UINT32 *m_mlc_vram = (UINT32*)DrvVRAM;
	UINT32 *m_irq_ram = (UINT32*)DrvIRQRAM;

	for (offs = (0x3000/4)-8; offs>=0; offs-=8)
	{
		if ((mlc_spriteram[offs+0]&0x8000)==0)
			continue;
		if ((mlc_spriteram[offs+1]&0x2000) && (nCurrentFrame & 1))
			continue;

		y = mlc_spriteram[offs+2]&0x7ff;
		x = mlc_spriteram[offs+3]&0x7ff;

		if (x&0x400) x=-(0x400-(x&0x3ff));
		if (y&0x400) y=-(0x400-(y&0x3ff));

		fx = mlc_spriteram[offs+1]&0x8000;
		fy = mlc_spriteram[offs+1]&0x4000;
		color = mlc_spriteram[offs+1]&0xff;

		INT32 raster_select = (mlc_spriteram[offs+1]&0x0180)>>7;

		rasterMode = (mlc_spriteram[offs+1]>>10)&0x1;

		clipper = (mlc_spriteram[offs+1]>>8)&0x3;

		indx = mlc_spriteram[offs+0]&0x3fff;
		yscale = mlc_spriteram[offs+4]&0x3ff;
		xscale = mlc_spriteram[offs+5]&0x3ff;
		colorOffset = 0;

		clipper=((clipper&2)>>1)|((clipper&1)<<1); // Swap low two bits

		INT32 upperclip = (mlc_spriteram[offs+1]>>10)&0x2;

		if (upperclip)
			clipper |= 0x4;

		INT32 min_y = m_mlc_clip_ram[(clipper*4)+0];
		INT32 max_y = m_mlc_clip_ram[(clipper*4)+1];

		if (scanline<min_y) continue;
		if (scanline>max_y) continue;

		clip[0]=m_mlc_clip_ram[(clipper*4)+2];
		clip[1]=m_mlc_clip_ram[(clipper*4)+3];

		// Any colours out of range (for the bpp value) trigger 'shadow' mode
		if (color & (sprite_color_mask+1))
			alpha=0x80;
		else
			alpha=0xff;

		color&=sprite_color_mask;

		// If this bit is set, combine this block with the next one
		if (mlc_spriteram[offs+1]&0x1000) {
			use8bppMode=1;
			// In 8bpp the palette base is stored in the next block
			if (offs-8>=0) {
				color = (mlc_spriteram[offs+1-8]&0x7f);
				indx2 = mlc_spriteram[offs+0-8]&0x3fff;
			}
		} else
			use8bppMode=0;

		// Lookup tiles/size in sprite index ram OR in the lookup rom
		if (mlc_spriteram[offs+0]&0x4000) {
			index_ptr8=rom + indx*8; // Byte ptr
			h=(index_ptr8[1]>>0)&0xf;
			w=(index_ptr8[3]>>0)&0xf;

			if (!h) h=16;
			if (!w) w=16;

			sprite = (index_ptr8[7]<<8)|index_ptr8[6];
			sprite |= (index_ptr8[4]&3)<<16;

			if (use8bppMode) {
				UINT8* index_ptr28=rom + indx2*8;
				sprite2=(index_ptr28[7]<<8)|index_ptr28[6];
			}

			yoffs=index_ptr8[0]&0xff;
			xoffs=index_ptr8[2]&0xff;

			fy1=(index_ptr8[1]&0x10)>>4;
			fx1=(index_ptr8[3]&0x10)>>4;

			tileFormat=index_ptr8[4]&0x80;

			if (index_ptr8[4]&0xc0)
				blockIsTilemapIndex=1;
			else
				blockIsTilemapIndex=0;

			useIndicesInRom=0;
		} else {
			indx&=0x1fff;
			index_ptr=m_mlc_vram + indx*4;
			h=(index_ptr[0]>>8)&0xf;
			w=(index_ptr[1]>>8)&0xf;

			if (!h) h=16;
			if (!w) w=16;

			if (use8bppMode) {
				UINT32* index_ptr2=m_mlc_vram + ((indx2*4)&0x7fff);
				sprite2=((index_ptr2[2]&0x3)<<16) | (index_ptr2[3]&0xffff);
			}

			sprite = ((index_ptr[2]&0x3)<<16) | (index_ptr[3]&0xffff);
			if (index_ptr[2]&0xc0)
				blockIsTilemapIndex=1;
			else
				blockIsTilemapIndex=0;

			tileFormat=index_ptr[2]&0x80;

			hibits=(index_ptr[2]&0x3c)<<10;
			useIndicesInRom=index_ptr[2]&3;

			yoffs=index_ptr[0]&0xff;
			xoffs=index_ptr[1]&0xff;

			fy1=(index_ptr[0]&0x1000)>>12;
			fx1=(index_ptr[1]&0x1000)>>12;
		}

		if(fx1) fx^=0x8000;
		if(fy1) fy^=0x4000;

		INT32 extra_x_scale = 0x100;

		if (rasterMode)
		{
			if (raster_select==1 || raster_select==2 || raster_select==3)
			{
				INT32 irq_base_reg; // 6, 9, 12  are possible
				if (raster_select== 1) irq_base_reg = 6;    // OK upper screen.. left?
				else if (raster_select== 2) irq_base_reg = 9; // OK upper screen.. main / center
				else irq_base_reg = 12;

				INT32 extra_y_off = m_irq_ram[irq_base_reg+0] & 0x7ff;
				INT32 extra_x_off = m_irq_ram[irq_base_reg+1] & 0x7ff;
				extra_x_scale = (m_irq_ram[irq_base_reg+2]>>0) & 0x3ff;

				if (extra_x_off & 0x400) { extra_x_off -= 0x800; }
				if (extra_y_off & 0x400) { extra_y_off -= 0x800; }

				x += extra_x_off;
				y += extra_y_off;
			}
		}

		xscale *= extra_x_scale;

		INT32 ybase=y<<16;
		INT32 yinc=(yscale<<8)*16;

		if (fy)
			ybase+=(yoffs-15) * (yscale<<8) - ((h-1)*yinc);
		else
			ybase-=yoffs * (yscale<<8);

		INT32 xbase=x<<16;
		INT32 xinc=(xscale)*16;

		if (fx)
			xbase+=(xoffs-15) * (xscale) - ((w-1)*xinc);
		else
			xbase-=xoffs * (xscale);


		INT32 full_realybase = ybase;
		INT32 full_sprite_screen_height = ((yscale<<8)*(h*16)+(0));
		INT32 full_sprite_screen_height_unscaled = ((1)*(h*16)+(0));

		if (!full_sprite_screen_height_unscaled)
			continue;

		INT32 ratio = full_sprite_screen_height / full_sprite_screen_height_unscaled;

		if (!ratio)
			continue;

		INT32 bby = scanline - (full_realybase>>16);

		if (bby < 0)
			continue;

		if (bby >= full_sprite_screen_height>>16)
			continue;

		INT32 srcline = ((bby<<16) / ratio);

		by = srcline >> 4;

		srcline &=0xf;
		if( fy )
		{
			srcline = 15 - srcline;
		}

		for (bx=0; bx<w; bx++) {
			INT32 realxbase = xbase + bx * xinc;
			INT32 count = 0;
			if (fx)
			{
				if (fy)
					count = (h-1-by) * w + (w-1-bx);
				else
					count = by * w + (w-1-bx);
			}
			else
			{
				if (fy)
					count = (h-1-by) * w + bx;
				else
					count = by * w + bx;
			}

			INT32 tile=sprite + count;
			INT32 tile2=sprite2 + count;

			if (blockIsTilemapIndex) {
				if (useIndicesInRom)
				{
					const UINT8* ptr=rawrom+(tile*2);
					tile=(*ptr) + ((*(ptr+1))<<8);

					if (use8bppMode) {
						const UINT8* ptr2=rawrom+(tile2*2);
						tile2=(*ptr2) + ((*(ptr2+1))<<8);
					}
					else
					{
						tile2=0;
					}

					if (tileFormat)
					{
						colorOffset=(tile&0xf000)>>12;
						tile=(tile&0x0fff)|hibits;
						tile2=(tile2&0x0fff)|hibits;
					}
					else
					{
						colorOffset=0;
						tile=(tile&0xffff)|(hibits<<2);
						tile2=(tile2&0xffff)|(hibits<<2);
					}
				}
				else
				{
					const UINT32 * ptr=m_mlc_vram + ((tile)&0x7fff);
					tile=(*ptr)&0xffff;

					if (tileFormat)
					{
						colorOffset=(tile&0xf000)>>12;
						tile=(tile&0x0fff)|hibits;
					}
					else
					{
						colorOffset=0;
						tile=(tile&0xffff)|(hibits<<2);
					}

					tile2=0;
				}
			}

			mlc_drawgfxzoomline(dest,clip,tile,tile2,color + colorOffset,fx,realxbase,0,use8bppMode,(xscale),alpha, srcline);
		}

		if (use8bppMode)
			offs-=8;
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x2000; i+=4) {
			palette_write(i);
		}
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3 * sizeof(UINT32));

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	BurnTransferClear();

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = (use_sh2 ? 21000000 : 14000000) / 58;
	INT32 nCyclesDone = 0;

	(use_sh2) ? Sh2NewFrame() : ArmNewFrame();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		global_scanline = ((i > 0) ? i - 1 : 0);

		if (global_scanline == scanline_timer) {
			if (use_sh2) {
				Sh2SetIRQLine(1, CPU_IRQSTATUS_ACK);
			} else {
				ArmSetIRQLine(ARM_IRQ_LINE, CPU_IRQSTATUS_ACK);
			}
			scanline_timer = ~0;
		}

		nCyclesDone += (use_sh2) ? Sh2Run(nCyclesTotal / nInterleave) : ArmRun(nCyclesTotal / nInterleave);

		if (i >= 8 && i < 248) draw_sprites(i);
	}

	if (pBurnSoundOut) {
		YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	memcpy (DrvSprBuf, DrvSprRAM, 0x1800);

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		(use_sh2) ? Sh2Scan(nAction) : ArmScan(nAction);

		deco_146_104_scan();

	//	EEPROMScan(nAction, pnMin);

		YMZ280BScan(nAction, pnMin);

		SCAN_VAR(scanline_timer);
	}

	return 0;
}


// Avengers In Galactic Storm (US)

static struct BurnRomInfo avengrgsRomDesc[] = {
	{ "sf_00-0.7k",			0x080000, 0x7d20e2df, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "sf_01-0.7l",			0x080000, 0xf37c0a01, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mcg-00.1j",			0x200000, 0x99129d9a, 2 | BRF_GRA },           //  2 Sprites
	{ "mcg-02.1f",			0x200000, 0x29af9866, 2 | BRF_GRA },           //  3
	{ "mcg-01.1d",			0x200000, 0x3638861b, 2 | BRF_GRA },           //  4
	{ "mcg-03.7m",			0x200000, 0x4a0c965f, 2 | BRF_GRA },           //  5
	{ "mcg-08.7p",			0x200000, 0xc253943e, 2 | BRF_GRA },           //  6
	{ "mcg-09.7n",			0x200000, 0x8fb9870b, 2 | BRF_GRA },           //  7
	{ "mcg-04.3j",			0x200000, 0xa4954c0e, 2 | BRF_GRA },           //  8
	{ "mcg-06.3f",			0x200000, 0x01571cf6, 2 | BRF_GRA },           //  9
	{ "mcg-05.3d",			0x200000, 0x182c2b49, 2 | BRF_GRA },           // 10
	{ "mcg-07.8m",			0x200000, 0xd09a3635, 2 | BRF_GRA },           // 11
	{ "mcg-10.8p",			0x200000, 0x1383f524, 2 | BRF_GRA },           // 12
	{ "mcg-11.8n",			0x200000, 0x8f7fc281, 2 | BRF_GRA },           // 13

	{ "sf_02-0.6j",			0x080000, 0xc98585dd, 3 | BRF_GRA },           // 14 Sprite Look-up Table

	{ "mcg-12.5a",			0x200000, 0xbef9b28f, 4 | BRF_GRA },           // 15 YMZ280b Samples
	{ "mcg-13.9k",			0x200000, 0x92301551, 4 | BRF_GRA },           // 16
	{ "mcg-14.6a",			0x200000, 0xc0d8b5f0, 4 | BRF_GRA },           // 17

	{ "avengrgs.nv",		0x000080, 0xc0e84b4e, 0 | BRF_PRG | BRF_OPT }, // 18 Default Settings
};

STD_ROM_PICK(avengrgs)
STD_ROM_FN(avengrgs)

static INT32 AvengrgsInit()
{
	return CommonArmInit(3);
}

struct BurnDriver BurnDrvAvengrgs = {
	"avengrgs", NULL, NULL, NULL, "1995",
	"Avengers In Galactic Storm (US)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, avengrgsRomInfo, avengrgsRomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	AvengrgsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Avengers In Galactic Storm (Japan)

static struct BurnRomInfo avengrgsjRomDesc[] = {
	{ "sd_00-2.7k",			0x080000, 0x136be46a, 1 | BRF_PRG | BRF_ESS }, //  0 SH2 Code
	{ "sd_01-2.7l",			0x080000, 0x9d87f576, 1 | BRF_PRG | BRF_ESS }, //  1
	
	{ "mcg-00.1j",			0x200000, 0x99129d9a, 2 | BRF_GRA },           //  2 Sprites
	{ "mcg-02.1f",			0x200000, 0x29af9866, 2 | BRF_GRA },           //  3
	{ "mcg-01.1d",			0x200000, 0x3638861b, 2 | BRF_GRA },           //  4
	{ "mcg-03.7m",			0x200000, 0x4a0c965f, 2 | BRF_GRA },           //  5
	{ "mcg-08.7p",			0x200000, 0xc253943e, 2 | BRF_GRA },           //  6
	{ "mcg-09.7n",			0x200000, 0x8fb9870b, 2 | BRF_GRA },           //  7
	{ "mcg-04.3j",			0x200000, 0xa4954c0e, 2 | BRF_GRA },           //  8
	{ "mcg-06.3f",			0x200000, 0x01571cf6, 2 | BRF_GRA },           //  9
	{ "mcg-05.3d",			0x200000, 0x182c2b49, 2 | BRF_GRA },           // 10
	{ "mcg-07.8m",			0x200000, 0xd09a3635, 2 | BRF_GRA },           // 11
	{ "mcg-10.8p",			0x200000, 0x1383f524, 2 | BRF_GRA },           // 12
	{ "mcg-11.8n",			0x200000, 0x8f7fc281, 2 | BRF_GRA },           // 13

	{ "sd_02-0.6j",			0x080000, 0x24fc2b3c, 3 | BRF_GRA },           // 14 Sprite Look-up Table

	{ "mcg-12.5a",			0x200000, 0xbef9b28f, 4 | BRF_GRA },           // 15 YMZ280b Samples
	{ "mcg-13.9k",			0x200000, 0x92301551, 4 | BRF_GRA },           // 16
	{ "mcg-14.6a",			0x200000, 0xc0d8b5f0, 4 | BRF_GRA },           // 17

	{ "avengrgsj.nv",		0x000080, 0x7ea70843, 0 | BRF_PRG | BRF_OPT }, // 18 Default Settings
};

STD_ROM_PICK(avengrgsj)
STD_ROM_FN(avengrgsj)

struct BurnDriver BurnDrvAvengrgsj = {
	"avengrgsj", "avengrgs", NULL, NULL, "1995",
	"Avengers In Galactic Storm (Japan)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, avengrgsjRomInfo, avengrgsjRomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	AvengrgsInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Skull Fang (World)

static struct BurnRomInfo skullfngRomDesc[] = {
	{ "sw00-0.2a",			0x080000, 0x9658d9ce, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "sw01-0.2b",			0x080000, 0xc0d83d14, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mch-00.2e",			0x200000, 0xd5cc4238, 2 | BRF_GRA },           //  2 Sprites
	{ "mch-01.8m",			0x200000, 0xd37cf0cd, 2 | BRF_GRA },           //  3
	{ "mch-02.4e",			0x200000, 0x4046314d, 2 | BRF_GRA },           //  4
	{ "mch-03.10m",			0x200000, 0x1dea8f6c, 2 | BRF_GRA },           //  5
	{ "mch-04.6e",			0x200000, 0x4869dfe8, 2 | BRF_GRA },           //  6
	{ "mch-05.11m",			0x200000, 0xef0b54ba, 2 | BRF_GRA },           //  7

	{ "sw02-0.6h",			0x080000, 0x0d3ae757, 3 | BRF_GRA },           //  8 Sprite Look-up Table

	{ "mch-06.6a",			0x200000, 0xb2efe4ae, 4 | BRF_SND },           //  9 YMZ280b Samples
	{ "mch-07.11j",			0x200000, 0xbc1a50a1, 4 | BRF_SND },           // 10

	{ "skullfng.eeprom",		0x000080, 0x240d882e, 0 | BRF_PRG | BRF_OPT }, // 11 Default Settings
};

STD_ROM_PICK(skullfng)
STD_ROM_FN(skullfng)

static INT32 SkullfngInit()
{
	return CommonArmInit(0);
}

struct BurnDriver BurnDrvSkullfng = {
	"skullfng", NULL, NULL, NULL, "1996",
	"Skull Fang (World)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, skullfngRomInfo, skullfngRomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	SkullfngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Skull Fang (Japan)

static struct BurnRomInfo skullfngjRomDesc[] = {
	{ "sh00-0.2a",			0x080000, 0xe50358e8, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "sh01-0.2b",			0x080000, 0x2c288bcc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mch-00.2e",			0x200000, 0xd5cc4238, 2 | BRF_GRA },           //  2 Sprites
	{ "mch-01.8m",			0x200000, 0xd37cf0cd, 2 | BRF_GRA },           //  3
	{ "mch-02.4e",			0x200000, 0x4046314d, 2 | BRF_GRA },           //  4
	{ "mch-03.10m",			0x200000, 0x1dea8f6c, 2 | BRF_GRA },           //  5
	{ "mch-04.6e",			0x200000, 0x4869dfe8, 2 | BRF_GRA },           //  6
	{ "mch-05.11m",			0x200000, 0xef0b54ba, 2 | BRF_GRA },           //  7

	{ "sh02-0.6h",			0x080000, 0x0d3ae757, 3 | BRF_GRA },           //  8 Sprite Look-up Table

	{ "mch-06.6a",			0x200000, 0xb2efe4ae, 4 | BRF_GRA },           //  9 YMZ280b Samples
	{ "mch-07.11j",			0x200000, 0xbc1a50a1, 4 | BRF_GRA },           // 10

	{ "skullfng.eeprom",		0x000080, 0x240d882e, 0 | BRF_PRG | BRF_OPT }, // 11 Default Settings
};

STD_ROM_PICK(skullfngj)
STD_ROM_FN(skullfngj)

struct BurnDriver BurnDrvSkullfngj = {
	"skullfngj", "skullfng", NULL, NULL, "1996",
	"Skull Fang (Japan)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, skullfngjRomInfo, skullfngjRomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	SkullfngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Skull Fang (Asia)

static struct BurnRomInfo skullfngaRomDesc[] = {
	{ "sx00-0.2a",			0x080000, 0x749c0972, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "sx01-0.2b",			0x080000, 0x61ae7dc3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mch-00.2e",			0x200000, 0xd5cc4238, 2 | BRF_GRA },           //  2 Sprites
	{ "mch-01.8m",			0x200000, 0xd37cf0cd, 2 | BRF_GRA },           //  3
	{ "mch-02.4e",			0x200000, 0x4046314d, 2 | BRF_GRA },           //  4
	{ "mch-03.10m",			0x200000, 0x1dea8f6c, 2 | BRF_GRA },           //  5
	{ "mch-04.6e",			0x200000, 0x4869dfe8, 2 | BRF_GRA },           //  6
	{ "mch-05.11m",			0x200000, 0xef0b54ba, 2 | BRF_GRA },           //  7

	{ "sx02-0.6h",			0x080000, 0x0d3ae757, 3 | BRF_GRA },           //  8 Sprite Look-up Table

	{ "mch-06.6a",			0x200000, 0xb2efe4ae, 4 | BRF_GRA },           //  9 YMZ280b Samples
	{ "mch-07.11j",			0x200000, 0xbc1a50a1, 4 | BRF_GRA },           // 10

	{ "skullfng.eeprom",		0x000080, 0x240d882e, 0 | BRF_PRG | BRF_OPT }, // 11 Default Settings
};

STD_ROM_PICK(skullfnga)
STD_ROM_FN(skullfnga)

struct BurnDriver BurnDrvSkullfnga = {
	"skullfnga", "skullfng", NULL, NULL, "1996",
	"Skull Fang (Asia)\0", NULL, "Data East Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL, 4, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, skullfngaRomInfo, skullfngaRomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	SkullfngInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	240, 320, 3, 4
};


// Stadium Hero '96 (World, EAJ)

static struct BurnRomInfo stadhr96RomDesc[] = {
	{ "sh-eaj.2a",			0x080000, 0x10d1496a, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "sh-eaj.2b",			0x080000, 0x608a9144, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mcm-00.2e",			0x400000, 0xc1919c3c, 2 | BRF_GRA },           //  2 Sprites
	{ "mcm-01.8m",			0x400000, 0x2255d47d, 2 | BRF_GRA },           //  3
	{ "mcm-02.4e",			0x400000, 0x38c39822, 2 | BRF_GRA },           //  4
	{ "mcm-03.10m",			0x400000, 0x4bd84ca7, 2 | BRF_GRA },           //  5
	{ "mcm-04.6e",			0x400000, 0x7c0bd84c, 2 | BRF_GRA },           //  6
	{ "mcm-05.11m",			0x400000, 0x476f03d7, 2 | BRF_GRA },           //  7

	{ "sh-eaf.6h",			0x080000, 0xf074a5c8, 3 | BRF_GRA },           //  8 Sprite Look-up Table

	{ "mcm-06.6a",			0x400000, 0xfbc178f3, 4 | BRF_GRA },           //  9 YMZ280b Samples

	{ "eeprom-stadhr96.bin",	0x000080, 0x77861793, 0 | BRF_PRG | BRF_OPT }, // 10 Default Settings
};

STD_ROM_PICK(stadhr96)
STD_ROM_FN(stadhr96)

static INT32 Stadhr96Init()
{
	return CommonArmInit(1);
}

struct BurnDriver BurnDrvStadhr96 = {
	"stadhr96", NULL, NULL, NULL, "1996",
	"Stadium Hero '96 (World, EAJ)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, stadhr96RomInfo, stadhr96RomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	Stadhr96Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Stadium Hero '96 (USA, EAH)

static struct BurnRomInfo stadhr96uRomDesc[] = {
	{ "eah00-0.2a",			0x080000, 0xf45b2ca0, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "eah01-0.2b",			0x080000, 0x328a2bca, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mcm-00.2e",			0x400000, 0xc1919c3c, 2 | BRF_GRA },           //  2 Sprites
	{ "mcm-01.8m",			0x400000, 0x2255d47d, 2 | BRF_GRA },           //  3
	{ "mcm-02.4e",			0x400000, 0x38c39822, 2 | BRF_GRA },           //  4
	{ "mcm-03.10m",			0x400000, 0x4bd84ca7, 2 | BRF_GRA },           //  5
	{ "mcm-04.6e",			0x400000, 0x7c0bd84c, 2 | BRF_GRA },           //  6
	{ "mcm-05.11m",			0x400000, 0x476f03d7, 2 | BRF_GRA },           //  7

	{ "eaf02-0.6h",			0x080000, 0xf95ad7ce, 3 | BRF_GRA },           //  8 Sprite Look-up Table

	{ "mcm-06.6a",			0x400000, 0xfbc178f3, 4 | BRF_GRA },           //  9 YMZ280b Samples

	{ "eeprom-stadhr96u.bin",	0x000080, 0x71d796ba, 0 | BRF_PRG | BRF_OPT }, // 10 Default Settings
};

STD_ROM_PICK(stadhr96u)
STD_ROM_FN(stadhr96u)

struct BurnDriver BurnDrvStadhr96u = {
	"stadhr96u", "stadhr96", NULL, NULL, "1996",
	"Stadium Hero '96 (USA, EAH)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, stadhr96uRomInfo, stadhr96uRomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	Stadhr96Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Stadium Hero '96 (Japan, EAD)

static struct BurnRomInfo stadhr96jRomDesc[] = {
	{ "ead00-4.2a",			0x080000, 0xb0adfc39, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "ead01-4.2b",			0x080000, 0x0b332820, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mcm-00.2e",			0x400000, 0xc1919c3c, 2 | BRF_GRA },           //  2 Sprites
	{ "mcm-01.8m",			0x400000, 0x2255d47d, 2 | BRF_GRA },           //  3
	{ "mcm-02.4e",			0x400000, 0x38c39822, 2 | BRF_GRA },           //  4
	{ "mcm-03.10m",			0x400000, 0x4bd84ca7, 2 | BRF_GRA },           //  5
	{ "mcm-04.6e",			0x400000, 0x7c0bd84c, 2 | BRF_GRA },           //  6
	{ "mcm-05.11m",			0x400000, 0x476f03d7, 2 | BRF_GRA },           //  7

	{ "ead02-0.6h",			0x080000, 0xf95ad7ce, 3 | BRF_GRA },           //  8 Sprite Look-up Table

	{ "mcm-06.6a",			0x400000, 0xfbc178f3, 4 | BRF_GRA },           //  9 YMZ280b Samples

	{ "eeprom-stadhr96j.bin",	0x000080, 0xcf98098f, 0 | BRF_PRG | BRF_OPT }, // 10 Default Settings
};

STD_ROM_PICK(stadhr96j)
STD_ROM_FN(stadhr96j)

struct BurnDriver BurnDrvStadhr96j = {
	"stadhr96j", "stadhr96", NULL, NULL, "1996",
	"Stadium Hero '96 (Japan, EAD)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, stadhr96jRomInfo, stadhr96jRomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	Stadhr96Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Hoops '96 (Europe/Asia 2.0)

static struct BurnRomInfo hoops96RomDesc[] = {
	{ "sz00-0.2a",			0x080000, 0x971b4376, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "sz01-0.2b",			0x080000, 0xb9679d7b, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mce-00.2e",			0x200000, 0x11b9bd96, 2 | BRF_GRA },           //  2 Sprites
	{ "mce-01.8m",			0x200000, 0x6817d0c6, 2 | BRF_GRA },           //  3
	{ "mce-02.4e",			0x200000, 0xbe7ff8ba, 2 | BRF_GRA },           //  4
	{ "mce-03.10m",			0x200000, 0x756c282e, 2 | BRF_GRA },           //  5
	{ "mce-04.8n",			0x200000, 0x91da9b4f, 2 | BRF_GRA },           //  6

	{ "rr02-0.6h",			0x020000, 0x9490041c, 3 | BRF_GRA },           //  7 Sprite Look-up Table

	{ "mce-05.6a",			0x400000, 0xe7a9355a, 4 | BRF_GRA },           //  8 YMZ280b Samples

	{ "hoops.nv",			0x000080, 0x67b18457, 0 | BRF_PRG | BRF_OPT }, //  9 Default Settings
};

STD_ROM_PICK(hoops96)
STD_ROM_FN(hoops96)

static INT32 Hoops96Init()
{
	return CommonArmInit(2);
}

struct BurnDriver BurnDrvHoops96 = {
	"hoops96", NULL, NULL, NULL, "1996",
	"Hoops '96 (Europe/Asia 2.0)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, hoops96RomInfo, hoops96RomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	Hoops96Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Hoops (Europe/Asia 1.7)

static struct BurnRomInfo hoops95RomDesc[] = {
	{ "hoops.a2",			0x080000, 0x02b8c61a, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "hoops.b2",			0x080000, 0xa1dc3519, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mce-00.2e",			0x200000, 0x11b9bd96, 2 | BRF_GRA },           //  2 Sprites
	{ "mce-01.8m",			0x200000, 0x6817d0c6, 2 | BRF_GRA },           //  3
	{ "mce-02.4e",			0x200000, 0xbe7ff8ba, 2 | BRF_GRA },           //  4
	{ "mce-03.10m",			0x200000, 0x756c282e, 2 | BRF_GRA },           //  5
	{ "mce-04.8n",			0x200000, 0x91da9b4f, 2 | BRF_GRA },           //  6

	{ "rl02-0.6h",			0x020000, 0x9490041c, 3 | BRF_GRA },           //  7 Sprite Look-up Table

	{ "mce-05.6a",			0x400000, 0xe7a9355a, 4 | BRF_GRA },           //  8 YMZ280b Samples

	{ "hoops.nv",			0x000080, 0x67b18457, 0 | BRF_PRG | BRF_OPT }, //  9 Default Settings
};

STD_ROM_PICK(hoops95)
STD_ROM_FN(hoops95)

struct BurnDriver BurnDrvHoops95 = {
	"hoops95", "hoops96", NULL, NULL, "1995",
	"Hoops (Europe/Asia 1.7)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, hoops95RomInfo, hoops95RomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	Hoops96Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};


// Dunk Dream '95 (Japan 1.4, EAM)

static struct BurnRomInfo ddream95RomDesc[] = {
	{ "rl00-2.2a",			0x080000, 0x07645092, 1 | BRF_PRG | BRF_ESS }, //  0 Arm Code (Encrypted)
	{ "rl01-2.2b",			0x080000, 0xcfc629fc, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mce-00.2e",			0x200000, 0x11b9bd96, 2 | BRF_GRA },           //  2 Sprites
	{ "mce-01.8m",			0x200000, 0x6817d0c6, 2 | BRF_GRA },           //  3
	{ "mce-02.4e",			0x200000, 0xbe7ff8ba, 2 | BRF_GRA },           //  4
	{ "mce-03.10m",			0x200000, 0x756c282e, 2 | BRF_GRA },           //  5
	{ "mce-04.8n",			0x200000, 0x91da9b4f, 2 | BRF_GRA },           //  6

	{ "rl02-0.6h",			0x020000, 0x9490041c, 3 | BRF_GRA },           //  7 Sprite Look-up Table

	{ "mce-05.6a",			0x400000, 0xe7a9355a, 4 | BRF_GRA },           //  8 YMZ280b Samples

	{ "hoops.nv",			0x000080, 0x67b18457, 0 | BRF_PRG | BRF_OPT }, //  9 Default Settings
};

STD_ROM_PICK(ddream95)
STD_ROM_FN(ddream95)

struct BurnDriver BurnDrvDdream95 = {
	"ddream95", "hoops96", NULL, NULL, "1995",
	"Dunk Dream '95 (Japan 1.4, EAM)\0", NULL, "Data East Corporation", "DECO MLC",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, ddream95RomInfo, ddream95RomName, NULL, NULL, NULL, NULL, MlcInputInfo, NULL,
	Hoops96Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	320, 240, 4, 3
};
