// FB Alpha Asterix driver module
// Based on MAME driver by Olivier Galibert

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "burn_ym2151.h"
#include "konamiic.h"
#include "k053260.h"
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
static UINT8 *Drv68KRAM0;
static UINT8 *Drv68KRAM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;
static UINT8 *RamEnd;
static UINT8 *MemEnd;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT16 spritebank;

static INT32 layerpri[3];
static INT32 layer_colorbase[4];
static INT32 spritebanks[4];
static INT32 sprite_colorbase;
static UINT32 tilebanks[4];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];
static UINT8 DrvDips[1];

static INT32 nCyclesDone[2];

static UINT16 prot[2];

static struct BurnInputInfo AsterixInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 10,	"service"	},
	{"Dip",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Asterix)

static struct BurnDIPInfo AsterixDIPList[]=
{
	{0x12, 0xff, 0xff, 0x04, NULL		},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x12, 0x01, 0x04, 0x04, "Off"		},
	{0x12, 0x01, 0x04, 0x00, "On"		},
};

STDDIPINFO(Asterix)

static void protection_write(UINT8 offset, UINT16 data)
{
	offset = (offset / 2) & 1;

	prot[offset] = data;

	if (offset == 1)
	{
		UINT32 cmd = (prot[0] << 16) | prot[1];

		switch (cmd >> 24)
		{
			case 0x64:
			{
				UINT32 param1 = (SekReadWord((cmd & 0xffffff) + 0) << 16) | SekReadWord((cmd & 0xffffff) + 2);
				UINT32 param2 = (SekReadWord((cmd & 0xffffff) + 4) << 16) | SekReadWord((cmd & 0xffffff) + 6);
	
				switch (param1 >> 24)
				{
					case 0x22:
					{
						int size = param2 >> 24;
						param1 &= 0xffffff;
						param2 &= 0xffffff;
						while(size >= 0)
						{
							SekWriteWord(param2, SekReadWord(param1));
							param1 += 2;
							param2 += 2;
							size--;
						}
						break;
					}
				}
				break;
			}
		}
	}
}

static void reset_spritebank()
{
	K053244BankSelect(0, spritebank & 7);

	spritebanks[0] = (spritebank << 12) & 0x7000;
	spritebanks[1] = (spritebank <<  9) & 0x7000;
	spritebanks[2] = (spritebank <<  6) & 0x7000;
	spritebanks[3] = (spritebank <<  3) & 0x7000;
}

static void _fastcall asterix_main_write_word(UINT32 address, UINT16 data)
{
//bprintf (0, _T("WW %5.5x, %4.4x\n"), address, data);

	if ((address & 0xfff000) == 0x400000) {
		K056832HalfRamWriteWord(address & 0xfff, data);
		return;
	}

	if ((address & 0xfffff0) == 0x200000) {
		K053244Write(0, (address & 0xe) + 0, data >> 8);
		K053244Write(0, (address & 0xe) + 1, data & 0xff);
		return;
	}

	if ((address & 0xffffe0) == 0x300000) {
		K053244Write(0, (address & 0xe) / 2, data & 0xff);
		return;
	}

	if ((address & 0xfffff8) == 0x380700) {
		return; // regsb
	}

	if ((address & 0xffffc0) == 0x440000) {
		K056832WordWrite(address & 0x3e, data);
		return;
	}

	switch (address)
	{
		case 0x380100:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
			K056832SetTileBank((data & 0x20) >> 5);
		return;

		case 0x380400:
			spritebank = data;
			reset_spritebank();
		return;

		case 0x380600:
		case 0x380601:
		return;		// nop

		case 0x380800:
		case 0x380802:
			protection_write(address, data);
		return;
	}
}

static void _fastcall asterix_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfff000) == 0x400000) {
		K056832HalfRamWriteByte(address & 0xfff, data);
		return;
	}

	if ((address & 0xfffff0) == 0x200000) {
		K053244Write(0, address & 0xf, data);
		return;
	}

	if ((address & 0xffffe1) == 0x300001) {
		K053244Write(0, (address & 0xe) / 2, data);
		return;
	}

	if ((address & 0xffffe1) == 0x380501) {
		K053251Write((address / 2) & 0xf, data);
		return;
	}

	if ((address & 0xfffff8) == 0x380700) {
		return;	// regsb
	}

	if ((address & 0xffffc0) == 0x440000) {
		K056832ByteWrite(address & 0x3f, data);
		return;
	}

	switch (address)
	{
		case 0x380101:
			EEPROMWrite((data & 0x04), (data & 0x02), (data & 0x01));
			K056832SetTileBank((data & 0x20) >> 5);
		return;

		case 0x380201:
		case 0x380203:
			K053260Write(0, (address / 2) & 1, data);
		return;

		case 0x380301:
			ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x380601:
		return; // nop
	}
}

