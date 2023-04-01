// FB Alpha GI Joe driver module
// Based on MAME driver by Olivier Galibert

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "k054539.h"
#include "eeprom.h"

static UINT8 *AllMem;
static UINT8 *Drv68KROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM;
static UINT8 *DrvEeprom;
static UINT8 *AllRam;
static UINT8 *Drv68KRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPalRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT8 *soundlatch;
static UINT8 *soundlatch2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 layerpri[4];
static INT32 layer_colorbase[4];
static INT32 sprite_colorbase;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];
static UINT8 DrvDips[3];

static INT32 avac_bits[4];
static INT32 avac_occupancy[4];
static INT32 avac_vrc;

static INT32 sound_nmi_enable;
static INT32 irq6_timer;
static UINT16 control_data;

static struct BurnInputInfo GijoeInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 2,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 3,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 10,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 11,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 8,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 9,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 14,	"p2 fire 3"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy2 + 2,	"p3 coin"	},
	{"P3 Start",		BIT_DIGITAL,	DrvJoy1 + 2,	"p3 start"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy4 + 2,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy4 + 3,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy4 + 0,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy4 + 1,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy4 + 4,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy4 + 5,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy4 + 6,	"p3 fire 3"	},

	{"P4 Coin",			BIT_DIGITAL,	DrvJoy2 + 3,	"p4 coin"	},
	{"P4 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p4 start"	},
	{"P4 Up",			BIT_DIGITAL,	DrvJoy4 + 10,	"p4 up"		},
	{"P4 Down",			BIT_DIGITAL,	DrvJoy4 + 11,	"p4 down"	},
	{"P4 Left",			BIT_DIGITAL,	DrvJoy4 + 8,	"p4 left"	},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy4 + 9,	"p4 right"	},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy4 + 12,	"p4 fire 1"	},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy4 + 13,	"p4 fire 2"	},
	{"P4 Button 3",		BIT_DIGITAL,	DrvJoy4 + 14,	"p4 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 9,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 10,	"service"	},
	{"Service 4",		BIT_DIGITAL,	DrvJoy2 + 11,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	DrvJoy1 + 11,	"diag"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Gijoe)

static struct BurnDIPInfo GijoeDIPList[]=
{
	{0x2a, 0xff, 0xff, 0x00, NULL				},
	{0x2b, 0xff, 0xff, 0x80, NULL				},
	{0x2c, 0xff, 0xff, 0x80, NULL				},

	{0   , 0xfe, 0   ,    2, "Sound"			},
	{0x2a, 0x01, 0x80, 0x80, "Mono"				},
	{0x2a, 0x01, 0x80, 0x00, "Stereo"			},

	{0   , 0xfe, 0   ,    2, "Coin mechanism"	},
	{0x2b, 0x01, 0x80, 0x80, "Common"			},
	{0x2b, 0x01, 0x80, 0x00, "Independent"		},

	{0   , 0xfe, 0   ,    2, "Number of Players"},
	{0x2c, 0x01, 0x80, 0x80, "2"				},
	{0x2c, 0x01, 0x80, 0x00, "4"				},
};

STDDIPINFO(Gijoe)

static void gijoe_objdma()
{
	UINT16 *src_head, *src_tail, *dst_head, *dst_tail;

	src_head = (UINT16*)DrvSprRAM;
	src_tail = src_head + 255 * 8;
	dst_head = (UINT16*)K053247Ram;
	dst_tail = dst_head + 255 * 8;

	for (; src_head <= src_tail; src_head += 8)
	{
		if (BURN_ENDIAN_SWAP_INT16(*src_head) & 0x8000)
		{
			memcpy(dst_head, src_head, 0x10);
			dst_head += 8;
		}
		else
		{
			*dst_tail = 0;
			dst_tail -= 8;
		}
	}
}

static void __fastcall gijoe_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffff8) == 0x110000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xffc000) == 0x120000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}

	if ((address & 0xfffff8) == 0x160000) {
		return;	// regsb
	}

	if ((address & 0xffffc0) == 0x1b0000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	switch (address)
	{
		case 0x1c000c:
			*soundlatch = data;
		break;

		case 0x1d0000:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static void __fastcall gijoe_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffff8) == 0x110000) {
		K053246Write((address & 0x07) ^ 0, data);
		return;
	}

	if ((address & 0xffc000) == 0x120000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	if ((address & 0xfffff8) == 0x160000) {
		return;	// regsb
	}

	if ((address & 0xffffe1) == 0x1a0001) {
		K053251Write((address / 2) & 0xf, data);
		return;
	}

	if ((address & 0xffffc0) == 0x1b0000) {
		K056832ByteWrite(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x170000:
		return;		// watchdog

		case 0x1e8001:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
			K053246_set_OBJCHA_line((data & 0x40) >> 6);
			control_data = data;
		return;

		case 0x1c000c:
		case 0x1c000d:
			*soundlatch = data;
		break;

		case 0x1d0000:
		case 0x1d0001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

	}
}

static UINT16 __fastcall gijoe_main_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x120000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x130000) {
		return K056832RomWordRead(address);
	}

	switch (address)
	{
		case 0x1e0000:
			return DrvInputs[2];

		case 0x1e0002:
			return DrvInputs[3];

		case 0x1e4000:
			return DrvInputs[1];

		case 0x1e4002:
			return (DrvInputs[0] & 0xfeff) | (EEPROMRead() ? 0x0100 : 0);

		case 0x1c0014:
			return *soundlatch2;

		case 0x1f0000:
			return K053246Read(1) + (K053246Read(0) << 8); // ?
	}

	return 0;
}

