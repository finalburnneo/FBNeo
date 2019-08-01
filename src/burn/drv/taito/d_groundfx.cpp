// FB Alpha Taito Ground Effects driver module
// Based on MAME driver by Bryan McPhail and David Graves

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "taito.h"
#include "taito_ic.h"
#include "taitof3_snd.h"
#include "eeprom.h"
#include "watchdog.h"

static UINT8 DrvRecalc;

static INT32 interrupt5_timer = -1;
static INT32 coin_word;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo GroundfxInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	TaitoInputPort1 + 2,	"p1 coin"	},
	{"P1 Brake",		BIT_DIGITAL,	TaitoInputPort0 + 1,	"p1 fire 1"	},
	{"P1 Shift Lo",		BIT_DIGITAL,	TaitoInputPort0 + 4,	"p1 fire 2"	},
	{"P1 Shift Hi",		BIT_DIGITAL,	TaitoInputPort0 + 0,	"p1 fire 3"	},
	A("P1 Steering",    BIT_ANALOG_REL, &TaitoAnalogPort0,      "p1 x-axis" ),
	A("P1 Accelerator", BIT_ANALOG_REL, &TaitoAnalogPort1,      "p1 fire 1" ),

	{"P2 Coin",			BIT_DIGITAL,	TaitoInputPort1 + 3,	"p2 coin"	},

	{"Reset",			BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	TaitoInputPort1 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	TaitoInputPort1 + 0,	"diag"	    },
};

STDINPUTINFO(Groundfx)
#undef A

static void __fastcall groundfx_main_write_long(UINT32 a, UINT32 d)
{
	TC0100SCN0LongWrite_Map(0x900000, 0x90ffff)

	switch (a)
	{
		case 0x304000:
		case 0x304400: // nop?
		case 0x400000: // gun feedback
		return;
	}

	bprintf (0, _T("WL: %5.5x, %8.8x\n"), a, d);
}

static void __fastcall groundfx_main_write_word(UINT32 a, UINT16 d)
{
	TC0100SCN0WordWrite_Map(0x900000, 0x90ffff)

	if ((a & 0xffffc0) == 0x830000) {
		TC0480SCPCtrlWordWrite((a / 2) & 0x1f, d);
		return;
	}

	if ((a & 0xfffff0) == 0x920000) {
		TC0100SCNCtrlWordWrite(0, (a / 2) & 7, d);
		return;
	}

	switch (a)
	{
		case 0xd00000:
		case 0xd00002:
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), a, d);
}

static void __fastcall groundfx_main_write_byte(UINT32 a, UINT8 d)
{

	TC0100SCN0ByteWrite_Map(0x900000, 0x90ffff)

	switch (a)
	{
		case 0x400000:
		case 0x400001:
		case 0x400002:
		case 0x400003:
			// gun feedback
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
			interrupt5_timer = 1; // adc timer irq
		return;

		case 0xd00000:
		case 0xd00001:
		case 0xd00002:
		case 0xd00003:
		case 0xf00000:
		case 0xf00001:
		case 0xf00002:
		case 0xf00003:
		case 0xc00000:
		case 0xc00001:
		case 0xc00002:
		case 0xc00003:
		case 0xc00004:
		case 0xc00005:
		case 0xc00006:
		case 0xc00007:
			// seat, rotate, network, etc?
		return;
	}

	bprintf (0, _T("WB: %5.5x, %2.2x\n"), a, d);
}