static UINT16 _fastcall asterix_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x200000) {
		return (K053244Read(0, address & 0xe) << 8) | K053244Read(0, (address & 0xe) + 1);
	}

	if ((address & 0xffffe0) == 0x300000) {
		return K053244Read(0, (address & 0xe)/2);
	}

	if ((address & 0xfff000) == 0x400000) {
		return K056832HalfRamReadWord(address & 0xfff);
	}

	if ((address & 0xffe000) == 0x420000) {
		return K056832RomWordRead(address);
	}

	switch (address)
	{
		case 0x380000:
			return DrvInputs[0];

		case 0x380002:
			return (DrvInputs[1] & ~0xf900) | (EEPROMRead() ? 0x100 : 0);
	}

	return 0;
}

static UINT8 _fastcall asterix_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x200000) {
		return K053244Read(0, address & 0xf);
	}

	if ((address & 0xffffe0) == 0x300000) {
		return K053244Read(0, (address & 0xe)/2);
	}

	if ((address & 0xfff000) == 0x400000) {
		return K056832HalfRamReadByte(address & 0xfff);
	}

	if ((address & 0xffe000) == 0x420000) {
		return K056832RomWordRead(address) >> ((~address & 1) * 8);
	}

	switch (address)
	{
		case 0x380000:
			return DrvInputs[0] >> 8;

		case 0x380001:
			return DrvInputs[0];

		case 0x380002:
			return ((DrvInputs[1] >> 8) & 0x06) | (EEPROMRead() ? 0x01 : 0);

		case 0x380003:
			return DrvInputs[1];

		case 0x380201:
		case 0x380203:
			return K053260Read(0, ((address / 2) & 1) + 2);

		case 0x380600:
		case 0x380601:
			return 0; // nop
	}

	return 0;
}

static void __fastcall asterix_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf801:
			BurnYM2151WriteRegister(data);
		return;

		case 0xfc00:
			nCyclesDone[1] += ZetRun(100);
			ZetNmi();
		return;

		case 0xfe00:
			BurnYM2151SelectRegister(data);
		return;
	}

	if (address >= 0xfa00 && address <= 0xfa2f) {
		K053260Write(0, address & 0x3f, data);
	}
}

static UINT8 __fastcall asterix_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xf801:
			return BurnYM2151ReadStatus();
	}

	if (address >= 0xfa00 && address <= 0xfa2f) {
		if ((address & 0x3e) == 0x00) ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);

		return K053260Read(0, address & 0x3f);
	}

	return 0;
}
static void asterix_sprite_callback(INT32 *code, INT32 *color, INT32 *priority)
{
	INT32 pri = (*color & 0x00e0) >> 2;
	if (pri <= layerpri[2])					*priority = 0;
	else if (pri > layerpri[2] && pri <= layerpri[1])	*priority = 0xf0;
	else if (pri > layerpri[1] && pri <= layerpri[0])	*priority = 0xfc;
	else							*priority = 0xfe;
	*color = sprite_colorbase | (*color & 0x001f);
	*code = (*code & 0xfff) | spritebanks[(*code >> 12) & 3];
}

static void asterix_tile_callback(INT32 layer, INT32 *code, INT32 *color, INT32 *flags)
{
	*flags = (*code & 0x1000) ? 1 : 0;

	*color = (layer_colorbase[layer] + ((*code & 0xe000) >> 13)) & 0x7f;
	*code = (*code & 0x03ff) | tilebanks[(*code >> 10) & 3];
}