static UINT8 __fastcall gijoe_main_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x120000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x130000) {
		return K056832RomWordRead(address) >> ((~address & 1) * 8);
	}

	switch (address)
	{
		case 0x1e0000:
			return DrvInputs[2] >> 8;

		case 0x1e0001:
			return DrvInputs[2];

		case 0x1e0002:
			return DrvInputs[3] >> 8;

		case 0x1e0003:
			return DrvInputs[3];

		case 0x1e4000:
			return DrvInputs[1] >> 8;

		case 0x1e4001:
			return DrvInputs[1];

		case 0x1e4002:
			return ((DrvInputs[0] >> 8) & 0xfe) | (EEPROMRead() ? 0x01 : 0);

		case 0x1e4003:
			return DrvInputs[0];

		case 0x1c0014:
		case 0x1c0015:
			return *soundlatch2;

		case 0x1f0000:
		case 0x1f0001:
			return K053246Read((address & 1));
	}

	return 0;
}

static void __fastcall gijoe_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0xf800 && address <= 0xfa2f) {
		if (address == 0xfa2f) sound_nmi_enable = data & 0x20;
		K054539Write(0, address & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0xfc00:
			*soundlatch2 = data;
		return;
	}
}

static UINT8 __fastcall gijoe_sound_read(UINT16 address)
{
	if (address >= 0xf800 && address <= 0xfa2f) {
		return K054539Read(0, address & 0x3ff);
	}

	switch (address)
	{
		case 0xfc02:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;
	}

	return 0;
}

static void gijoe_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 pri = (*color & 0x03e0) >> 4;

	if (pri <= layerpri[3])								*priority = 0x0000;
	else if (pri > layerpri[3] && pri <= layerpri[2])	*priority = 0xff00;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority = 0xfff0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority = 0xfffc;
	else												*priority = 0xfffe;

	*color = sprite_colorbase | (*color & 0x001f);
}

static void gijoe_tile_callback(INT32 layer, INT32 *code, INT32 *color, INT32 */*flags*/)
{
	INT32 tile = *code;

	if (tile >= 0xf000 && tile <= 0xf4ff)
	{
		tile &= 0x0fff;
		if (tile < 0x0310)
		{
			avac_occupancy[layer] |= 0x0f00;
			tile |= avac_bits[0];
		}
		else if (tile < 0x0470)
		{
			avac_occupancy[layer] |= 0xf000;
			tile |= avac_bits[1];
		}
		else
		{
			avac_occupancy[layer] |= 0x00f0;
			tile |= avac_bits[2];
		}
		*code = tile;
	}

	*color = (*color >> 2 & 0x0f) | layer_colorbase[layer];
}

