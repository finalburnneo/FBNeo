// FB Alpha Xexex driver module
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
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvGfxROMExp2;
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
static UINT8 *soundlatch3;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 layerpri[4];
static INT32 layer_colorbase[4];
static INT32 sprite_colorbase;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvReset;
static UINT16 DrvInputs[4];
static UINT8 DrvDips[2];

static INT32 z80_bank;
static INT32 sound_nmi_enable;
static INT32 irq5_timer;
static UINT16 control_data;
static INT32 enable_alpha;

static struct BurnInputInfo XexexInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy3 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"service"},
	{"Service 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"service"},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy4 + 3,	"diagnostics"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
};

STDINPUTINFO(Xexex)

static struct BurnDIPInfo XexexDIPList[]=
{
	{0x14, 0xff, 0xff, 0x00, NULL		},

	{0   , 0xfe, 0   ,    2, "PCM Volume Boost"	},
	{0x14, 0x01, 0x08, 0x00, "Off"		},
	{0x14, 0x01, 0x08, 0x08, "On"		},
};

STDDIPINFO(Xexex)

static void xexex_objdma()
{
	UINT16 *dst = (UINT16*)K053247Ram;
	UINT16 *src = (UINT16*)DrvSprRAM;
	INT32 num_inactive = 256;
	INT32 counter = 256;

	do
	{
		if (*src & 0x8000)
		{
			dst[0] = src[0x0];  dst[1] = src[0x2];
			dst[2] = src[0x4];  dst[3] = src[0x6];
			dst[4] = src[0x8];  dst[5] = src[0xa];
			dst[6] = src[0xc];  dst[7] = src[0xe];
			dst += 8;
			num_inactive--;
		}
		src += 0x40;
	}
	while (--counter);

	if (num_inactive) do { *dst = 0; dst += 8; } while (--num_inactive);
}

static void update_de000()
{
	K053246_set_OBJCHA_line((control_data & 0x100) >> 8);
	EEPROMWrite((control_data & 0x04), (control_data & 0x02), (control_data & 0x01));
	enable_alpha = ~control_data & 0x200;

}

static void _fastcall xexex_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffc0) == 0x0c0000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0c2000) {
		K053246Write((address & 0x06) + 0, data >> 8);
		K053246Write((address & 0x06) + 1, data&0xff);
		return;
	}

	if ((address & 0xfffff0) == 0x0c8000) {
		K053250RegWrite(0, address, data);
		return;	
	}

	if ((address & 0xffffe0) == 0x0ca000) {
		K054338WriteWord(address, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0d8000) {
		return;	// regsb
	}

	if ((address & 0xffc000) == 0x180000) {
		K056832RamWriteWord(address & 0x1fff, data);
		return;
	}

	switch (address)
	{
		case 0x0de000:
			control_data = data;
			update_de000();
		return;
	}
}

static void _fastcall xexex_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffc0) == 0x0c0000) {
		K056832ByteWrite(address & 0x3f, data);
		return;
	}

	if ((address & 0xfffff8) == 0x0c2000) {
		K053246Write((address & 0x07) ^ 0, data);
		return;
	}

	if ((address & 0xfffff0) == 0x0c8000) {
		K053250RegWrite(0, address, data);
		return;	
	}

	if ((address & 0xffffe0) == 0x0ca000) {
		K054338WriteByte(address, data);
		return;
	}

	if ((address & 0xffffe1) == 0x0cc001) {
		K053251Write((address / 2) & 0xf, data);
		return;
	}

	if ((address & 0xffffe0) == 0x0d0000) {
		// k053252
		return;
	}

	if ((address & 0xfffff8) == 0x0d8000) {
		return;	// regsb
	}

	if ((address & 0xffc000) == 0x180000) {
		K056832RamWriteByte(address & 0x1fff, data);
		return;
	}

	switch (address)
	{
		case 0x0d4000:
		case 0x0d4001:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x0d6001:
		case 0x0d6003:
		case 0x0d6005:
		case 0x0d6007:
		case 0x0d6009:
		return;	//	nop

		case 0x0d600c:
		case 0x0d600d:
			*soundlatch = data;
		return;

		case 0x0d600e:
		case 0x0d600f:
			*soundlatch2 = data;
		return;

		case 0x0de000:
			control_data = (control_data & 0x00ff) | (data << 8);
			update_de000();
		return;

		case 0x0de001:
			control_data = (control_data & 0xff00) | (data << 0);
			update_de000();
		return;

		case 0x170000:
		return;		// watchdog
	}
}