static const eeprom_interface asterix_eeprom_interface =
{
	7,
	8,
	"111000",
	"111100",
	"1100100000000",
	"1100000000000",
	"1100110000000",
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

	EEPROMReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(DrvEeprom, 0, 128);
	}

	KonamiICReset();

	BurnYM2151Reset();
	K053260Reset(0);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x100000;
	DrvZ80ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROMExp0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x400000;
	DrvGfxROMExp1		= Next; Next += 0x800000;

	DrvSndROM		= Next; Next += 0x200000;

	DrvEeprom		= Next; Next += 0x000080;

	konami_palette32	= (UINT32*)Next;
	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	Drv68KRAM0		= Next; Next += 0x008000;
	Drv68KRAM1		= Next; Next += 0x000800;

	DrvZ80RAM		= Next; Next += 0x000800;

	DrvPalRAM		= Next; Next += 0x001000;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane1[4]  = { STEP4(24, -8) };
	INT32 XOffs1[16] = { STEP8(0,1), STEP8(256,1) };
	INT32 YOffs1[16] = { STEP8(0,32), STEP8(512,32) };

	K053247GfxDecode(DrvGfxROM0, DrvGfxROMExp0, 0x100000);

	GfxDecode(0x8000, 4, 16, 16, Plane1, XOffs1, YOffs1, 128*8, DrvGfxROM1, DrvGfxROMExp1);

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

		if (BurnLoadRomExt(DrvGfxROM0 + 0x000000,  5, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM0 + 0x000002,  6, 4, 2)) return 1;

		if (BurnLoadRomExt(DrvGfxROM1 + 0x000000,  7, 4, 2)) return 1;
		if (BurnLoadRomExt(DrvGfxROM1 + 0x000002,  8, 4, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  9, 1)) return 1;

		if (BurnLoadRom(DrvEeprom  + 0x000000, 10, 1)) return 1;

		DrvGfxDecode();
	}

	K056832Init(DrvGfxROM0, DrvGfxROMExp0, 0x100000, asterix_tile_callback);
	K056832SetGlobalOffsets(112, 16);

	K053245Init(0, DrvGfxROM1, DrvGfxROMExp1, 0x3fffff, asterix_sprite_callback);
	K053245SetSpriteOffset(0, -115, 15);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM0,	0x100000, 0x107fff, MAP_RAM);
	SekMapMemory(K053245Ram[0],	0x180000, 0x1807ff, MAP_RAM);
	SekMapMemory(Drv68KRAM1,	0x180800, 0x180fff, MAP_RAM);
	SekMapMemory(DrvPalRAM,		0x280000, 0x280fff, MAP_RAM);
	SekSetWriteWordHandler(0,	asterix_main_write_word);
	SekSetWriteByteHandler(0,	asterix_main_write_byte);
	SekSetReadWordHandler(0,	asterix_main_read_word);
	SekSetReadByteHandler(0,	asterix_main_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,		0x0000, 0xefff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(asterix_sound_write);
	ZetSetReadHandler(asterix_sound_read);
	ZetClose();

	EEPROMInit(&asterix_eeprom_interface);

	BurnYM2151Init(4000000);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 1.00, BURN_SND_ROUTE_LEFT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 1.00, BURN_SND_ROUTE_RIGHT);

	K053260Init(0, 4000000, DrvSndROM, 0x200000);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_1, 0.75, BURN_SND_ROUTE_LEFT);
	K053260SetRoute(0, BURN_SND_K053260_ROUTE_2, 0.75, BURN_SND_ROUTE_RIGHT);

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
	K053260Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteRecalc()
{
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x1000/2; i++)
	{
		INT32 r = (pal[i] & 0x1f);
		INT32 g = (pal[i] >> 5) & 0x1f;
		INT32 b = (pal[i] >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2); 

		DrvPalette[i] = (r << 16) + (g << 8) + b;
	}
}

