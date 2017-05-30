// FB Alpha Lethal Enforcers driver module
// Based on MAME driver by R. Belmont and Nicola Salmoria
// Notes:
//   japan version needs sprites fixed (x flipped not y flipped)
//

#include "tiles_generic.h"
#include "hd6309_intf.h"
#include "z80_intf.h"
#include "konamiic.h"
#include "burn_ym2151.h"
#include "k054539.h"
#include "eeprom.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *DrvMainROM;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROMExp0;
static UINT8 *DrvGfxROMExp1;
static UINT8 *DrvSndROM;
static UINT8 *DrvEeprom;
static UINT8 *AllRam;
static UINT8 *DrvMainRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT8 *soundlatch;
static UINT8 *soundlatch2;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 current_4800_bank = 0;
static INT32 layer_colorbase[4];
static INT32 sprite_colorbase = 0;
static INT32 sound_nmi_enable = 0;
static INT32 screen_flip = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvReset;
static UINT8 DrvInputs[1];
static UINT8 DrvDips[1];

static INT32 LethalGun0 = 0;
static INT32 LethalGun1 = 0;
static INT32 LethalGun2 = 0;
static INT32 LethalGun3 = 0;
static UINT8 ReloadGun0 = 0;
static UINT8 ReloadGun1 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo LethalenInputList[] = {
	{"P1 Coin",		    BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"mouse button 1" },
	{"P1 Button 2",		BIT_DIGITAL,   &ReloadGun0 ,	"mouse button 2" },
	A("P1 Gun X",    BIT_ANALOG_REL, &LethalGun0   ,    "mouse x-axis" ),
	A("P1 Gun Y",    BIT_ANALOG_REL, &LethalGun1   ,    "mouse y-axis" ),

	{"P2 Coin",		    BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,   &ReloadGun1 ,	"p2 fire 2"	},
	A("P2 Gun X",    BIT_ANALOG_REL, &LethalGun2   ,    "p2 x-axis" ),
	A("P2 Gun Y",    BIT_ANALOG_REL, &LethalGun3   ,    "p2 y-axis" ),

	{"Reset",		    BIT_DIGITAL,	&DrvReset  ,	"reset"		},
	{"Service",		    BIT_DIGITAL,	DrvJoy1 + 2,	"service"	},
	{"Dips",		  BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Lethalen)

static struct BurnDIPInfo LethalenDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xd8, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0c, 0x01, 0x08, 0x08, "Off"			},
	{0x0c, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x0c, 0x01, 0x10, 0x10, "English"		},
	{0x0c, 0x01, 0x10, 0x00, "Spanish"		},

	{0   , 0xfe, 0   ,    2, "Game Type"		},
	{0x0c, 0x01, 0x20, 0x20, "Street"		},
	{0x0c, 0x01, 0x20, 0x00, "Arcade"		},

	{0   , 0xfe, 0   ,    2, "Coin Mechanism"	},
	{0x0c, 0x01, 0x40, 0x40, "Common"		},
	{0x0c, 0x01, 0x40, 0x00, "Independent"		},

	{0   , 0xfe, 0   ,    2, "Sound Output"		},
	{0x0c, 0x01, 0x80, 0x00, "Mono"			},
	{0x0c, 0x01, 0x80, 0x80, "Stereo"		},
};

STDDIPINFO(Lethalen)

#define GUNX(a) (( ( BurnGunReturnX(a - 1) * 287 ) / 0xff ) + 16)
#define GUNY(a) (( ( BurnGunReturnY(a - 1) * 223 ) / 0xff ) + 10)

static UINT8 guns_r(UINT16 address)
{
	switch (address)
	{
		case 0:
			return (ReloadGun0) ? 16 >> 1: GUNX(1) >> 1;
		case 1:
			if ((GUNY(1)<=0x0b) || (GUNY(1)>=0xe8))
				return 0;
			else
				return (ReloadGun0) ? 0 : (232 - GUNY(1));
		case 2:
			return (ReloadGun1) ? 16 >> 1: GUNX(2) >> 1;
		case 3:
			if ((GUNY(2)<=0x0b) || (GUNY(2)>=0xe8))
				return 0;
			else
				return (ReloadGun1) ? 0 : (232 - GUNY(2));
	}

	return 0;
}