static UINT16 _fastcall xexex_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x0c8000) {
		return K053250RegRead(0, address);
	}

	if ((address & 0xffc000) == 0x180000) {
		return K056832RamReadWord(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x190000) {
		return K056832RomWordRead(address);
	}

	if ((address & 0xffe000) == 0x1a0000) {
		return K053250RomRead(0, address);
	}

	switch (address)
	{
		case 0x0c4000:
			return K053246Read(1) + (K053246Read(0) << 8);

		case 0x0da000:
			return DrvInputs[1];

		case 0x0da002:
			return DrvInputs[2];

		case 0x0dc000:
			return DrvInputs[0];

		case 0x0dc002:
			return (DrvInputs[3] & 0x8) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x0de000:
			return control_data;
	}

	return 0;
}

static UINT8 _fastcall xexex_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x0c8000) {
		return K053250RegRead(0, address);	
	}

	if ((address & 0xffc000) == 0x180000) {
		return K056832RamReadByte(address & 0x1fff);
	}

	if ((address & 0xffe000) == 0x190000) {
		return K056832RomWordRead(address) >> ((~address & 1) * 8);
	}

	if ((address & 0xffe000) == 0x1a0000) {
		return K053250RomRead(0, address) >> ((~address & 1) * 8);
	}

	switch (address)
	{
		case 0x0c4000:
		case 0x0c4001:
			return K053246Read((address & 1)); // ^ 1? ??

		case 0x0d6011:
			return 0; // nop

		case 0x0d6015:
			return *soundlatch3;

		case 0x0da000:
			return DrvInputs[1] >> 8;

		case 0x0da001:
			return DrvInputs[1];

		case 0x0da002:
			return DrvInputs[2] >> 8;

		case 0x0da003:
			return DrvInputs[2];

		case 0x0dc000:
			return DrvInputs[0] >> 8;

		case 0x0dc001:
			return DrvInputs[0];

		case 0x0dc002:
			return 0;

		case 0x0dc003:
			return (DrvInputs[3] & 0x8) | 2 | (EEPROMRead() ? 0x01 : 0);
	}

	return 0;
}