static INT32 DrvDraw()
{
	DrvPaletteRecalc();

	INT32 layer[3];

	tilebanks[0] = (K056832GetLookup(0) << 10);
	tilebanks[1] = (K056832GetLookup(1) << 10);
	tilebanks[2] = (K056832GetLookup(2) << 10);
	tilebanks[3] = (K056832GetLookup(3) << 10);

	layer_colorbase[0] = K053251GetPaletteIndex(0);
	layer_colorbase[1] = K053251GetPaletteIndex(2);
	layer_colorbase[2] = K053251GetPaletteIndex(3);
	layer_colorbase[3] = K053251GetPaletteIndex(4);

	sprite_colorbase   = K053251GetPaletteIndex(1);

	KonamiClearBitmaps(DrvPalette[0]);

	if (K056832ReadRegister(0) & 0x10)
	{
		K056832SetLayerOffsets(0, 89 - 176, 0);
		K056832SetLayerOffsets(1, 91 - 176, 0);
		K056832SetLayerOffsets(2, 89 - 176, 0);
		K056832SetLayerOffsets(3, 95 - 176, 0);
	}
	else
	{
		K056832SetLayerOffsets(0, 89, 0);
		K056832SetLayerOffsets(1, 91, 0);
		K056832SetLayerOffsets(2, 89, 0);
		K056832SetLayerOffsets(3, 95, 0);
	}

	layer[0] = 0;
	layerpri[0] = K053251GetPriority(0);
	layer[1] = 1;
	layerpri[1] = K053251GetPriority(2);
	layer[2] = 3;
	layerpri[2] = K053251GetPriority(4);

	konami_sortlayers3(layer, layerpri);

	if (nBurnLayer & 1) K056832Draw(layer[0], K056832_DRAW_FLAG_MIRROR, 1);
	if (nBurnLayer & 2) K056832Draw(layer[1], K056832_DRAW_FLAG_MIRROR, 2);
	if (nBurnLayer & 4) K056832Draw(layer[2], K056832_DRAW_FLAG_MIRROR, 4);

	if (nSpriteEnable & 1) K053245SpritesRender(0);

	if (nBurnLayer & 8) K056832Draw(2, K056832_DRAW_FLAG_MIRROR, 0);

	KonamiBlendCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0x07ff;
		DrvInputs[1] = ((DrvDips[0] & 0x04) << 8) | 0x02ff;
		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = nBurnSoundLen;
	INT32 nSoundBufferPos = 0;
	INT32 nCyclesTotal[2] = { 12000000 / 60, 8000000 / 60 };
	nCyclesDone[0] = nCyclesDone[1] = 0;

	SekOpen(0);
	ZetOpen(0);

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext, nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[0] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[0];
		nCyclesSegment = SekRun(nCyclesSegment);
		nCyclesDone[0] += nCyclesSegment;

		nNext = (i + 1) * nCyclesTotal[1] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[1];
		nCyclesSegment = ZetRun(nCyclesSegment);
		nCyclesDone[1] += nCyclesSegment;

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (K056832IsIrqEnabled()) SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			K053260Update(0, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
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
		K053260Scan(nAction);

		KonamiICScan(nAction);

		SCAN_VAR(prot[0]);
		SCAN_VAR(prot[1]);

		SCAN_VAR(spritebank);
	}

	if (nAction & ACB_WRITE) {
		reset_spritebank();
	}

	EEPROMScan(nAction, pnMin);

	return 0;
}


// Asterix (ver EAD)

static struct BurnRomInfo asterixRomDesc[] = {
	{ "068_ea_d01.8c",	0x020000, 0x61d6621d, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "068_ea_d02.8d",	0x020000, 0x53aac057, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "068a03.7c",		0x020000, 0x8223ebdc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "068a04.7d",		0x020000, 0x9f351828, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "068_a05.5f",		0x010000, 0xd3d0d77b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "068a12.16k",		0x080000, 0xb9da8e9c, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "068a11.12k",		0x080000, 0x7eb07a81, 3 | BRF_GRA },           //  6

	{ "068a08.7k",		0x200000, 0xc41278fe, 4 | BRF_GRA },           //  7 K053245 Sprites
	{ "068a07.3k",		0x200000, 0x32efdbc4, 4 | BRF_GRA },           //  8

	{ "068a06.1e",		0x200000, 0x6df9ec0e, 5 | BRF_SND },           //  9 K053260 Samples