static UINT8 gunsaux_r()
{
	int res = 0;

	if (ReloadGun0) return 0;

	if (GUNX(1) & 1) res |= 0x80;
	if (GUNX(2) & 1) res |= 0x40;

	return res;
}

#undef GUNX
#undef GUNY

static void bankswitch(INT32 bank)
{
	bank = (bank & 0x1f) * 0x2000;

	HD6309MapMemory(DrvMainROM + bank, 0x0000, 0x1fff, MAP_ROM);
}

static void lethal_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffc0) == 0x4000) {
		K056832ByteWrite(address, data);
		return;
	}

	if ((address & 0xfff0) == 0x4040) {
		// K056832_regb
		return;
	}

	switch (address)
	{
		case 0x40c4:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
			current_4800_bank = (data >> 4) & 1;
		return;

		case 0x40c5:
		case 0x40c6:
		case 0x40c7:
		return;

		case 0x40c8:
			layer_colorbase[0] = ((data & 0x7) - 1) * 0x40;
			layer_colorbase[1] = (((data >> 4) & 0x7) - 1) * 0x40;
		break;

		case 0x40c9:
		case 0x40ca:
		case 0x40cb:
		return;

		case 0x40cc:
			layer_colorbase[2] = ((data & 0x7) - 1) * 0x40;
			layer_colorbase[3] = (((data >> 4) & 0x7) - 1) * 0x40;
		return;

		case 0x40cd:
		case 0x40ce:
		case 0x40cf:
		break;

		case 0x40d0:
			sprite_colorbase = ((data & 0x7) - 1) * 0x40;
		return;

		case 0x40dc:
			bankswitch(data);
		return;

		case 0x47fe:
		case 0x47ff:
			DrvPalRAM[0x3800 + (address & 1)] = data;
		return;
	}

	if (address < 0x4800 || address >= 0x8000) return;

	address = (address - 0x4800) + (0x3800 * current_4800_bank);

	if (address >= 0x3800 && address < 0x8000) {
		DrvPalRAM[(address - 0x3800)] = data;
		return;
	}

	if ((address & 0xfff0) == 0x0040) {
		K053244Write(0, (address & 0x0f), data);
		return;
	}

	if ((address & 0xffe0) == 0x0080) {
		K054000Write(address, data);
		return;
	}

	if (address >= 0x0800 && address <= 0x17ff) {
		K053245Write(0, address & 0x7ff, data);
		return;
	}

	if (address >= 0x1800 && address < 0x3800) {
		address -= 0x1800;
		INT32 offset = (((address & 0x1800) ^ 0x1000) >> 11) | ((address & 0x07ff) << 2);
		K056832RamWriteByte(offset^1, data);
		return;
	}

	switch (address)
	{
		case 0x00c6:
			*soundlatch = data;
		return;

		case 0x00c7:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;
	}
}

static UINT8 lethal_main_read(UINT16 address)
{
	switch (address)
	{
		case 0x40d4:
		case 0x40d5:
		case 0x40d6:
		case 0x40d7:
			return guns_r(address-0x40d4);

		case 0x40d8:
			return (DrvDips[0] & 0xfc) | 2 | (EEPROMRead() ? 0x01 : 0);

		case 0x40d9:
			return DrvInputs[0];

		case 0x40db:
		case 0x40dc:
		case 0x40de:
		case 0x40dd:
			return gunsaux_r();
	}

	if (address < 0x4800 || address >= 0x8000) return 0;

	address = (address - 0x4800) + (0x3800 * current_4800_bank);

	if (address >= 0x3800 && address < 0x8000) {
		return DrvPalRAM[(address - 0x3800)];
	}

	if ((address & 0xfff0) == 0x0040) {
		return K053244Read(0, address & 0xf);
	}

	if ((address & 0xffe0) == 0x0080) {
		return K054000Read(address);
	}

	if (address >= 0x0800 && address <= 0x17ff) {
		return K053245Read(0, address & 0x7ff);
	}

	if (address >= 0x1800 && address < 0x3800) {
		address -= 0x1800;

		INT32 offset = (((address & 0x1800) ^ 0x1000) >> 11) | ((address & 0x07ff) << 2);
		return K056832RamReadByte(offset^1);
	}

	switch (address)
	{
		case 0x00ca:
			return 0x0f;
	}

	return 0;
}