static void bankswitch(INT32 data)
{
	z80_bank = data;
	ZetMapMemory(DrvZ80ROM + ((data & 0x07) * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall xexex_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xec00:
			BurnYM2151SelectRegister(data);
		return;

		case 0xec01:
			BurnYM2151WriteRegister(data);
		return;

		case 0xf000:
			*soundlatch3 = data;
		return;

		case 0xf800:
			bankswitch(data);
		return;
	}

	if (address >= 0xe000 && address <= 0xe22f) {
		return K054539Write(0, address & 0x3ff, data);
	}
}

static UINT8 __fastcall xexex_sound_read(UINT16 address)
{
	if (address >= 0xe000 && address <= 0xe22f) {
		return K054539Read(0, address & 0x3ff);
	}

	switch (address)
	{
		case 0xec00:
		case 0xec01:
			return BurnYM2151ReadStatus();

		case 0xf002:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xf003:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch2;
	}

	return 0;
}

static void xexex_sprite_callback(INT32 */*code*/, INT32 *color, INT32 *priority)
{
	INT32 pri = (*color & 0x3e0) >> 4;

	if (pri <= layerpri[3])					*priority = 0x0000;
	else if (pri > layerpri[3] && pri <= layerpri[2])	*priority = 0xff00;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority = 0xfff0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority = 0xfffc;
	else							*priority = 0xfffe;

	*color = sprite_colorbase | (*color & 0x001f);
}

static void xexex_tile_callback(INT32 layer, INT32 */*code*/, INT32 *color, INT32 */*flags*/)
{
	*color = layer_colorbase[layer] | (*color >> 2 & 0x0f);
}

static const eeprom_interface xexex_eeprom_interface =
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
	bankswitch(2);
	ZetClose();

	KonamiICReset();

	BurnYM2151Reset();
	K054539Reset(0);

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEeprom, 0, 128);
	}

	control_data = 0;
	irq5_timer = 0;

	for (INT32 i = 0; i < 4; i++)
	{
		layer_colorbase[i] = 0;
		layerpri[i] = 0;
	}

	sound_nmi_enable = 0;
	z80_bank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROMExp0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROMExp1		= Next; Next += 0x800000;
	DrvGfxROM2		= Next; Next += 0x080000;
	DrvGfxROMExp2		= Next; Next += 0x100000;

	DrvSndROM		= Next; Next += 0x400000;

	DrvEeprom		= Next; Next += 0x000080;

	konami_palette32	= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM		= Next; Next += 0x010000;
	DrvSprRAM		= Next; Next += 0x008000;
	DrvPalRAM		= Next; Next += 0x002000;

	DrvZ80RAM		= Next; Next += 0x002000;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;
	soundlatch3		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void XexexApanCallback(double one, double two)
{
	//bprintf(0, _T("apan %f, %f. "), one, two); - wip
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(54.25);
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

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000004,  9, 8, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000006, 10, 8, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, 11, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, 12, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x200000, 13, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 14, 1)) return 1;

		K053247GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x200000);
		K053247GfxDecode(DrvGfxROM1, DrvGfxROMExp1, 0x400000);
	}

	K054338Init();

	K053250Init(0, DrvGfxROM2, DrvGfxROMExp2, 0x80000);
	K053250SetOffsets(0, -45, -16);

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x200000, xexex_tile_callback);
	K056832SetGlobalOffsets(40, 0);
	K056832SetLayerOffsets(0, -2, 16);
	K056832SetLayerOffsets(1,  2, 16);
	K056832SetLayerOffsets(2,  4, 16);
	K056832SetLayerOffsets(3,  6, 16);

	K053247Init(DrvGfxROM1, DrvGfxROMExp1, 0x3fffff, xexex_sprite_callback, 1);
	K053247SetSpriteOffset(-88, -32);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x080000, 0x08ffff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x090000, 0x097fff, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x098000, 0x09ffff, MAP_RAM);
	SekMapMemory((UINT8*)K053250Ram,	0x0c6000, 0x0c7fff, MAP_RAM);
	SekMapMemory(Drv68KROM + 0x080000,	0x100000, 0x17ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x1b0000, 0x1b1fff, MAP_RAM);
	SekSetWriteWordHandler(0,		xexex_main_write_word);
	SekSetWriteByteHandler(0,		xexex_main_write_byte);
	SekSetReadWordHandler(0,		xexex_main_read_word);
	SekSetReadByteHandler(0,		xexex_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xc000, 0xdfff, MAP_RAM);
	ZetSetWriteHandler(xexex_sound_write);
	ZetSetReadHandler(xexex_sound_read);
	ZetClose();

	EEPROMInit(&xexex_eeprom_interface);

	if (nBurnSoundRate == 44100) {
		BurnYM2151Init(3700000); // 3.7mhz here to match the tuning of the 48000khz k054539 chip, otherwise the music sounds horrible! - dink Nov.7.2014
	} else {
		BurnYM2151Init(4000000);
	}
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);

	K054539Init(0, 48000, DrvSndROM, 0x300000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, (DrvDips[0] & 0x08) ? 1.40 : 1.10, BURN_SND_ROUTE_BOTH);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, (DrvDips[0] & 0x08) ? 1.40 : 1.10, BURN_SND_ROUTE_BOTH);
	K054539SetApanCallback(0, XexexApanCallback);

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

	BurnYM2151Exit();
	K054539Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x2000/2; i+=2) {
		DrvPalette[i/2] = ((pal[i+0] & 0xff) << 16) + pal[i+1];
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	INT32 layer[4], bg_colorbase, plane, alpha;

	sprite_colorbase = K053251GetPaletteIndex(0);

	bg_colorbase = K053251GetPaletteIndex(1);

	layer_colorbase[0] = 0x70;

	for (plane = 1; plane < 4; plane++)
	{
		layer_colorbase[plane] = K053251GetPaletteIndex(plane+1);
	}

	layer[0] = 1;
	layerpri[0] = K053251GetPriority(2);
	layer[1] = 2;
	layerpri[1] = K053251GetPriority(3);
	layer[2] = 3;
	layerpri[2] = K053251GetPriority(4);
	layer[3] = -1;
	layerpri[3] = K053251GetPriority(1);

	konami_sortlayers4(layer, layerpri);

	KonamiClearBitmaps(0);

	for (plane = 0; plane < 4; plane++)
	{
		if (layer[plane] < 0)
		{
			if (nSpriteEnable & 2) K053250Draw(0, bg_colorbase, 0, 1 << plane);
		}
		else if (!enable_alpha || layer[plane] != 1)
		{
			if (nBurnLayer & (1 << layer[plane])) K056832Draw(layer[plane], 0, 1 << plane);
		}
	}

	if (nSpriteEnable & 1) K053247SpritesRender();

	if (enable_alpha)
	{
		alpha = K054338_set_alpha_level(1);

		// this kludge fixes: flashy transitions in intro, cutscene after stage 3
		// is missing several effects, continue screen is missing "zooming!" numbers.
		if (alpha < 0x10)
			alpha = 0x10;

		if (alpha > 0)
		{
			if (nBurnLayer & 8) K056832Draw(1, K056832_SET_ALPHA(255-alpha), 0);
		}
	}

	if (nBurnLayer & 8) K056832Draw(0, 0, 0);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0xff, 3 * sizeof(INT16));
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		DrvInputs[3] = (DrvJoy4[3]) ? 0x00 : 0x08; // Service Mode
	}

	INT32 nInterleave = 120;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { (16000000 * 100) / 5425, (8000000 * 100) / 5425 };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext, nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesDone[0] += SekRun(nCyclesSegment);

		if (i == 0 && control_data & 0x20) {
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}

		if (i != 0 && i != ((nInterleave/2)-1)) {
			if (irq5_timer > 0) {
				irq5_timer--;
				if (irq5_timer == 0 && control_data & 0x40) {
					SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
				}
			}
		}

		if (i == ((nInterleave/2)-1)) {
			if (K053246_is_IRQ_enabled()) {
				xexex_objdma();
				irq5_timer = 5; // lots of testing led to this number.
			}

			if (control_data & 0x0800)
				SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);
		}

		nNext = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[1];
		nCyclesDone[1] += ZetRun(nCyclesSegment);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K054539Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}
	
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K054539Update(0, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

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

		BurnYM2151Scan(nAction);
		K054539Scan(nAction);

		KonamiICScan(nAction);

		SCAN_VAR(z80_bank);
		SCAN_VAR(sound_nmi_enable);

		SCAN_VAR(irq5_timer);

		SCAN_VAR(control_data);
		SCAN_VAR(enable_alpha);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch(z80_bank);
		ZetClose();
	}

	EEPROMScan(nAction, pnMin);

	return 0;
}