static const eeprom_interface gijoe_eeprom_interface =
{
	7,
	8,
	"011000",
	"011100",
	"0100100000000",
	"0100000000000",
	"0100110000000",
	0,
	0
};

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	KonamiICReset();

	K054539Reset(0);

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEeprom, 0, 128);
	}

	control_data = 0;

	for (INT32 i = 0; i < 4; i++)
	{
		avac_occupancy[i] = 0;
		avac_bits[i] = 0;
		layer_colorbase[i] = 0;
		layerpri[i] = 0;
	}

	avac_vrc = 0xffff;
	sound_nmi_enable = 0;

	irq6_timer = -1;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROMExp0	= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROMExp1	= Next; Next += 0x800000;

	DrvSndROM		= Next; Next += 0x200000;

	DrvEeprom		= Next; Next += 0x000080;

	konami_palette32= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x001000;

	DrvZ80RAM		= Next; Next += 0x000800;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x080000,  3, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  4, 1)) return 1;

		// gijoeua has partial gfx dumps that are currently unused,
		// they are being overridden by bad dumps, so let's skip them for now
		INT32 skipped_roms = (strcmp(BurnDrvGetTextA(DRV_NAME), "gijoeua") == 0 ? 4 : 0);

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5 + skipped_roms, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6 + skipped_roms, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7 + skipped_roms, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8 + skipped_roms, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  9 + skipped_roms, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 10 + skipped_roms, 8, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 11 + skipped_roms, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 12 + skipped_roms, 1)) return 1;

		K053247GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x200000);
		K053247GfxDecode(DrvGfxROM1, DrvGfxROMExp1, 0x400000);
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(DrvSprRAM,			0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x180000, 0x18ffff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x190000, 0x190fff, MAP_RAM);
	SekSetWriteWordHandler(0,		gijoe_main_write_word);
	SekSetWriteByteHandler(0,		gijoe_main_write_byte);
	SekSetReadWordHandler(0,		gijoe_main_read_word);
	SekSetReadByteHandler(0,		gijoe_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(gijoe_sound_write);
	ZetSetReadHandler(gijoe_sound_read);
	ZetClose();

	EEPROMInit(&gijoe_eeprom_interface);

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x200000, gijoe_tile_callback);
	K056832SetGlobalOffsets(24, 16);
	K056832SetLinemap();

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x3fffff, gijoe_sprite_callback, 1);
	K053247SetSpriteOffset(-61, -46+10);

	K054539Init(0, 48000, DrvSndROM, 0x200000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 2.10, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 2.10, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	SekExit();
	ZetExit();

	EEPROMExit();

	K054539Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		INT32 r = (BURN_ENDIAN_SWAP_INT16(pal[i]) & 0x1f);
		INT32 g = (BURN_ENDIAN_SWAP_INT16(pal[i]) >> 5) & 0x1f;
		INT32 b = (BURN_ENDIAN_SWAP_INT16(pal[i]) >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		DrvPalette[i] = (r << 16) + (g << 8) + b;
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	INT32 layers[4], vrc_mode, vrc_new;

	K056832ReadAvac(&vrc_mode, &vrc_new);

	if (vrc_mode)
	{
		avac_vrc = vrc_new;
		avac_bits[0] = vrc_new << 4  & 0xf000;
		avac_bits[1] = vrc_new       & 0xf000;
		avac_bits[2] = vrc_new << 8  & 0xf000;
		avac_bits[3] = vrc_new << 12 & 0xf000;
	}
	else
		avac_bits[3] = avac_bits[2] = avac_bits[1] = avac_bits[0] = 0xf000;

	sprite_colorbase = K053251GetPaletteIndex(0);

	for (INT32 i = 0; i < 4; i++) {
		layer_colorbase[i] = K053251GetPaletteIndex(i+1);
	}

	if (K056832ReadRegister(0x14) == 2)
	{
		K056832SetLayerOffsets(0, 2, 0);
		K056832SetLayerOffsets(1, 4, 0);
		K056832SetLayerOffsets(2, 6, 0);
		K056832SetLayerOffsets(3, 8, 0);
	}
	else
	{
		K056832SetLayerOffsets(0, 0, 0);
		K056832SetLayerOffsets(1, 8, 0);
		K056832SetLayerOffsets(2, 14, 0);
		K056832SetLayerOffsets(3, 16, 0);
	}

	KonamiClearBitmaps(DrvPalette[0]);

	layers[0] = 0;
	layerpri[0] = 0; // not sure
	layers[1] = 1;
	layerpri[1] = K053251GetPriority(2);
	layers[2] = 2;
	layerpri[2] = K053251GetPriority(3);
	layers[3] = 3;
	layerpri[3] = K053251GetPriority(4);

	konami_sortlayers4(layers, layerpri);

	// find layer 3's pri, tell renderer to inhibit shadows on that pri
	for (INT32 i = 0; i < 4; i++) {
		if (layers[i] == 3) {
			konami_set_layer_shadow_inhibit_mode(1 << i);
		}
	}

	if (nBurnLayer & 1) K056832Draw(layers[0], K056832_DRAW_FLAG_MIRROR, 1);
	if (nBurnLayer & 2) K056832Draw(layers[1], K056832_DRAW_FLAG_MIRROR, 2);
	if (nBurnLayer & 4) K056832Draw(layers[2], K056832_DRAW_FLAG_MIRROR, 4);
	if (nBurnLayer & 8) K056832Draw(layers[3], K056832_DRAW_FLAG_MIRROR, 8);

	if (nSpriteEnable & 1) K053247SpritesRender();

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 4 * sizeof(INT16));
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		DrvInputs[0] = (DrvInputs[0] & 0x0fff);
		DrvInputs[2] = (DrvInputs[2] & 0x7f7f) | (DrvDips[0] & 0x80) | ((DrvDips[1] & 0x80) << 8);
		DrvInputs[3] = (DrvInputs[3] & 0xff7f) | (DrvDips[2] & 0x80);
		DrvInputs[1] &= 0x0fff;
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 16000000 / 60, 8000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		CPU_RUN(0, Sek);

		if (control_data & 0x20 && irq6_timer == 0) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
			irq6_timer = -1;
		} else if (irq6_timer != -1) irq6_timer--;

		CPU_RUN(1, Zet);
		if ((i % (nInterleave / 8)) == ((nInterleave / 8) - 1) && sound_nmi_enable) {
			ZetNmi();
		}

		if (i == 240) {
			if (K056832IsIrqEnabled()) {
				if (K053246_is_IRQ_enabled()) {
					gijoe_objdma();
					irq6_timer = 1; // guess
				}

				if (control_data & 0x80) {
					SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
				}
			}

			if (pBurnDraw) { // draw at vblank (or linemapping flickers @ elevator level)
				DrvDraw();
			}
		}
	}

	if (pBurnSoundOut) {
		BurnSoundClear();
		K054539Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	SekClose();

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029732;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);
		ZetScan(nAction);

		K054539Scan(nAction, pnMin);

		KonamiICScan(nAction);

		SCAN_VAR(avac_vrc);
		SCAN_VAR(avac_bits);
		SCAN_VAR(avac_occupancy);
		SCAN_VAR(sound_nmi_enable);
		SCAN_VAR(control_data);
		SCAN_VAR(irq6_timer);

		SCAN_VAR(layerpri);
		SCAN_VAR(layer_colorbase);
		SCAN_VAR(sprite_colorbase);
	}

	return 0;
}