static void __fastcall lethal_sound_write(UINT16 address, UINT8 data)
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

static UINT8 __fastcall lethal_sound_read(UINT16 address)
{
	if (address >= 0xf800 && address <= 0xfa2f) {
		return K054539Read(0, address & 0x3ff);
	}

	switch (address)
	{
		case 0xfc02:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return *soundlatch;

		case 0xfc03:
			return 0; // soundlatch2?
	}

	return 0;
}

static const eeprom_interface lethalen_eeprom_interface =
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

static void lethal_sprite_callback(INT32 *code, INT32 *color, INT32 *priority)
{
	INT32 pri = (*color & 0xfff0);
	*color = *color & 0x000f;
	*color += 0x400 / 64;

	if (pri == 0x10)	*priority = 0xf0;
	else if (pri == 0x90)	*priority = 0xf0;
	else if (pri == 0x20)	*priority = 0xfc;
	else if (pri == 0xa0)	*priority = 0xfc;
	else if (pri == 0x40)	*priority = 0;
	else if (pri == 0x00)	*priority = 0;
	else if (pri == 0x30)	*priority = 0xfe;
	else			*priority = 0;

	*code = (*code & 0x3fff);
}

static void lethal_tile_callback(INT32 layer, INT32 *code, INT32 *color, INT32 */*flags*/)
{
	*color = layer_colorbase[layer] + ((*color & 0x3c) << 2);
	*code &= 0xffff;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	HD6309Open(0);
	HD6309Reset();
	HD6309Close();

	ZetOpen(0);
	ZetReset();
	ZetClose();

	KonamiICReset();

	K054539Reset(0);

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEeprom, 0, 128);
	}

	layer_colorbase[0] = 0x00;
	layer_colorbase[1] = 0x40;
	layer_colorbase[2] = 0x80;
	layer_colorbase[3] = 0xc0;

	sound_nmi_enable = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x040000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROMExp0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROMExp1		= Next; Next += 0x800000;

	DrvSndROM		= Next; Next += 0x200000;

	DrvEeprom		= Next; Next += 0x000080;

	konami_palette32	= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x002000;
	DrvPalRAM		= Next; Next += 0x004000;

	DrvZ80RAM		= Next; Next += 0x000800;

	soundlatch		= Next; Next += 0x000001;
	soundlatch2		= Next; Next += 0x000001;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
#if 0
	INT32 Plane0[8] = { STEP4((0x200000*8), 1), STEP4(0,1) };
	INT32 XOffs0[8] = { STEP8(0,4) };
	INT32 YOffs0[8] = { STEP8(0,32) };

	INT32 Plane1[6] = { (0x200000*8)+8, (0x200000*8)+0, STEP4(24, -8) };
	INT32 XOffs1[16] = { STEP8(0,7), STEP8(256, 1) };
	INT32 YOffs1[16] = { STEP8(0,32), STEP8(512,32) };
#endif
	INT32 Plane0[8] = { 0+(0x200000*8), 1+(0x200000*8), 2+(0x200000*8), 3+(0x200000*8), 0, 1, 2, 3 };
	INT32 XOffs0[8] = { 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 };
	INT32 YOffs0[8] = { 0*8*4, 1*8*4, 2*8*4, 3*8*4, 4*8*4, 5*8*4, 6*8*4, 7*8*4 };

	INT32 Plane1[6] = { (0x200000*8)+8, (0x200000*8)+0, 24, 16, 8, 0  };
	INT32 XOffs1[16] = { 0, 1, 2, 3, 4, 5, 6, 7,8*32+0, 8*32+1, 8*32+2, 8*32+3, 8*32+4, 8*32+5, 8*32+6, 8*32+7 };
	INT32 YOffs1[16] = { 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32, 16*32, 17*32, 18*32, 19*32, 20*32, 21*32, 22*32, 23*32 };

	GfxDecode(0x10000, 8,  8,  8, Plane0, XOffs0, YOffs0, 8*8*4, DrvGfxROM0, DrvGfxROMExp0);
	GfxDecode(0x04000, 6, 16, 16, Plane1, XOffs1, YOffs1, 128*8, DrvGfxROM1, DrvGfxROMExp1);

	return 0;
}