static UINT32 __fastcall groundfx_main_read_long(UINT32 address)
{
	bprintf (0, _T("RL: %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall groundfx_main_read_word(UINT32 address)
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

static UINT8 __fastcall groundfx_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x500000:
		case 0x500001:
			return 0xff;

		case 0x500002:
			return TaitoInput[0]; // buttons

		case 0x500003:
			return (EEPROMRead() ? 0x80 : 0) | (nCurrentFrame & 1) | 0x7e;

		case 0x500004:
			return 0xff;

		case 0x500005:
			return 0xff;

		case 0x500006:
			return 0xff;

		case 0x500007:
			return TaitoInput[1]; // system

		case 0x600000:
		case 0x600001: return 0;
		case 0x600002: return ProcessAnalog(TaitoAnalogPort0, 1, INPUT_DEADZONE, 0x00, 0xff);
		case 0x600003: return 0xff - ProcessAnalog(TaitoAnalogPort1, 0, INPUT_DEADZONE | INPUT_MIGHTBEDIGITAL, 0x00, 0xff);

		case 0xc00000:
		case 0xc00001:
		case 0xc00002:
		case 0xc00003:
		case 0xc00004:
		case 0xc00005:
		case 0xc00006:
		case 0xc00007:// nop
			return 0;
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (TaitoRamStart, 0, TaitoRamEnd - TaitoRamStart);
	}

	SekReset(0);

	TaitoICReset();
	TaitoF3SoundReset();

	BurnWatchdogReset();

	EEPROMReset();
	if (EEPROMAvailable() == 0) {
		EEPROMFill(TaitoDefaultEEProm, 0, 0x80);
	}

	interrupt5_timer = -1;
	coin_word = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1		= Next; Next += 0x200000;
	Taito68KRom2		= Next;
	TaitoF3SoundRom		= Next; Next += 0x100000;

	TaitoChars			= Next; Next += 0x800000;

	TaitoSpritesA		= Next; Next += 0x1000000;
	TaitoCharsPivot		= Next; Next += 0x800000;

	TaitoSpriteMapRom	= Next; Next += 0x080000;

	TaitoDefaultEEProm	= Next; Next += 0x000080;

	TaitoES5505Rom		= Next;
	TaitoF3ES5506Rom	= Next; Next += 0x1000000;

	TaitoPalette		= (UINT32*)Next; Next += 0x40000 * sizeof(UINT32);

	TaitoF2SpriteList	= (TaitoF2SpriteEntry*)Next; Next += 0x40000 * sizeof(TaitoF2SpriteEntry);

	TaitoRamStart		= Next;

	TaitoSharedRam		= Next; Next += 0x0004000;
	TaitoSpriteRam		= Next; Next += 0x0040000;
	Taito68KRam1		= Next; Next += 0x0200000;
	TaitoPaletteRam		= Next; Next += 0x0100000;

//	TC0100SCNRam[0]		= Next; Next += 0x010000; // allocated in init
//	TC0480SCPRam		= Next; Next += 0x010000; // allocated in init

	TaitoF3SoundRam		= Next; Next += 0x0100000;	// 64 KB
	TaitoF3SharedRam	= Next; Next += 0x0008000;	// 2 KB
	TaitoES5510DSPRam	= Next; Next += 0x0002000;	// 512 Bytes
	TaitoES5510GPR		= (UINT32 *)Next; Next += 0x0003000;	// 192x4 Bytes
	TaitoES5510DRAM		= (UINT16 *)Next; Next += 0x4000000;	// 4 MB

	TaitoRamEnd		= Next;

	TaitoMemEnd		= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4]  = { STEP4(0,1) };
	INT32 XOffs0[16] = { 1*4, 0*4, 5*4, 4*4, 3*4, 2*4, 7*4, 6*4, 9*4, 8*4, 13*4, 12*4, 11*4, 10*4, 15*4, 14*4 };
	INT32 YOffs0[16] = { STEP16(0,64) };

	INT32 Plane1[5]  = { 0x800000*8, STEP4(0,8) };
	INT32 XOffs1[16] = { STEP8(32,1), STEP8(0,1) };
	INT32 YOffs1[16] = { STEP16(0,64) };

	INT32 Plane2[6] = { 0x200000*8, 0x200000*8+1, STEP4(0,1) };
	INT32 XOffs2[8] = { 2*4, 3*4, 0*4, 1*4, 6*4, 7*4, 4*4, 5*4 };
	INT32 YOffs2[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x1000000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, TaitoChars, 0x400000);

	GfxDecode(0x08000, 4, 16, 16, Plane0, XOffs0, YOffs0, 0x400, tmp, TaitoChars);

	memcpy (tmp, TaitoSpritesA, 0x1000000);

	GfxDecode(0x10000, 5, 16, 16, Plane1, XOffs1, YOffs1, 0x400, tmp, TaitoSpritesA);

	memcpy (tmp, TaitoCharsPivot, 0x400000);

	GfxDecode(0x10000, 6,  8,  8, Plane2, XOffs2, YOffs2, 0x100, tmp, TaitoCharsPivot);

	BurnFree(tmp);

	return 0;
}