// Xexex (ver EAA)

static struct BurnRomInfo xexexRomDesc[] = {
	{ "067eaa01.16d",	0x040000, 0x3ebcb066, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "067eaa02.16f",	0x040000, 0x36ea7a48, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "067b03.13d",		0x040000, 0x97833086, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "067b04.13f",		0x040000, 0x26ec5dc8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "067eaa05.4e",	0x020000, 0x0e33d6ec, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "067b14.1n",		0x100000, 0x02a44bfa, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "067b13.2n",		0x100000, 0x633c8eb5, 3 | BRF_GRA },           //  6

	{ "067b12.17n",		0x100000, 0x08d611b0, 4 | BRF_GRA },           //  7 K056832 Sprites
	{ "067b11.19n",		0x100000, 0xa26f7507, 4 | BRF_GRA },           //  8
	{ "067b10.20n",		0x100000, 0xee31db8d, 4 | BRF_GRA },           //  9
	{ "067b09.22n",		0x100000, 0x88f072ef, 4 | BRF_GRA },           // 10

	{ "067b08.22f",		0x080000, 0xca816b7b, 5 | BRF_GRA },           // 11 K053250 Tiles

	{ "067b06.3e",		0x200000, 0x3b12fce4, 6 | BRF_SND },           // 12 K054539 Samples
	{ "067b07.1e",		0x100000, 0xec87fe1b, 6 | BRF_SND },           // 13