static INT32 DrvInit(INT32 flipy)
{
	screen_flip = (flipy) ? 0 : 1;

	GenericTilesInit();

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x000000,  0, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  1, 1)) return 1;

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  2, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  3, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x200002,  4, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x200000,  5, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  6, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  7, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x200000,  8, 4, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  9, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 10, 1)) return 1;

		DrvGfxDecode();
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(DrvMainROM + 0x00000,	0x0000, 0x1fff, MAP_ROM);
 	HD6309MapMemory(DrvMainRAM,		0x2000, 0x3fff, MAP_RAM);
	HD6309MapMemory(DrvMainROM + 0x38000,	0x8000, 0xffff, MAP_ROM);
	HD6309SetReadHandler(lethal_main_read);
	HD6309SetWriteHandler(lethal_main_write);
	HD6309Close();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(lethal_sound_write);
	ZetSetReadHandler(lethal_sound_read);
	ZetClose();

	EEPROMInit(&lethalen_eeprom_interface);

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x400000, lethal_tile_callback);
	K056832SetGlobalOffsets((flipy) ? 216 : 224, 16);
	K056832SetExtLinescroll();

	K053245Init(0, DrvGfxROM1, DrvGfxROMExp1, 0x3fffff, lethal_sprite_callback);
	K053245SetSpriteOffset(0, (flipy) ? (-216+96) : (-224-105), -15);
	K053245SetBpp(0, 6);

	K054539Init(0, 48000, DrvSndROM, 0x200000);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	K054539SetRoute(0, BURN_SND_K054539_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	DrvDoReset();

	BurnGunInit(2, true);

	return 0;
}

static INT32 LethalenInit()
{
	return DrvInit(1);
}

static INT32 LethalenjInit()
{
	return DrvInit(0);
}

static INT32 DrvExit()
{
	GenericTilesExit();

	KonamiICExit();

	HD6309Exit();
	ZetExit();

	EEPROMExit();

	K054539Exit();

	BurnGunExit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x3802/2; i++)
	{
		UINT16 p = (pal[i] >> 8) | (pal[i] << 8);

		INT32 r = (p & 0x1f);
		INT32 g = (p >> 5) & 0x1f;
		INT32 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2); 

		DrvPalette[i] = (r << 16) + (g << 8) + b;
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	if (screen_flip) { // x
		K056832SetLayerOffsets(0, -195, 0);
		K056832SetLayerOffsets(1, -193, 0);
		K056832SetLayerOffsets(2, -191, 0);
		K056832SetLayerOffsets(3, -189, 0);
	} else {
		K056832SetLayerOffsets(0, 188, 0);
		K056832SetLayerOffsets(1, 190, 0);
		K056832SetLayerOffsets(2, 192, 0);
		K056832SetLayerOffsets(3, 194, 0);
	}

	KonamiClearBitmaps(DrvPalette[0x1c00]);

	if (nBurnLayer & 1) K056832Draw(3, K056832_DRAW_FLAG_MIRROR, 1);
	if (nBurnLayer & 2) K056832Draw(2, K056832_DRAW_FLAG_MIRROR, 2);
	if (nBurnLayer & 4) K056832Draw(1, K056832_DRAW_FLAG_MIRROR, 4);

	if (nSpriteEnable & 1) K053245SpritesRender(0);

	if (nBurnLayer & 8) K056832Draw(0, K056832_DRAW_FLAG_MIRROR, 0);

#if 1
	if (screen_flip)	// flip horizontally
	{
		UINT32 tmp;
		for (INT32 y = 0; y < nScreenHeight; y++) {
			UINT32 *src = konami_bitmap32 + (y * nScreenWidth);
			for (INT32 x = 0; x < (nScreenWidth / 2); x++) {
				tmp = src[x];
				src[x] = src[((nScreenWidth - 1) - x)];
				src[((nScreenWidth - 1) - x)] = tmp;
			}
		}
	}
	else			// flip vertically..
	{
		UINT32 tmp[512];
		for (INT32 y = 0; y < (nScreenHeight / 2); y++) {
			UINT32 *src = konami_bitmap32 + (y * nScreenWidth);
			UINT32 *dst = konami_bitmap32 + ((nScreenHeight - 1) - y) * nScreenWidth;
			memcpy (tmp, src, nScreenWidth * 4);
			memcpy (src, dst, nScreenWidth * 4);
			memcpy (dst, tmp, nScreenWidth * 4);
		}
	}