	{ "asterix.nv",		0x000080, 0x490085c8, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(asterix)
STD_ROM_FN(asterix)

struct BurnDriver BurnDrvAsterix = {
	"asterix", NULL, NULL, NULL, "1992",
	"Asterix (ver EAD)\0", NULL, "Konami", "GX068",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, asterixRomInfo, asterixRomName, NULL, NULL, AsterixInputInfo, AsterixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Asterix (ver EAC)

static struct BurnRomInfo asterixeacRomDesc[] = {
	{ "068_ea_c01.8c",	0x020000, 0x0ccd1feb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "068_ea_c02.8d",	0x020000, 0xb0805f47, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "068a03.7c",		0x020000, 0x8223ebdc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "068a04.7d",		0x020000, 0x9f351828, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "068_a05.5f",		0x010000, 0xd3d0d77b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "068a12.16k",		0x080000, 0xb9da8e9c, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "068a11.12k",		0x080000, 0x7eb07a81, 3 | BRF_GRA },           //  6

	{ "068a08.7k",		0x200000, 0xc41278fe, 4 | BRF_GRA },           //  7 K053245 Sprites
	{ "068a07.3k",		0x200000, 0x32efdbc4, 4 | BRF_GRA },           //  8

	{ "068a06.1e",		0x200000, 0x6df9ec0e, 5 | BRF_SND },           //  9 K053260 Samples

	{ "asterixeac.nv",	0x000080, 0x490085c8, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(asterixeac)
STD_ROM_FN(asterixeac)

struct BurnDriver BurnDrvAsterixeac = {
	"asterixeac", "asterix", NULL, NULL, "1992",
	"Asterix (ver EAC)\0", NULL, "Konami", "GX068",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, asterixeacRomInfo, asterixeacRomName, NULL, NULL, AsterixInputInfo, AsterixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Asterix (ver EAA)

static struct BurnRomInfo asterixeaaRomDesc[] = {
	{ "068_ea_a01.8c",	0x020000, 0x85b41d8e, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "068_ea_a02.8d",	0x020000, 0x8e886305, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "068a03.7c",		0x020000, 0x8223ebdc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "068a04.7d",		0x020000, 0x9f351828, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "068_a05.5f",		0x010000, 0xd3d0d77b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "068a12.16k",		0x080000, 0xb9da8e9c, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "068a11.12k",		0x080000, 0x7eb07a81, 3 | BRF_GRA },           //  6

	{ "068a08.7k",		0x200000, 0xc41278fe, 4 | BRF_GRA },           //  7 K053245 Sprites
	{ "068a07.3k",		0x200000, 0x32efdbc4, 4 | BRF_GRA },           //  8

	{ "068a06.1e",		0x200000, 0x6df9ec0e, 5 | BRF_SND },           //  9 K053260 Samples

	{ "asterixeaa.nv",	0x000080, 0x30275de0, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(asterixeaa)
STD_ROM_FN(asterixeaa)

struct BurnDriver BurnDrvAsterixeaa = {
	"asterixeaa", "asterix", NULL, NULL, "1992",
	"Asterix (ver EAA)\0", NULL, "Konami", "GX068",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, asterixeaaRomInfo, asterixeaaRomName, NULL, NULL, AsterixInputInfo, AsterixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Asterix (ver AAD)

static struct BurnRomInfo asterixaadRomDesc[] = {
	{ "068_aa_d01.8c",	0x020000, 0x3fae5f1f, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "068_aa_d02.8d",	0x020000, 0x171f0ba0, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "068a03.7c",		0x020000, 0x8223ebdc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "068a04.7d",		0x020000, 0x9f351828, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "068_a05.5f",		0x010000, 0xd3d0d77b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "068a12.16k",		0x080000, 0xb9da8e9c, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "068a11.12k",		0x080000, 0x7eb07a81, 3 | BRF_GRA },           //  6

	{ "068a08.7k",		0x200000, 0xc41278fe, 4 | BRF_GRA },           //  7 K053245 Sprites
	{ "068a07.3k",		0x200000, 0x32efdbc4, 4 | BRF_GRA },           //  8

	{ "068a06.1e",		0x200000, 0x6df9ec0e, 5 | BRF_SND },           //  9 K053260 Samples

	{ "asterixaad.nv",	0x000080, 0xbcca86a7, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(asterixaad)
STD_ROM_FN(asterixaad)

struct BurnDriver BurnDrvAsterixaad = {
	"asterixaad", "asterix", NULL, NULL, "1992",
	"Asterix (ver AAD)\0", NULL, "Konami", "GX068",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, asterixaadRomInfo, asterixaadRomName, NULL, NULL, AsterixInputInfo, AsterixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};


// Asterix (ver JAD)

static struct BurnRomInfo asterixjRomDesc[] = {
	{ "068_ja_d01.8c",	0x020000, 0x2bc10940, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "068_ja_d02.8d",	0x020000, 0xde438300, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "068a03.7c",		0x020000, 0x8223ebdc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "068a04.7d",		0x020000, 0x9f351828, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "068_a05.5f",		0x010000, 0xd3d0d77b, 2 | BRF_PRG | BRF_ESS }, //  4 Z80 Code

	{ "068a12.16k",		0x080000, 0xb9da8e9c, 3 | BRF_GRA },           //  5 K056832 Characters
	{ "068a11.12k",		0x080000, 0x7eb07a81, 3 | BRF_GRA },           //  6

	{ "068a08.7k",		0x200000, 0xc41278fe, 4 | BRF_GRA },           //  7 K053245 Sprites
	{ "068a07.3k",		0x200000, 0x32efdbc4, 4 | BRF_GRA },           //  8

	{ "068a06.1e",		0x200000, 0x6df9ec0e, 5 | BRF_SND },           //  9 K053260 Samples

	{ "asterixj.nv",	0x000080, 0x84229f2c, 6 | BRF_OPT },           // 10 eeprom data
};

STD_ROM_PICK(asterixj)
STD_ROM_FN(asterixj)

struct BurnDriver BurnDrvAsterixj = {
	"asterixj", "asterix", NULL, NULL, "1992",
	"Asterix (ver JAD)\0", NULL, "Konami", "GX068",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_PREFIX_KONAMI, GBF_SCRFIGHT, 0,
	NULL, asterixjRomInfo, asterixjRomName, NULL, NULL, AsterixInputInfo, AsterixDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	288, 224, 4, 3
};