	{ "er5911.19b",		0x000080, 0x155624cc, 7 | BRF_PRG | BRF_ESS }, // 14 eeprom data
};

STD_ROM_PICK(xexex)
STD_ROM_FN(xexex)

struct BurnDriver BurnDrvXexex = {
	"xexex", NULL, NULL, NULL, "1991",
	"Xexex (ver EAA)\0", NULL, "Konami", "GX067",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, xexexRomInfo, xexexRomName, NULL, NULL, XexexInputInfo, XexexDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 256, 4, 3
};


// Orius (ver UAA)

static struct BurnRomInfo oriusRomDesc[] = {
	{ "067uaa01.16d",	0x040000, 0xf1263d3e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "067uaa02.16f",	0x040000, 0x77709f64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "067b03.13d",		0x040000, 0x97833086, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "067b04.13f",		0x040000, 0x26ec5dc8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "067uaa05.4e",	0x020000, 0x0e33d6ec, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "067b14.1n",		0x100000, 0x02a44bfa, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "067b13.2n",		0x100000, 0x633c8eb5, 3 | BRF_GRA },           //  6

	{ "067b12.17n",		0x100000, 0x08d611b0, 4 | BRF_GRA },           //  7 K056832 Sprites
	{ "067b11.19n",		0x100000, 0xa26f7507, 4 | BRF_GRA },           //  8
	{ "067b10.20n",		0x100000, 0xee31db8d, 4 | BRF_GRA },           //  9
	{ "067b09.22n",		0x100000, 0x88f072ef, 4 | BRF_GRA },           // 10

	{ "067b08.22f",		0x080000, 0xca816b7b, 5 | BRF_GRA },           // 11 K053250 Tiles

	{ "067b06.3e",		0x200000, 0x3b12fce4, 6 | BRF_SND },           // 12 K054539 Samples
	{ "067b07.1e",		0x100000, 0xec87fe1b, 6 | BRF_SND },           // 13

	{ "er5911.19b",		0x000080, 0x547ee4e4, 7 | BRF_PRG | BRF_ESS }, // 14 eeprom data
};

STD_ROM_PICK(orius)
STD_ROM_FN(orius)

struct BurnDriver BurnDrvOrius = {
	"orius", "xexex", NULL, NULL, "1991",
	"Orius (ver UAA)\0", NULL, "Konami", "GX067",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, oriusRomInfo, oriusRomName, NULL, NULL, XexexInputInfo, XexexDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 256, 4, 3
};


// Xexex (ver AAA)