#endif
	KonamiBlendCopy(DrvPalette);
	for (INT32 i = 0; i < nBurnGunNumPlayers; i++) {
		BurnGunDrawTarget(i, BurnGunX[i] >> 8, BurnGunY[i] >> 8);
	}

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;

		if (ReloadGun0) { // for simulated reload-gun button
			DrvJoy1[4] = 1;
		}
		if (ReloadGun1) {
			DrvJoy1[5] = 1;
		}

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		}

		if (!ReloadGun0)
			BurnGunMakeInputs(0, (INT16)LethalGun0, (INT16)LethalGun1);
		if (!ReloadGun1)
			BurnGunMakeInputs(1, (INT16)LethalGun2, (INT16)LethalGun3);
	}

	INT32 nInterleave = nBurnSoundLen;
	INT32 nCyclesTotal[2] = { 3000000 / 60, 6000000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	HD6309Open(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext, nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesSegment = HD6309Run(nCyclesSegment);
		nCyclesDone[0] += nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[1];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[1] += nCyclesSegment;
		if ((i % (nInterleave / 8)) == ((nInterleave / 8) - 1) && sound_nmi_enable) {
			ZetNmi();
		}
	}

	if (K056832IsIrqEnabled()) {
		HD6309SetIRQLine(0, CPU_IRQSTATUS_AUTO);
	}

	if (pBurnSoundOut) {
		memset (pBurnSoundOut, 0, nBurnSoundLen * 2 * 2);
		K054539Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	HD6309Close();

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

		HD6309Scan(nAction);
		ZetScan(nAction);

		K054539Scan(nAction);

		KonamiICScan(nAction);
		BurnGunScan();

		SCAN_VAR(current_4800_bank);
		SCAN_VAR(sound_nmi_enable);
		SCAN_VAR(layer_colorbase);
		SCAN_VAR(sprite_colorbase);
	}

	return 0;
}


// Lethal Enforcers (ver UAE, 11/19/92 15:04)

