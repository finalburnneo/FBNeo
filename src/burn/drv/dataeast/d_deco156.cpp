// FB Alpha Data East 156 CPU-based games driver module
// Based on MAME driver by Bryan McPhail

#include "tiles_generic.h"
#include "arm_intf.h"
#include "msm6295.h"
#include "ymz280b.h"
#include "eeprom.h"
#include "deco16ic.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvArmROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvArmRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 oki_bank[2];

static UINT8 DrvJoy1[32];
static UINT32 DrvInputs[1];
static UINT8 DrvReset;

static INT32 has_ymz = 0;

static void (*palette_update)();

static struct BurnInputInfo HvysmshInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 16,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 17,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 18,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 19,	"diag"		},
};

STDINPUTINFO(Hvysmsh)

static void palette_update_hvysmsh()
{
	UINT32 *p = (UINT32*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/4; i++)
	{
		UINT8 r = p[i] >> 0;
		UINT8 g = p[i] >> 8;
		UINT8 b = p[i] >> 16;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void palette_update_wcvol95()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800/2; i++)
	{
		UINT8 r = (p[i] >>  0) & 0x1f;
		UINT8 g = (p[i] >>  5) & 0x1f;
		UINT8 b = (p[i] >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void set_oki0_bank(INT32 data)
{
	oki_bank[0] = data;
	MSM6295SetBank(0, DrvSndROM0 + (data & 1) * 0x40000, 0, 0x3ffff);
}

static void set_oki1_bank(INT32 data)
{
	oki_bank[1] = data;
	MSM6295SetBank(1, DrvSndROM1 + (data & 7) * 0x40000, 0, 0x3ffff);
}

static UINT32 hvysmsh_read_long(UINT32 address)
{
	Read16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Read16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Read16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a0fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a4fff) // 16-bit

	Read16Long(DrvSprRAM,				0x1e0000, 0x1e1fff) // 16-bit

	switch (address)
	{
		case 0x120000:
			return (DrvInputs[0] & ~0x01100000) | (deco16_vblank ? 0x00100000 : 0) | (EEPROMRead() ? 0x01000000 : 0);

		case 0x140000:
			return MSM6295Read(0);

		case 0x160000:
			return MSM6295Read(1);

		case 0x1d0010:
		case 0x1d0014:
		case 0x1d0018:
		case 0x1d001c:
		case 0x1d0020:
		case 0x1d0024:
		case 0x1d0028:
		case 0x1d002c:
			return 0; // ?
	}

	return 0;
}

static UINT8 hvysmsh_read_byte(UINT32 address)
{
	Read16Byte(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Read16Byte(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Read16Byte(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Read16Byte(deco16_pf_rowscroll[0],		0x1a0000, 0x1a0fff) // 16-bit
	Read16Byte(deco16_pf_rowscroll[1],		0x1a4000, 0x1a4fff) // 16-bit

	Read16Byte(DrvSprRAM,				0x1e0000, 0x1e1fff) // 16-bit

	switch (address)
	{
		case 0x120000: {
			UINT32 ret = (DrvInputs[0] & ~0x01100000) | (deco16_vblank ? 0x00100000 : 0) | (EEPROMRead() ? 0x01000000 : 0);
			return ret >> ((address & 3) * 8);
		}

		case 0x140000:
			return MSM6295Read(0);

		case 0x160000:
			return MSM6295Read(1);

		case 0x1d0010:
		case 0x1d0014:
		case 0x1d0018:
		case 0x1d001c:
		case 0x1d0020:
		case 0x1d0024:
		case 0x1d0028:
		case 0x1d002c:
			return 0; // ?
	}

	return 0;
}

static void hvysmsh_write_long(UINT32 address, UINT32 data)
{
	Write16Long(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Write16Long(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Write16Long(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[0],		0x1a0000, 0x1a0fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[1],		0x1a4000, 0x1a4fff) // 16-bit

	Write16Long(DrvSprRAM,				0x1e0000, 0x1e1fff) // 16-bit

	switch (address)
	{
		case 0x120000:
			// volume
		return;

		case 0x120004:
			set_oki1_bank(data);
			EEPROMWrite(data & 0x20, data & 0x40, data & 0x10);
		return;

		case 0x120008:
			// irq ack?
		return;

		case 0x12000c:
			set_oki0_bank(data);
		return;

		case 0x140000:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x160000:
			MSM6295Write(1, data & 0xff);
		return;
	}
}

static void hvysmsh_write_byte(UINT32 address, UINT8 data)
{
	Write16Byte(((UINT8*)deco16_pf_control[0]),	0x180000, 0x18001f) // 16-bit
	Write16Byte(deco16_pf_ram[0],			0x190000, 0x191fff) // 16-bit
	Write16Byte(deco16_pf_ram[1],			0x194000, 0x195fff) // 16-bit
	Write16Byte(deco16_pf_rowscroll[0],		0x1a0000, 0x1a0fff) // 16-bit
	Write16Byte(deco16_pf_rowscroll[1],		0x1a4000, 0x1a4fff) // 16-bit

	Write16Byte(DrvSprRAM,				0x1e0000, 0x1e1fff) // 16-bit

	switch (address)
	{
		case 0x120000:
			// volume
		return;

		case 0x120004:
			set_oki1_bank(data);
			EEPROMWrite(data & 0x20, data & 0x40, data & 0x10);
		return;

		case 0x120008:
			// irq ack?
		return;

		case 0x12000c:
			set_oki0_bank(data);
		return;

		case 0x140000:
			MSM6295Write(0, data & 0xff);
		return;

		case 0x160000:
			MSM6295Write(1, data & 0xff);
		return;
	}
}

static UINT32 wcvol95_read_long(UINT32 address)
{
	Read16Long(((UINT8*)deco16_pf_control[0]),	0x100000, 0x10001f) // 16-bit
	Read16Long(deco16_pf_ram[0],			0x110000, 0x111fff) // 16-bit
	Read16Long(deco16_pf_ram[1],			0x114000, 0x115fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[0],		0x120000, 0x120fff) // 16-bit
	Read16Long(deco16_pf_rowscroll[1],		0x124000, 0x124fff) // 16-bit

	Read16Long(DrvSprRAM,				0x160000, 0x161fff) // 16-bit
	Read16Long(DrvPalRAM,				0x180000, 0x180fff) // 16-bit

	switch (address)
	{
		case 0x140000:
			return (DrvInputs[0] & ~0x01100000) | (deco16_vblank ? 0x00100000 : 0) | (EEPROMRead() ? 0x01000000 : 0);

		case 0x1a0000:
		case 0x1a0004:
			return YMZ280BRead((address / 4) & 1);
	}

	return 0;
}

static UINT8 wcvol95_read_byte(UINT32 address)
{
	Read16Byte(((UINT8*)deco16_pf_control[0]),	0x100000, 0x10001f) // 16-bit
	Read16Byte(deco16_pf_ram[0],			0x110000, 0x111fff) // 16-bit
	Read16Byte(deco16_pf_ram[1],			0x114000, 0x115fff) // 16-bit
	Read16Byte(deco16_pf_rowscroll[0],		0x120000, 0x120fff) // 16-bit
	Read16Byte(deco16_pf_rowscroll[1],		0x124000, 0x124fff) // 16-bit

	Read16Byte(DrvSprRAM,				0x160000, 0x161fff) // 16-bit
	Read16Byte(DrvPalRAM,				0x180000, 0x180fff) // 16-bit

	switch (address)
	{
		case 0x140000: {
			UINT32 ret = (DrvInputs[0] & ~0x01100000) | (deco16_vblank ? 0x00100000 : 0) | (EEPROMRead() ? 0x01000000 : 0);
			return ret >> ((address & 3) * 8);
		}

		case 0x1a0000:
		case 0x1a0001:
		case 0x1a0002:
		case 0x1a0003:
		case 0x1a0004:
		case 0x1a0005:
		case 0x1a0006:
		case 0x1a0007:
			return YMZ280BRead((address / 4) & 1);
	}

	return 0;
}

static void wcvol95_write_long(UINT32 address, UINT32 data)
{
	Write16Long(((UINT8*)deco16_pf_control[0]),	0x100000, 0x10001f) // 16-bit
	Write16Long(deco16_pf_ram[0],			0x110000, 0x111fff) // 16-bit
	Write16Long(deco16_pf_ram[1],			0x114000, 0x115fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[0],		0x120000, 0x120fff) // 16-bit
	Write16Long(deco16_pf_rowscroll[1],		0x124000, 0x124fff) // 16-bit

	Write16Long(DrvSprRAM,				0x160000, 0x161fff) // 16-bit
	Write16Long(DrvPalRAM,				0x180000, 0x180fff) // 16-bit

	switch (address)
	{
		case 0x150000:
			EEPROMWrite(data & 0x02, data & 0x04, data & 0x01);
		return;

		case 0x170000:
		return; // nop

		case 0x1a0000:
		case 0x1a0004:
			YMZ280BWrite((address / 4) & 1, data & 0xff);
		return;
	}
}

static void wcvol95_write_byte(UINT32 address, UINT8 data)
{
	Write16Byte(((UINT8*)deco16_pf_control[0]),	0x100000, 0x10001f) // 16-bit
	Write16Byte(deco16_pf_ram[0],			0x110000, 0x111fff) // 16-bit
	Write16Byte(deco16_pf_ram[1],			0x114000, 0x115fff) // 16-bit
	Write16Byte(deco16_pf_rowscroll[0],		0x120000, 0x120fff) // 16-bit
	Write16Byte(deco16_pf_rowscroll[1],		0x124000, 0x124fff) // 16-bit

	Write16Byte(DrvSprRAM,				0x160000, 0x161fff) // 16-bit
	Write16Byte(DrvPalRAM,				0x180000, 0x180fff) // 16-bit

	switch (address)
	{
		case 0x150000:
			EEPROMWrite(data & 0x02, data & 0x04, data & 0x01);
		return;

		case 0x1a0000:
		case 0x1a0004:
			YMZ280BWrite((address / 4) & 1, data);
		return;
	}
}

static INT32 bank_callback(const INT32 bank)
{
	return (bank & 0x70) * 0x100;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ArmOpen(0);
	ArmReset();
	ArmClose();

	if (has_ymz)
	{
		YMZ280BReset();
	}
	else
	{
		set_oki0_bank(0);
		set_oki1_bank(0);
		MSM6295Reset();
	}

	EEPROMReset();

	deco16Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvArmROM		= Next; Next += 0x0100000;

	DrvGfxROM0		= Next; Next += 0x0400000;
	DrvGfxROM1		= Next; Next += 0x0400000;
	DrvGfxROM2		= Next; Next += 0x0800000;

	MSM6295ROM		= Next;
	DrvSndROM0		= Next; Next += 0x0080000;
	YMZ280BROM		= Next; 
	DrvSndROM1		= Next; Next += 0x0200000;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam			= Next;

	DrvArmRAM		= Next; Next += 0x0008000;
	DrvPalRAM		= Next; Next += 0x0001000;
	DrvSprRAM		= Next; Next += 0x0001000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void descramble_sound(UINT8 *rom, INT32 len)
{
	UINT8 *buf = (UINT8*)BurnMalloc(len);

	for (INT32 i = 0; i < len; i++)
	{
		buf[((i & 1) << 20) | ((i >> 1) & 0xfffff)] = rom[i];
	}

	memcpy (rom, buf, len);

	BurnFree (buf);
}

static INT32 HvysmshInit()
{
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRomExt(DrvArmROM + 0x000002,   0, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvArmROM + 0x000000,   1, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,     2, 1)) return 1;

		for (INT32 i = 0; i < 0x200000; i++) {
			DrvGfxROM1[(i & 0x7ffff) | ((i & 0x80000) << 1) | ((i & 0x100000) >> 1)] = DrvGfxROM0[i];
		}

		if (BurnLoadRom(DrvGfxROM2 + 0x000001,     3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,     4, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,     5, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,     6, 1)) return 1;

		deco156_decrypt(DrvArmROM, 0x100000);
		deco56_decrypt_gfx(DrvGfxROM1, 0x200000);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x200000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x200000, 0);
		deco16_sprite_decode(DrvGfxROM2, 0x400000);
		descramble_sound(DrvSndROM1, 0x200000);
	}

	ArmInit(0);
	ArmOpen(0);
	ArmMapMemory(DrvArmROM,		0x000000, 0x0fffff, MAP_ROM);
	ArmMapMemory(DrvArmRAM,		0x100000, 0x107fff, MAP_RAM);
	ArmMapMemory(DrvPalRAM,		0x1c0000, 0x1c0fff, MAP_RAM);
	ArmSetWriteByteHandler(hvysmsh_write_byte);
	ArmSetWriteLongHandler(hvysmsh_write_long);
	ArmSetReadByteHandler(hvysmsh_read_byte);
	ArmSetReadLongHandler(hvysmsh_read_long);
	ArmClose();

	deco16Init(1, 0, 1);
	deco16_set_bank_callback(0, bank_callback);
	deco16_set_bank_callback(1, bank_callback);
	deco16_set_color_base(0, 0x000);
	deco16_set_color_base(1, 0x100);
	deco16_set_graphics(DrvGfxROM0, 0x400000, DrvGfxROM1, 0x400000, DrvGfxROM1, 0x100);
	deco16_set_global_offsets(0, 8);

	MSM6295Init(0, (28000000 / 28) / MSM6295_PIN7_HIGH, 0);
	MSM6295Init(1, (28000000 / 14) / MSM6295_PIN7_HIGH, 1);

	EEPROMInit(&eeprom_interface_93C46);

	palette_update = palette_update_hvysmsh;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 Wcvol95Init()
{
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRomExt(DrvArmROM + 0x000002,   0, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(DrvArmROM + 0x000000,   1, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,     2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000001,     3, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000000,     4, 2)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000,     5, 1)) return 1;

		deco156_decrypt(DrvArmROM, 0x100000);
		deco56_decrypt_gfx(DrvGfxROM1, 0x080000);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x080000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x080000, 0);
		deco16_sprite_decode(DrvGfxROM2, 0x200000);
		memcpy (DrvGfxROM2 + 0x400000, DrvGfxROM2, 0x400000);
		descramble_sound(DrvSndROM1, 0x200000);
	}

	ArmInit(0);
	ArmOpen(0);
	ArmMapMemory(DrvArmROM,		0x000000, 0x0fffff, MAP_ROM);
	ArmMapMemory(DrvArmRAM,		0x130000, 0x137fff, MAP_RAM);
	ArmSetWriteByteHandler(wcvol95_write_byte);
	ArmSetWriteLongHandler(wcvol95_write_long);
	ArmSetReadByteHandler(wcvol95_read_byte);
	ArmSetReadLongHandler(wcvol95_read_long);
	ArmClose();

	deco16Init(1, 0, 1);
	deco16_set_bank_callback(0, bank_callback);
	deco16_set_bank_callback(1, bank_callback);
	deco16_set_color_base(0, 0x000);
	deco16_set_color_base(1, 0x100);
	deco16_set_graphics(DrvGfxROM0, 0x100000, DrvGfxROM1, 0x100000, DrvGfxROM1, 0x100);
	deco16_set_global_offsets(0, 8);

	has_ymz = 1;
	YMZ280BInit(14000000, NULL);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	YMZ280BSetRoute(BURN_SND_YMZ280B_YMZ280B_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	EEPROMInit(&eeprom_interface_93C46);

	palette_update = palette_update_wcvol95;

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	EEPROMExit();

	ArmExit();

	if (has_ymz) {
		YMZ280BExit();
		YMZ280BROM = NULL;
	} else {
		MSM6295Exit();
	}

	MSM6295ROM = NULL;

	GenericTilesExit();

	deco16Exit();

	BurnFree (AllMem);

	has_ymz = 0;

	return 0;
}

static void draw_sprites(UINT16 *dest, UINT8 *spriteram, UINT8 *gfx, INT32 coloff)
{
	UINT16 *ram = (UINT16*)spriteram;

	int flipscreen = 1;

	for (INT32 offs = 0x800-4; offs >= 0; offs -= 4)
	{
		INT32 inc, mult;

		INT32 sy     = BURN_ENDIAN_SWAP_INT16(ram[offs + 0]);
		INT32 code   = BURN_ENDIAN_SWAP_INT16(ram[offs + 1]) & 0x7fff;
		INT32 sx     = BURN_ENDIAN_SWAP_INT16(ram[offs + 2]);

		if ((sy & 0x1000) && (nCurrentFrame & 1)) continue;

		INT32 color = (sx >> 9) & 0x1f;

		INT32 pri = sx & 0xc000;

		switch (pri & 0xc000)
		{
			case 0x0000: pri = 0;   break;
			case 0x4000: pri = 0xf0;break;
			case 0x8000: pri = 0xfc;break;
			case 0xc000: pri = 0xfc;break;
		}

		INT32 flipx = sy & 0x2000;
		INT32 flipy = sy & 0x4000;
		INT32 multi = (1 << ((sy & 0x0600) >> 9)) - 1;

		sx &= 0x01ff;
		sy &= 0x01ff;
		if (sx >= 320) sx -= 512;
		if (sy >= 256) sy -= 512;
		sy = 240 - sy;
		sx = 304 - sx;

		code &= ~multi;

		if (flipy) {
			inc = -1;
		} else {
			code += multi;
			inc = 1;
		}

		if (flipscreen)
		{
			sy = 240 - sy;
			sx = 304 - sx;
			flipx = !flipx;
			flipy = !flipy;
			mult = 16;
		}
		else
			mult = -16;

		if (sx >= 320 || sx < -15) continue;

		while (multi >= 0)
		{
			INT32 y = (sy + mult * multi);
			INT32 c = (code - multi * inc) & 0x7fff;

			deco16_draw_prio_sprite(dest, gfx, c, (color*16)+coloff, sx, y, flipx, flipy, pri);

			multi--;
		}
	}
}

static INT32 DrvDraw()
{
	palette_update();

	BurnTransferClear();

	deco16_pf12_update();
	deco16_clear_prio_map();

	deco16_draw_layer(1, pTransDraw, 0 | DECO16_LAYER_OPAQUE);

	draw_sprites(pTransDraw, DrvSprRAM, DrvGfxROM2, 0x200);
	deco16_draw_layer(0, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

inline static void ClearOpposites32(UINT32* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x00) {
		*nJoystickInputs |= 0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x00) {
		*nJoystickInputs |= 0x0c;
	}

	if ((*nJoystickInputs & 0x0300) == 0x0000) {
		*nJoystickInputs |= 0x0300;
	}
	if ((*nJoystickInputs & 0x0c00) == 0x0000) {
		*nJoystickInputs |= 0x0c00;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = ~0;

		for (INT32 i = 0; i < 32; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		ClearOpposites32(&DrvInputs[0]);
	}

	INT32 nTotalCycles = 28000000 / 58;

	ArmOpen(0);
	deco16_vblank = 0;
	ArmRun(nTotalCycles - 2240);
	ArmSetIRQLine(ARM_IRQ_LINE, CPU_IRQSTATUS_AUTO);
	deco16_vblank = 1;
	ArmRun(2240);
	ArmClose();

	if (pBurnSoundOut) {
		if (has_ymz) {
			YMZ280BRender(pBurnSoundOut, nBurnSoundLen);
		} else {
			MSM6295Render(pBurnSoundOut, nBurnSoundLen);
		}
	}

	if (pBurnDraw) {
		DrvDraw();
	}

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
		ArmScan(nAction);

		SCAN_VAR(oki_bank);

		if (has_ymz) {
			YMZ280BScan(nAction, pnMin);
		} else {
			MSM6295Scan(nAction, pnMin);
			set_oki0_bank(oki_bank[0]);
			set_oki1_bank(oki_bank[1]);
		}

		deco16Scan();
	}

	return 0;
}


// Heavy Smash (Europe version -2)

static struct BurnRomInfo hvysmshRomDesc[] = {
	{ "lt00-2.2j",		0x080000, 0xf6e10fc0, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "lt01-2.3j",		0x080000, 0xce2a75e2, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mbg-00.9a",		0x200000, 0x7d94eb16, 2 | BRF_GRA },           //  2 Tilemap 0&1 Characters & Tiles

	{ "mbg-01.10a",		0x200000, 0xbcd7fb29, 3 | BRF_GRA },           //  3 Sprites
	{ "mbg-02.11a",		0x200000, 0x0cc16440, 3 | BRF_GRA },           //  4

	{ "mbg-03.10k",		0x080000, 0x4b809420, 4 | BRF_SND },           //  5 MSM6295 #0 Samples

	{ "mbg-04.11k",		0x200000, 0x2281c758, 5 | BRF_SND },           //  6 MSM6295 #1 Samples
};

STD_ROM_PICK(hvysmsh)
STD_ROM_FN(hvysmsh)

struct BurnDriver BurnDrvHvysmsh = {
	"hvysmsh", NULL, NULL, NULL, "1993",
	"Heavy Smash (Europe version -2)\0", NULL, "Data East Corporation", "DECO 156",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, hvysmshRomInfo, hvysmshRomName, NULL, NULL, NULL, NULL, HvysmshInputInfo, NULL,
	HvysmshInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Heavy Smash (Japan version -2)

static struct BurnRomInfo hvysmshjRomDesc[] = {
	{ "lp00-2.2j",		0x080000, 0x3f8fd724, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "lp01-2.3j",		0x080000, 0xa6fe282a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mbg-00.9a",		0x200000, 0x7d94eb16, 2 | BRF_GRA },           //  2 Tilemap 0&1 Characters & Tiles

	{ "mbg-01.10a",		0x200000, 0xbcd7fb29, 3 | BRF_GRA },           //  3 Sprites
	{ "mbg-02.11a",		0x200000, 0x0cc16440, 3 | BRF_GRA },           //  4

	{ "mbg-03.10k",		0x080000, 0x4b809420, 4 | BRF_SND },           //  5 MSM6295 #0 Samples

	{ "mbg-04.11k",		0x200000, 0x2281c758, 5 | BRF_SND },           //  6 MSM6295 #1 Samples
};

STD_ROM_PICK(hvysmshj)
STD_ROM_FN(hvysmshj)

struct BurnDriver BurnDrvHvysmshj = {
	"hvysmshj", "hvysmsh", NULL, NULL, "1993",
	"Heavy Smash (Japan version -2)\0", NULL, "Data East Corporation", "DECO 156",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, hvysmshjRomInfo, hvysmshjRomName, NULL, NULL, NULL, NULL, HvysmshInputInfo, NULL,
	HvysmshInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// Heavy Smash (Asia version -4)

static struct BurnRomInfo hvysmshaRomDesc[] = {
	{ "xx00-4.2j",		0x080000, 0x333a92c1, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "xx01-4.3j",		0x080000, 0x8c24c5ed, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mbg-00.9a",		0x200000, 0x7d94eb16, 2 | BRF_GRA },           //  2 Tilemap 0&1 Characters & Tiles

	{ "mbg-01.10a",		0x200000, 0xbcd7fb29, 3 | BRF_GRA },           //  3 Sprites
	{ "mbg-02.11a",		0x200000, 0x0cc16440, 3 | BRF_GRA },           //  4

	{ "mbg-03.10k",		0x080000, 0x4b809420, 4 | BRF_SND },           //  5 MSM6295 #0 Samples

	{ "mbg-04.11k",		0x200000, 0x2281c758, 5 | BRF_SND },           //  6 MSM6295 #1 Samples
};

STD_ROM_PICK(hvysmsha)
STD_ROM_FN(hvysmsha)

struct BurnDriver BurnDrvHvysmsha = {
	"hvysmsha", "hvysmsh", NULL, NULL, "1993",
	"Heavy Smash (Asia version -4)\0", NULL, "Data East Corporation", "DECO 156",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, hvysmshaRomInfo, hvysmshaRomName, NULL, NULL, NULL, NULL, HvysmshInputInfo, NULL,
	HvysmshInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// World Cup Volley '95 (Asia v1.0)

static struct BurnRomInfo wcvol95RomDesc[] = {
	{ "pw00-0.2f",		0x080000, 0x86765209, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "pw01-0.4f",		0x080000, 0x3a0ee861, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mbx-00.9a",		0x080000, 0xa0b24204, 2 | BRF_GRA },           //  2 Tilemap 0&1 Characters & Tiles

	{ "mbx-01.12a",		0x100000, 0x73deb3f1, 3 | BRF_GRA },           //  3 Sprites
	{ "mbx-02.13a",		0x100000, 0x3204d324, 3 | BRF_GRA },           //  4

	{ "mbx-03.13j",		0x200000, 0x061632bc, 4 | BRF_GRA },           //  5 YMZ280b Samples

	{ "gal16v8b.10j.bin",	0x000117, 0x06bbcbd5, 5 | BRF_GRA },           //  6 GALs
	{ "gal16v8b.5d.bin",	0x000117, 0x117784f0, 5 | BRF_GRA },           //  7
};

STD_ROM_PICK(wcvol95)
STD_ROM_FN(wcvol95)

struct BurnDriver BurnDrvWcvol95 = {
	"wcvol95", NULL, NULL, NULL, "1995",
	"World Cup Volley '95 (Asia v1.0)\0", NULL, "Data East Corporation", "DECO 156",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, wcvol95RomInfo, wcvol95RomName, NULL, NULL, NULL, NULL, HvysmshInputInfo, NULL,
	Wcvol95Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// World Cup Volley '95 (Japan v1.0)

static struct BurnRomInfo wcvol95jRomDesc[] = {
	{ "pn00-0.2f",		0x080000, 0xc9ed2006, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "pn01-0.4f",		0x080000, 0x1c3641c3, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mbx-00.9a",		0x080000, 0xa0b24204, 2 | BRF_GRA },           //  2 Tilemap 0&1 Characters & Tiles

	{ "mbx-01.12a",		0x100000, 0x73deb3f1, 3 | BRF_GRA },           //  3 Sprites
	{ "mbx-02.13a",		0x100000, 0x3204d324, 3 | BRF_GRA },           //  4

	{ "mbx-03.13j",		0x200000, 0x061632bc, 4 | BRF_GRA },           //  5 YMZ280b Samples

	{ "gal16v8b.10j.bin",	0x000117, 0x06bbcbd5, 5 | BRF_GRA },           //  6 GALs
	{ "gal16v8b.5d.bin",	0x000117, 0x117784f0, 5 | BRF_GRA },           //  7
};

STD_ROM_PICK(wcvol95j)
STD_ROM_FN(wcvol95j)

struct BurnDriver BurnDrvWcvol95j = {
	"wcvol95j", "wcvol95", NULL, NULL, "1995",
	"World Cup Volley '95 (Japan v1.0)\0", NULL, "Data East Corporation", "DECO 156",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, wcvol95jRomInfo, wcvol95jRomName, NULL, NULL, NULL, NULL, HvysmshInputInfo, NULL,
	Wcvol95Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};


// World Cup Volley '95 Extra Version (Asia v2.0B)

static struct BurnRomInfo wcvol95xRomDesc[] = {
	{ "2f.bin",		0x080000, 0xac06633d, 1 | BRF_PRG | BRF_ESS }, //  0 ARM Code
	{ "4f.bin",		0x080000, 0xe211f67a, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "mbx-00.9a",		0x080000, 0xa0b24204, 2 | BRF_GRA },           //  2 Tilemap 0&1 Characters & Tiles

	{ "mbx-01.12a",		0x100000, 0x73deb3f1, 3 | BRF_GRA },           //  3 Sprites
	{ "mbx-02.13a",		0x100000, 0x3204d324, 3 | BRF_GRA },           //  4

	{ "mbx-03.13j",		0x200000, 0x061632bc, 4 | BRF_GRA },           //  5 YMZ280b Samples

	{ "gal16v8b.10j.bin",	0x000117, 0x06bbcbd5, 5 | BRF_GRA },           //  6 GALs
	{ "gal16v8b.5d.bin",	0x000117, 0x117784f0, 5 | BRF_GRA },           //  7
};

STD_ROM_PICK(wcvol95x)
STD_ROM_FN(wcvol95x)

struct BurnDriver BurnDrvWcvol95x = {
	"wcvol95x", "wcvol95", NULL, NULL, "1995",
	"World Cup Volley '95 Extra Version (Asia v2.0B)\0", NULL, "Data East Corporation", "DECO 156",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_DATAEAST, GBF_SPORTSMISC, 0,
	NULL, wcvol95xRomInfo, wcvol95xRomName, NULL, NULL, NULL, NULL, HvysmshInputInfo, NULL,
	Wcvol95Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	320, 240, 4, 3
};