static struct BurnRomInfo xexexaRomDesc[] = {
	{ "067aaa01.16d",	0x040000, 0xcf557144, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "067aaa02.16f",	0x040000, 0xb7b98d52, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "067b03.13d",		0x040000, 0x97833086, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "067b04.13f",		0x040000, 0x26ec5dc8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "067eaa05.4e",	0x020000, 0x0e33d6ec, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "067b14.1n",		0x100000, 0x02a44bfa, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "067b13.2n",		0x100000, 0x633c8eb5, 3 | BRF_GRA },           //  6

	{ "067b12.17n",		0x100000, 0x08d611b0, 4 | BRF_GRA },           //  7 K056832 Sprites
	{ "067b11.19n",		0x100000, 0xa26f7507, 4 | BRF_GRA },           //  8
	{ "067b10.20n",		0x100000, 0xee31db8d, 4 | BRF_GRA },           //  9
	{ "067b09.22n",		0x100000, 0x88f072ef, 4 | BRF_GRA },           // 10

	{ "067b08.22f",		0x080000, 0xca816b7b, 5 | BRF_GRA },           // 11 K053250 Tiles

	{ "067b06.3e",		0x200000, 0x3b12fce4, 6 | BRF_SND },           // 12 K054539 Samples
	{ "067b07.1e",		0x100000, 0xec87fe1b, 6 | BRF_SND },           // 13

	{ "er5911.19b",		0x000080, 0x051c14c6, 7 | BRF_PRG | BRF_ESS }, // 14 eeprom data
};

STD_ROM_PICK(xexexa)
STD_ROM_FN(xexexa)

struct BurnDriver BurnDrvXexexa = {
	"xexexa", "xexex", NULL, NULL, "1991",
	"Xexex (ver AAA)\0", NULL, "Konami", "GX067",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, xexexaRomInfo, xexexaRomName, NULL, NULL, XexexInputInfo, XexexDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 256, 4, 3
};


// Xexex (ver JAA)

static struct BurnRomInfo xexexjRomDesc[] = {
	{ "067jaa01.16d",	0x040000, 0x06e99784, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "067jaa02.16f",	0x040000, 0x30ae5bc4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "067b03.13d",		0x040000, 0x97833086, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "067b04.13f",		0x040000, 0x26ec5dc8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "067jaa05.4e",	0x020000, 0x2f4dd0a8, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "067b14.1n",		0x100000, 0x02a44bfa, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "067b13.2n",		0x100000, 0x633c8eb5, 3 | BRF_GRA },           //  6

	{ "067b12.17n",		0x100000, 0x08d611b0, 4 | BRF_GRA },           //  7 K056832 Sprites
	{ "067b11.19n",		0x100000, 0xa26f7507, 4 | BRF_GRA },           //  8
	{ "067b10.20n",		0x100000, 0xee31db8d, 4 | BRF_GRA },           //  9
	{ "067b09.22n",		0x100000, 0x88f072ef, 4 | BRF_GRA },           // 10

	{ "067b08.22f",		0x080000, 0xca816b7b, 5 | BRF_GRA },           // 11 K053250 Tiles

	{ "067b06.3e",		0x200000, 0x3b12fce4, 6 | BRF_SND },           // 12 K054539 Samples
	{ "067b07.1e",		0x100000, 0xec87fe1b, 6 | BRF_SND },           // 13

	{ "er5911.19b",		0x000080, 0x79a79c7b, 7 | BRF_PRG | BRF_ESS }, // 14 eeprom data
};

STD_ROM_PICK(xexexj)
STD_ROM_FN(xexexj)

struct BurnDriver BurnDrvXexexj = {
	"xexexj", "xexex", NULL, NULL, "1991",
	"Xexex (ver JAA)\0", NULL, "Konami", "GX067",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_HORSHOOT, 0,
	NULL, xexexjRomInfo, xexexjRomName, NULL, NULL, XexexInputInfo, XexexDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	384, 256, 4, 3
};