static struct BurnRomInfo lethalenRomDesc[] = {
	{ "191uae01.u4",	0x040000, 0xdca340e3, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethalenue.nv",	0x000080, 0x6e7224e6, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethalen)
STD_ROM_FN(lethalen)

struct BurnDriver BurnDrvLethalen = {
	"lethalen", NULL, NULL, NULL, "1992",
	"Lethal Enforcers (ver UAE, 11/19/92 15:04)\0", NULL, "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethalenRomInfo, lethalenRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Lethal Enforcers (ver UAB, 09/01/92 11:12)

static struct BurnRomInfo lethalenubRomDesc[] = {
	{ "191uab01.u4",	0x040000, 0x2afd7528, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethalenub.nv",	0x000080, 0x14c6c6e5, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethalenub)
STD_ROM_FN(lethalenub)

struct BurnDriver BurnDrvLethalenub = {
	"lethalenub", "lethalen", NULL, NULL, "1992",
	"Lethal Enforcers (ver UAB, 09/01/92 11:12)\0", NULL, "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethalenubRomInfo, lethalenubRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Lethal Enforcers (ver UAA, 08/17/92 21:38)

static struct BurnRomInfo lethalenuaRomDesc[] = {
	{ "191uaa01.u4",	0x040000, 0xab6b8f16, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethalenua.nv",	0x000080, 0xf71ad1c3, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethalenua)
STD_ROM_FN(lethalenua)

struct BurnDriver BurnDrvLethalenua = {
	"lethalenua", "lethalen", NULL, NULL, "1992",
	"Lethal Enforcers (ver UAA, 08/17/92 21:38)\0", NULL, "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethalenuaRomInfo, lethalenuaRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Lethal Enforcers (ver unknown, US, 08/06/92 15:11, hacked/proto?)

static struct BurnRomInfo lethalenuxRomDesc[] = {
	{ "191xxx01.u4",	0x040000, 0xa3b9e790, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethalenux.nv",	0x000080, 0x5d69c39d, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethalenux)
STD_ROM_FN(lethalenux)

struct BurnDriver BurnDrvLethalenux = {
	"lethalenux", "lethalen", NULL, NULL, "1992",
	"Lethal Enforcers (ver unknown, US, 08/06/92 15:11, hacked/proto?)\0", NULL, "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethalenuxRomInfo, lethalenuxRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Lethal Enforcers (ver EAA, 09/09/92 09:44)

static struct BurnRomInfo lethaleneaaRomDesc[] = {
	{ "191_a01.u4",		0x040000, 0xc6f4d712, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethaleneaa.nv",	0x000080, 0xa85d64ee, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethaleneaa)
STD_ROM_FN(lethaleneaa)

struct BurnDriver BurnDrvLethaleneaa = {
	"lethaleneaa", "lethalen", NULL, NULL, "1992",
	"Lethal Enforcers (ver EAA, 09/09/92 09:44)\0", NULL, "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethaleneaaRomInfo, lethaleneaaRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Lethal Enforcers (ver EAB, 10/14/92 19:53)

static struct BurnRomInfo lethaleneabRomDesc[] = {
	{ "191eab01.u4",	0x040000, 0xd7ce111e, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethaleneab.nv",	0x000080, 0x4e9bb34d, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethaleneab)
STD_ROM_FN(lethaleneab)

struct BurnDriver BurnDrvLethaleneab = {
	"lethaleneab", "lethalen", NULL, NULL, "1992",
	"Lethal Enforcers (ver EAB, 10/14/92 19:53)\0", NULL, "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethaleneabRomInfo, lethaleneabRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Lethal Enforcers (ver EAE, 11/19/92 16:24)

static struct BurnRomInfo lethaleneaeRomDesc[] = {
	{ "191eae01.u4",	0x040000, 0xc6a3c6ac, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethaleneae.nv",	0x000080, 0xeb369a67, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethaleneae)
STD_ROM_FN(lethaleneae)

struct BurnDriver BurnDrvLethaleneae = {
	"lethaleneae", "lethalen", NULL, NULL, "1992",
	"Lethal Enforcers (ver EAE, 11/19/92 16:24)\0", NULL, "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethaleneaeRomInfo, lethaleneaeRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};


// Lethal Enforcers (ver JAD, 12/04/92 17:16)

static struct BurnRomInfo lethalenjRomDesc[] = {
	{ "191jad01.u4",	0x040000, 0x160a25c0, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "191a02.f4",		0x010000, 0x72b843cc, 2 | BRF_PRG | BRF_ESS }, //  1 Z80 Code

	{ "191a08",			0x100000, 0x555bd4db, 3 | BRF_GRA },           //  2 K056832 Characters
	{ "191a10",			0x100000, 0x2fa9bf51, 3 | BRF_GRA },           //  3
	{ "191a07",			0x100000, 0x1dad184c, 3 | BRF_GRA },           //  4
	{ "191a09",			0x100000, 0xe2028531, 3 | BRF_GRA },           //  5

	{ "191a05",			0x100000, 0xf2e3b58b, 4 | BRF_GRA },           //  6 K053244 Sprites
	{ "191a04",			0x100000, 0x5c3eeb2b, 4 | BRF_GRA },           //  7
	{ "191a06",			0x100000, 0xee11fc08, 4 | BRF_GRA },           //  8

	{ "191a03",			0x200000, 0x9b13fbe8, 5 | BRF_SND },           //  9 K054539 Samples

	{ "lethalenj.nv",	0x000080, 0x20b28f2f, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(lethalenj)
STD_ROM_FN(lethalenj)

struct BurnDriverD BurnDrvLethalenj = {
	"lethalenj", "lethalen", NULL, NULL, "1992",
	"Lethal Enforcers (ver JAD, 12/04/92 17:16)\0", "no sprites!", "Konami", "GX191",
	NULL, NULL, NULL, NULL,
	BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SHOOT, 0,
	NULL, lethalenjRomInfo, lethalenjRomName, NULL, NULL, LethalenInputInfo, LethalenDIPInfo,
	LethalenjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x2000,
	288, 224, 4, 3
};