static void DrvGfxReorder()
{
	INT32 size = 0x400000;
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

static INT32 DrvInit()
{
	TaitoMem = NULL;
	MemIndex();
	INT32 nLen = TaitoMemEnd - (UINT8 *)0;
	if ((TaitoMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(TaitoMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Taito68KRom1 + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(Taito68KRom1 + 0x000002, k++, 4)) return 1;

		if (BurnLoadRom(Taito68KRom2 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Taito68KRom2 + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(TaitoChars + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoChars + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoSpritesA + 0x800000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000003, k++, 4)) return 1;

		if (BurnLoadRom(TaitoCharsPivot + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoCharsPivot + 0x300000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoSpriteMapRom + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoF3ES5506Rom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0xc00001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoDefaultEEProm  + 0x000000, k++, 1)) return 1;

		DrvGfxReorder();
		DrvGfxDecode();
	}

	GenericTilesInit();
	TC0100SCNInit(0, 0x10000, 50, 16+8, 0, pPrioDraw);
	TC0100SCNSetColourDepth(0, 6);
	TC0480SCPInit(0x4000, 0, 36, 0, -1, 0, 24);
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
	SekMapMemory(TaitoSharedRam,		0xb00000, 0xb003ff, MAP_RAM); // Unknown
	SekSetWriteLongHandler(0,			groundfx_main_write_long);
	SekSetWriteWordHandler(0,			groundfx_main_write_word);
	SekSetWriteByteHandler(0,			groundfx_main_write_byte);
	SekSetReadLongHandler(0,			groundfx_main_read_long);
	SekSetReadWordHandler(0,			groundfx_main_read_word);
	SekSetReadByteHandler(0,			groundfx_main_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	TaitoF3ES5506RomSize = 0x1000000;
	TaitoF3SoundInit(1);

	EEPROMInit(&eeprom_interface_93C46);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	EEPROMExit();
	BurnWatchdogExit();

	TaitoF3SoundExit();
	TaitoICExit();

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

		UINT8 r = color >> 16;
		UINT8 g = color >> 8;
		UINT8 b = color;

		TaitoPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(INT32 do_hack, INT32 x_offs, INT32 y_offs)
{
	UINT32 *spriteram32 = (UINT32*)TaitoSpriteRam;
	UINT16 *spritemap = (UINT16 *)TaitoSpriteMapRom;
	INT32 offs, tilenum, color, flipx, flipy;
	INT32 x, y, priority, dblsize, curx, cury;
	INT32 sprites_flipscreen = 0;
	INT32 zoomx, zoomy, zx, zy;
	INT32 sprite_chunk,map_offset,code,j,k,px,py;
	INT32 dimension,total_chunks;
	static const INT32 primasks[4] = { 0xffff, 0xfffc, 0xfff0, 0xff00 };

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

		color /= 2;
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

		dimension = ((dblsize*2) + 2);
		total_chunks = ((dblsize*3) + 1) << 2;
		map_offset = tilenum << 2;

		for (sprite_chunk=0;sprite_chunk<total_chunks;sprite_chunk++)
		{
			j = sprite_chunk / dimension;
			k = sprite_chunk % dimension;

			px = k;
			py = j;
			if (flipx)  px = dimension-1-k;
			if (flipy)  py = dimension-1-j;

			code = spritemap[map_offset + px + (py<<(dblsize+1))];

			if (code == 0xffff)
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
			sprite_ptr->Colour = (color << 5) | 0x1000;
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

		if (do_hack && sprite_ptr->Priority==1 && sprite_ptr->y<100)
			GenericTilesSetClip(69, 250+1, 5, 44+1);

		RenderZoomedPrioSprite(pTransDraw, TaitoSpritesA,
			sprite_ptr->Code,
			sprite_ptr->Colour, 0,
			sprite_ptr->x, sprite_ptr->y-24,
			sprite_ptr->xFlip,sprite_ptr->yFlip,
			16, 16,
			sprite_ptr->xZoom,sprite_ptr->yZoom, primasks[sprite_ptr->Priority]);

		if (do_hack && sprite_ptr->Priority==1 && sprite_ptr->y<100)
			GenericTilesClearClip();
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
//	}

	BurnTransferClear();

	UINT8 Layer[5];
	UINT16 priority = TC0480SCPGetBgPriority();

	Layer[0] = (priority & 0xf000) >> 12;
	Layer[1] = (priority & 0x0f00) >>  8;
	Layer[2] = (priority & 0x00f0) >>  4;
	Layer[3] = (priority & 0x000f) >>  0;
	Layer[4] = 4;

	// force updates
	//TC0100SCNBgLayerUpdate[0] = 1;
	//TC0100SCNFgLayerUpdate[0] = 1;

	INT32 Disable = TC0100SCNCtrl[0][6] & 0x3;
	if (TC0100SCNBottomLayer(0)) {
		if (nSpriteEnable & 8) if (!(Disable & 0x02)) TC0100SCNRenderFgLayer(0, 1, TaitoCharsPivot);
		if (nSpriteEnable & 4) if (!(Disable & 0x01)) TC0100SCNRenderBgLayer(0, 0, TaitoCharsPivot);
	} else {
		if (nSpriteEnable & 4) if (!(Disable & 0x01)) TC0100SCNRenderBgLayer(0, 1, TaitoCharsPivot);
		if (nSpriteEnable & 8) if (!(Disable & 0x02)) TC0100SCNRenderFgLayer(0, 0, TaitoCharsPivot);
	}

	UINT32 hackAddress0 = *((UINT32*)(TC0100SCNRam[0] + 0x4090));
	UINT32 hackAddress1 = *((UINT32*)(TC0480SCPRam + 0x20));

	if (hackAddress0 || hackAddress1 == 0x08660024)
	{
		if (nBurnLayer & 1) TC0480SCPTilemapRenderPrio(Layer[1], 0, 2, TaitoChars);
		if (nBurnLayer & 2) TC0480SCPTilemapRenderPrio(Layer[2], 0, 4, TaitoChars);
		if (nBurnLayer & 4) TC0480SCPTilemapRenderPrio(Layer[3], 0, 8, TaitoChars);

		if ((nBurnLayer & 8) && (hackAddress1 != 0x08660024)) {
			GenericTilesSetClip(69, 250+1, 5, 44+1);
			TC0480SCPTilemapRenderPrio(Layer[0], 0, 0, TaitoChars);
			GenericTilesClearClip();
		}

		if (nSpriteEnable & 1) draw_sprites(1, 44, -574);
	}
	else
	{
		if (nBurnLayer & 1) TC0480SCPTilemapRenderPrio(Layer[0], 0, 1, TaitoChars);
		if (nBurnLayer & 2) TC0480SCPTilemapRenderPrio(Layer[1], 0, 2, TaitoChars);
		if (nBurnLayer & 4) TC0480SCPTilemapRenderPrio(Layer[2], 0, 4, TaitoChars);
		if (nBurnLayer & 8) TC0480SCPTilemapRenderPrio(Layer[3], 0, 8, TaitoChars);

		TC0100SCNRenderCharLayer(0);

		if (nSpriteEnable & 2) draw_sprites(0, 44, -574);
	}

	TC0480SCPRenderCharLayer();

	BurnTransferCopy(TaitoPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (TaitoReset) {
		DrvDoReset(1);
	}

	{
		TaitoInput[0] = 0xff;
		TaitoInput[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			TaitoInput[0] ^= (TaitoInputPort0[i] & 1) << i;
			TaitoInput[1] ^= (TaitoInputPort1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal = 20000000 / 60;
	INT32 nCyclesDone = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);

		nCyclesDone += SekRun(((i + 1) * nCyclesTotal / nInterleave) - nCyclesDone);

		if (i == 255)
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

		SCAN_VAR(coin_word);
		SCAN_VAR(interrupt5_timer);
	}

	return 0;
}


// Ground Effects / Super Ground Effects (Japan)

static struct BurnRomInfo groundfxRomDesc[] = {
	{ "d51-24.79",	0x080000, 0x5caaa031, TAITO_68KROM1_BYTESWAP32 },	//  0 Main 68ec020 Code
	{ "d51-23.61",	0x080000, 0x462e3c9b, TAITO_68KROM1_BYTESWAP32 },	//  1
	{ "d51-22.77",	0x080000, 0xb6b04d88, TAITO_68KROM1_BYTESWAP32 },	//  2
	{ "d51-21.59",	0x080000, 0x21ecde2b, TAITO_68KROM1_BYTESWAP32 },	//  3

	{ "d51-29.54",	0x040000, 0x4b64f41d, TAITO_68KROM2_BYTESWAP },		//  4 Sound 68k Code
	{ "d51-30.56",	0x040000, 0x45f339fe, TAITO_68KROM2_BYTESWAP },		//  5

	{ "d51-08.35",	0x200000, 0x835b7a0f, TAITO_CHARS_BYTESWAP },		//  6 TC0480SCP Tiles
	{ "d51-09.34",	0x200000, 0x6dabd83d, TAITO_CHARS_BYTESWAP },		//  7

	{ "d51-03.47",	0x200000, 0x629a5c99, TAITO_SPRITESA },				//  8 Sprites
	{ "d51-04.48",	0x200000, 0xf49b14b7, TAITO_SPRITESA_BYTESWAP32 },	//  9
	{ "d51-05.49",	0x200000, 0x3a2e2cbf, TAITO_SPRITESA_BYTESWAP32 },	// 10
	{ "d51-06.50",	0x200000, 0xd33ce2a0, TAITO_SPRITESA_BYTESWAP32 },	// 11
	{ "d51-07.51",	0x200000, 0x24b2f97d, TAITO_SPRITESA_BYTESWAP32 },	// 12

	{ "d51-10.95",	0x100000, 0xd5910604, TAITO_CHARS_PIVOT },			// 13 tc0100scn Tiles
	{ "d51-11.96",	0x100000, 0xfee5f5c6, TAITO_CHARS_PIVOT },			// 14
	{ "d51-12.97",	0x100000, 0xd630287b, TAITO_CHARS_PIVOT },			// 15

	{ "d51-13.7",	0x080000, 0x36921b8b, TAITO_SPRITEMAP },			// 16 Sprite Map

	{ "d51-01.73",	0x200000, 0x92f09155, TAITO_ES5505_BYTESWAP },		// 17 ENSONIQ Samples/Data
	{ "d51-02.74",	0x200000, 0x20a9428f, TAITO_ES5505_BYTESWAP },		// 18

	{ "93c46.164",	0x000080, 0x6f58851d, TAITO_DEFAULT_EEPROM },		// 19 Default EEPROM
};

STD_ROM_PICK(groundfx)
STD_ROM_FN(groundfx)

struct BurnDriver BurnDrvGroundfx = {
	"groundfx", NULL, NULL, NULL, "1992",
	"Ground Effects / Super Ground Effects (Japan)\0", NULL, "Taito Corporation", "K1100744A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_TAITO_MISC, GBF_RACING, 0,
	NULL, groundfxRomInfo, groundfxRomName, NULL, NULL, NULL, NULL, GroundfxInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	320, 232, 4, 3
};