// G.I. Joe (World, EAB, set 1)

static struct BurnRomInfo gijoeRomDesc[] = {
	{ "069eab03.14e",	0x040000, 0xdd2d533f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "069eab02.18e",	0x040000, 0x6bb11c87, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "069a12.13e",		0x040000, 0x75a7585c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "069a11.16e",		0x040000, 0x3153e788, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "069a01.7c",		0x010000, 0x74172b99, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "069a10.18j",		0x100000, 0x4c6743ee, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "069a09.16j",		0x100000, 0xe6e36b05, 3 | BRF_GRA },           //  6

	{ "069a08.6h",		0x100000, 0x325477d4, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "069a05.1h",		0x100000, 0xc4ab07ed, 4 | BRF_GRA },           //  8
	{ "069a07.4h",		0x100000, 0xccaa3971, 4 | BRF_GRA },           //  9
	{ "069a06.2h",		0x100000, 0x63eba8e1, 4 | BRF_GRA },           // 10

	{ "069a04.1e",		0x200000, 0x11d6dcd6, 5 | BRF_SND },           // 11 k054539

	{ "er5911.7d",		0x000080, 0xa0d50a79, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(gijoe)
STD_ROM_FN(gijoe)

struct BurnDriver BurnDrvGijoe = {
	"gijoe", NULL, NULL, NULL, "1992",
	"G.I. Joe (World, EAB, set 1)\0", NULL, "Konami", "GX069",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gijoeRomInfo, gijoeRomName, NULL, NULL, NULL, NULL, GijoeInputInfo, GijoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// G.I. Joe (World, EB8, prototype?)

static struct BurnRomInfo gijoeeaRomDesc[] = {
	{ "rom3.14e",		0x040000, 0x0a11f63a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "rom2.18e",		0x040000, 0x8313c559, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "069a12.13e",		0x040000, 0x75a7585c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "069a11.16e",		0x040000, 0x3153e788, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "069a01.7c",		0x010000, 0x74172b99, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "069a10.18j",		0x100000, 0x4c6743ee, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "069a09.16j",		0x100000, 0xe6e36b05, 3 | BRF_GRA },           //  6

	{ "069a08.6h",		0x100000, 0x325477d4, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "069a05.1h",		0x100000, 0xc4ab07ed, 4 | BRF_GRA },           //  8
	{ "069a07.4h",		0x100000, 0xccaa3971, 4 | BRF_GRA },           //  9
	{ "069a06.2h",		0x100000, 0x63eba8e1, 4 | BRF_GRA },           // 10

	{ "069a04.1e",		0x200000, 0x11d6dcd6, 5 | BRF_SND },           // 11 k054539

	{ "er5911.7d",		0x000080, 0x64f5c87b, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(gijoeea)
STD_ROM_FN(gijoeea)

struct BurnDriver BurnDrvGijoeea = {
	"gijoeea", "gijoe", NULL, NULL, "1992",
	"G.I. Joe (World, EB8, prototype?)\0", NULL, "Konami", "GX069",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gijoeeaRomInfo, gijoeeaRomName, NULL, NULL, NULL, NULL, GijoeInputInfo, GijoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// G.I. Joe (US, UAB)

static struct BurnRomInfo gijoeuRomDesc[] = {
	{ "069uab03.14e",	0x040000, 0x25ff77d2, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "069uab02.18e",	0x040000, 0x31cced1c, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "069a12.13e",		0x040000, 0x75a7585c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "069a11.16e",		0x040000, 0x3153e788, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "069a01.7c",		0x010000, 0x74172b99, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "069a10.18j",		0x100000, 0x4c6743ee, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "069a09.16j",		0x100000, 0xe6e36b05, 3 | BRF_GRA },           //  6

	{ "069a08.6h",		0x100000, 0x325477d4, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "069a05.1h",		0x100000, 0xc4ab07ed, 4 | BRF_GRA },           //  8
	{ "069a07.4h",		0x100000, 0xccaa3971, 4 | BRF_GRA },           //  9
	{ "069a06.2h",		0x100000, 0x63eba8e1, 4 | BRF_GRA },           // 10

	{ "069a04.1e",		0x200000, 0x11d6dcd6, 5 | BRF_SND },           // 11 k054539

	{ "er5911.7d",		0x000080, 0xca966023, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(gijoeu)
STD_ROM_FN(gijoeu)

struct BurnDriver BurnDrvGijoeu = {
	"gijoeu", "gijoe", NULL, NULL, "1992",
	"G.I. Joe (US, UAB)\0", NULL, "Konami", "GX069",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gijoeuRomInfo, gijoeuRomName, NULL, NULL, NULL, NULL, GijoeInputInfo, GijoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// G.I. Joe (US, UAA)

static struct BurnRomInfo gijoeuaRomDesc[] = {
	{ "069uaa03.14e",	0x040000, 0xcfb1af44, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "069uaa02.18e",	0x040000, 0x3e6a56cd, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "069a12.13e",		0x040000, 0x75a7585c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "069a11.16e",		0x040000, 0x3153e788, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "069a01.7c",		0x010000, 0x74172b99, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

// the GFX ROMs are smaller and believed to have the same content as the other sets, in fact the game runs fine. Still better to have them dumped
// only 4 GFX ROMs were dumped from this PCB because the others are soldered ant the dumper didn't want to risk damage
//	{ "069a10al.d7",	0x040000, 0x00000000, 3 | BRF_GRA | BRF_NODUMP }, //  5 K056832 Characters
//	{ "069a10ah.d9",	0x040000, 0x00000000, 3 | BRF_GRA | BRF_NODUMP }, //  6
//	{ "069a09al.d3",	0x040000, 0x00000000, 3 | BRF_GRA | BRF_NODUMP }, //  7
//	{ "069a09ah.d5",	0x040000, 0x00000000, 3 | BRF_GRA | BRF_NODUMP }, //  8
	{ "069a10bl.j7",	0x040000, 0x087d8e25, 3 | BRF_GRA | BRF_OPT }, 			  //  5
	{ "069a10bh.j9",	0x040000, 0xfc7ad198, 3 | BRF_GRA | BRF_OPT }, 			  //  6
	{ "069a09bl.j3",	0x040000, 0x385217cc, 3 | BRF_GRA | BRF_OPT }, 			  //  7
	{ "069a09bh.j5",	0x040000, 0xc6d43c8a, 3 | BRF_GRA | BRF_OPT }, 			  //  8
// 	overlay standard ROMs for now
	{ "069a10.18j",		0x100000, 0x4c6743ee, 3 | BRF_GRA },           //  9 K056832 Characters
	{ "069a09.16j",		0x100000, 0xe6e36b05, 3 | BRF_GRA },           // 10

//	{ "069a08al.k3",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 13 K053246 Sprites
//	{ "069a08ah.n3",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 14
//	{ "069a05al.k5",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 15
//	{ "069a05ah.n5",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 16
//	{ "069a07al.k7",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 15
//	{ "069a07ah.n7",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 16
//	{ "069a06al.k9",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 17
//	{ "069a06ah.n9",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 18
//	{ "069a08bl.l3",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 19
//	{ "069a08bh.p3",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 20
//	{ "069a05bl.l5",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 21
//	{ "069a05bh.p5",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 22
//	{ "069a07bl.l7",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 23
//	{ "069a07bh.p7",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 24
//	{ "069a06bl.l9",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 25
//	{ "069a06bh.p9",	0x040000, 0x00000000, 4 | BRF_GRA | BRF_NODUMP }, // 26
// 	overlay standard ROMs for now
	{ "069a08.6h",		0x100000, 0x325477d4, 4 | BRF_GRA },           // 11 K053247 Sprites
	{ "069a05.1h",		0x100000, 0xc4ab07ed, 4 | BRF_GRA },           // 12
	{ "069a07.4h",		0x100000, 0xccaa3971, 4 | BRF_GRA },           // 13
	{ "069a06.2h",		0x100000, 0x63eba8e1, 4 | BRF_GRA },           // 14
	
//	{ "069a04a.g2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 27 k054539
//	{ "069a04b.j2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 28
//	{ "069a04c.k2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 29
//	{ "069a04d.l2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 30
//	{ "069a04e.n2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 31
//	{ "069a04f.p2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 32
//	{ "069a04g.r2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 33
//	{ "069a04h.s2",		0x040000, 0x00000000, 5 | BRF_SND | BRF_NODUMP }, // 34
// 	overlay standard ROMs for now
	{ "069a04a.1e",		0x200000, 0x11d6dcd6, 5 | BRF_SND },           // 11 k054539

	{ "er5911.7d",		0x000080, 0x33b07813, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(gijoeua)
STD_ROM_FN(gijoeua)

struct BurnDriver BurnDrvGijoeua = {
	"gijoeua", "gijoe", NULL, NULL, "1992",
	"G.I. Joe (US, UAA)\0", NULL, "Konami", "GX069",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gijoeuaRomInfo, gijoeuaRomName, NULL, NULL, NULL, NULL, GijoeInputInfo, GijoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// G.I. Joe (Japan, JAA)

static struct BurnRomInfo gijoejRomDesc[] = {
	{ "069jaa03.14e",	0x040000, 0x4b398901, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "069jaa02.18e",	0x040000, 0x8bb22392, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "069a12.13e",		0x040000, 0x75a7585c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "069a11.16e",		0x040000, 0x3153e788, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "069a01.7c",		0x010000, 0x74172b99, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "069a10.18j",		0x100000, 0x4c6743ee, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "069a09.16j",		0x100000, 0xe6e36b05, 3 | BRF_GRA },           //  6

	{ "069a08.6h",		0x100000, 0x325477d4, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "069a05.1h",		0x100000, 0xc4ab07ed, 4 | BRF_GRA },           //  8
	{ "069a07.4h",		0x100000, 0xccaa3971, 4 | BRF_GRA },           //  9
	{ "069a06.2h",		0x100000, 0x63eba8e1, 4 | BRF_GRA },           // 10

	{ "069a04.1e",		0x200000, 0x11d6dcd6, 5 | BRF_SND },           // 11 k054539

	{ "er5911.7d",		0x000080, 0xc914fcf2, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(gijoej)
STD_ROM_FN(gijoej)

struct BurnDriver BurnDrvGijoej = {
	"gijoej", "gijoe", NULL, NULL, "1992",
	"G.I. Joe (Japan, JAA)\0", NULL, "Konami", "GX069",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gijoejRomInfo, gijoejRomName, NULL, NULL, NULL, NULL, GijoeInputInfo, GijoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// G.I. Joe (Asia, AA)

static struct BurnRomInfo gijoeaRomDesc[] = {
	{ "069aa03.14e",	0x040000, 0x74355c6e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "069aa02.18e",	0x040000, 0xd3dd0397, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "069a12.13e",		0x040000, 0x75a7585c, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "069a11.16e",		0x040000, 0x3153e788, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "069a01.7c",		0x010000, 0x74172b99, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "069a10.18j",		0x100000, 0x4c6743ee, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "069a09.16j",		0x100000, 0xe6e36b05, 3 | BRF_GRA },           //  6

	{ "069a08.6h",		0x100000, 0x325477d4, 4 | BRF_GRA },           //  7 K053247 Sprites
	{ "069a05.1h",		0x100000, 0xc4ab07ed, 4 | BRF_GRA },           //  8
	{ "069a07.4h",		0x100000, 0xccaa3971, 4 | BRF_GRA },           //  9
	{ "069a06.2h",		0x100000, 0x63eba8e1, 4 | BRF_GRA },           // 10

	{ "069a04.1e",		0x200000, 0x11d6dcd6, 5 | BRF_SND },           // 11 k054539

	{ "er5911.7d",		0x000080, 0x6363513c, 6 | BRF_OPT },           // 12 eeprom data
};

STD_ROM_PICK(gijoea)
STD_ROM_FN(gijoea)

struct BurnDriver BurnDrvGijoea = {
	"gijoea", "gijoe", NULL, NULL, "1992",
	"G.I. Joe (Asia, AA)\0", NULL, "Konami", "GX069",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_PREFIX_KONAMI, GBF_RUNGUN, 0,
	NULL, gijoeaRomInfo, gijoeaRomName, NULL, NULL, NULL, NULL, GijoeInputInfo, GijoeDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};
