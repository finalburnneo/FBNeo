// FB Alpha Taito Gunbuster driver module
// Based on MAME driver by Bryan McPhail and David Graves

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "taitof3_snd.h"
#include "taito_ic.h"
#include "taito.h"
#include "eeprom.h"
#include "watchdog.h"
#include "burn_gun.h"

static UINT8 DrvRecalc;
static INT32 gun_interrupt_timer;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo GunbustrInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	TaitoInputPort3 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TaitoInputPort3 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	TaitoInputPort1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	TaitoInputPort1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	TaitoInputPort1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	TaitoInputPort1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	TaitoInputPort1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TaitoInputPort1 + 5,	"p1 fire 2"	},
	A("P1 Gun X",       BIT_ANALOG_REL, &TaitoAnalogPort0,      "mouse x-axis"),
	A("P1 Gun Y",       BIT_ANALOG_REL, &TaitoAnalogPort1,      "mouse y-axis"),

	{"P2 Coin",			BIT_DIGITAL,	TaitoInputPort3 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	TaitoInputPort3 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	TaitoInputPort2 + 0,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	TaitoInputPort2 + 1,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	TaitoInputPort2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	TaitoInputPort2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	TaitoInputPort1 + 6,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	TaitoInputPort1 + 7,	"p2 fire 2"	},
	A("P2 Gun X",       BIT_ANALOG_REL, &TaitoAnalogPort2,      "p2 x-axis"),
	A("P2 Gun Y",       BIT_ANALOG_REL, &TaitoAnalogPort3,      "p2 y-axis"),

	{"Freeze",			BIT_DIGITAL,	TaitoInputPort0 + 0,	"tilt"	    },
	{"Reset",			BIT_DIGITAL,	&TaitoReset,		    "reset"		},
	{"Service",			BIT_DIGITAL,	TaitoInputPort3 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	TaitoInputPort3 + 0,	"diag"	    },
};

STDINPUTINFO(Gunbustr)
#undef A

static UINT32 __fastcall gunbuster_read_long(UINT32 address)
{
	bprintf (0, _T("RL: %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall gunbuster_read_word(UINT32 address)
{
	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall gunbuster_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return TaitoInput[2];

		case 0x400002:
			return TaitoInput[1];

		case 0x400003:
			return TaitoInput[0] | (EEPROMRead() ? 0x80 : 0);

		case 0x400001:
		case 0x400004:
		case 0x400005:
		case 0x400006:
			return 0xff;

		case 0x400007:
			return TaitoInput[3];

		case 0x500000: return BurnGunReturnX(0);
		case 0x500001: return 0xff - BurnGunReturnY(0);
		case 0x500002: return BurnGunReturnX(1);
		case 0x500003: return 0xff - BurnGunReturnY(1);
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static void __fastcall gunbuster_write_long(UINT32 address, UINT32 data)
{
	bprintf (0, _T("WL: %5.5x, %8.8x\n"), address, data);
}

static void __fastcall gunbuster_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffc0) == 0x830000) {
		TC0480SCPCtrlWordWrite((address/2)&0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x380000:
			// motor control (not used in FBA)
		return;
	}

	bprintf (0, _T("WW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall gunbuster_write_byte(UINT32 address, UINT8 data)
{
	switch (address)
	{
		case 0x400000:
			BurnWatchdogWrite();
		return;

		case 0x400001:
		case 0x400002:
		return;

		case 0x400003:
			EEPROMSetClockLine((data & 0x20) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit(data & 0x40);
			EEPROMSetCSLine((data & 0x10) ? EEPROM_CLEAR_LINE : EEPROM_ASSERT_LINE);
		return;

		case 0x400004:
		case 0x400005:
		case 0x400006:
		case 0x400007:
			TC0510NIOHalfWordWrite(address & 7, data);
		return;

		case 0x500000:
		case 0x500001:
		case 0x500002:
		case 0x500003:
			gun_interrupt_timer = 10;
		return;
	}

	if (address != 0x400000) bprintf (0, _T("WB: %5.5x, %2.2x\n"), address, data);
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (TaitoRamStart, 0, TaitoRamEnd - TaitoRamStart);
	}

	SekReset(0);

	EEPROMReset();

	TaitoF3SoundReset();
	TaitoICReset();

	if (EEPROMAvailable() == 0) {
		EEPROMFill(TaitoDefaultEEProm, 0, 0x80);
	}

	gun_interrupt_timer = -1;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1		= Next; Next += 0x100000;

	Taito68KRom2		= Next;
	TaitoF3SoundRom		= Next; Next += 0x100000;

	TaitoChars			= Next; Next += 0x200000;
	TaitoSpritesA		= Next; Next += 0x800000;
	TaitoSpriteMapRom	= Next; Next += 0x080000;

	TaitoES5505Rom		= Next;
	TaitoF3ES5506Rom	= Next; Next += 0x800000;

	TaitoDefaultEEProm	= Next; Next += 0x0000800;

	TaitoPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);

	TaitoF2SpriteList	= (TaitoF2SpriteEntry*)Next; Next += 0x4000 * sizeof(TaitoF2SpriteEntry);

	TaitoRamStart		= Next;

	TaitoF3SoundRam		= Next; Next += 0x010000;	// 64 KB
	TaitoF3SharedRam	= Next; Next += 0x000800;	// 2 KB
	TaitoES5510DSPRam	= Next; Next += 0x000200;	// 512 Bytes
	TaitoES5510GPR		= (UINT32 *)Next; Next += 0x000300;	// 192x4 Bytes
	TaitoES5510DRAM		= (UINT16 *)Next; Next += 0x400000;	// 4 MB

	Taito68KRam1		= Next; Next += 0x020000;
	Taito68KRam2		= Next; Next += 0x004000;
	TaitoSpriteRam		= Next; Next += 0x002000;

	TaitoPaletteRam		= Next; Next += 0x002000;

	TaitoRamEnd			= Next;

	TaitoMemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane0[4]  = { STEP4(0,8) };
	static INT32 Plane1[4]  = { STEP4(0,1) };
	static INT32 XOffs0[16] = { STEP8(32,1), STEP8(0,1) };
	static INT32 XOffs1[16] = { 1*4, 0*4, 5*4, 4*4, 3*4, 2*4, 7*4, 6*4,
					9*4, 8*4, 13*4, 12*4, 11*4, 10*4, 15*4, 14*4 };
	static INT32 YOffs[16]  = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, TaitoSpritesA, 0x400000);

	GfxDecode(0x8000, 4, 16, 16, Plane0, XOffs0, YOffs, 0x400, tmp, TaitoSpritesA);

	memcpy (tmp, TaitoChars, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane1, XOffs1, YOffs, 0x400, tmp, TaitoChars);

	BurnFree (tmp);

	return 0;
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

		if (BurnLoadRom(TaitoF3SoundRom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3SoundRom + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(TaitoChars + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(TaitoChars + 0x000001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoSpritesA + 0x000003, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(TaitoSpritesA + 0x000000, k++, 4)) return 1;

		if (BurnLoadRom(TaitoSpriteMapRom + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoF3ES5506Rom + 0x000001, k  , 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x400001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x600001, k++, 2)) return 1;

		if (BurnLoadRom(TaitoDefaultEEProm + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	GenericTilesInit();
	TC0510NIOInit();
	TC0480SCPInit(0x2000, 0, 32, 8, -1, -1, 0);
	TC0480SCPSetPriMap(pPrioDraw);

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,		0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(TaitoSpriteRam,	0x300000, 0x301fff, MAP_RAM);
	SekMapMemory(TaitoF3SharedRam,	0x390000, 0x3907ff, MAP_RAM);
	SekMapMemory(TC0480SCPRam,		0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(TaitoPaletteRam,	0x900000, 0x901fff, MAP_RAM);
	SekMapMemory(Taito68KRam2,		0xc00000, 0xc03fff, MAP_RAM);
	SekSetWriteLongHandler(0,		gunbuster_write_long);
	SekSetWriteWordHandler(0,		gunbuster_write_word);
	SekSetWriteByteHandler(0,		gunbuster_write_byte);
	SekSetReadLongHandler(0,		gunbuster_read_long);
	SekSetReadWordHandler(0,		gunbuster_read_word);
	SekSetReadByteHandler(0,		gunbuster_read_byte);
	SekClose();

	EEPROMInit(&eeprom_interface_93C46);
	EEPROMIgnoreErrMessage(1); // game likes to spew crap at the eeprom

	BurnWatchdogInit(DrvDoReset, 180);

	TaitoF3SoundInit(1);
	TaitoF3ES5506RomSize = 0x800000;

	BurnGunInit(2, true);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();
	TaitoICExit();
	TaitoF3SoundExit();
	EEPROMExit();

	BurnGunExit();

	BurnFree(TaitoMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)TaitoPaletteRam;

	for (INT32 i = 0; i < 0x2000/2; i++)
	{
		UINT8 r = (p[i] >> 10) & 0x1f;
		UINT8 g = (p[i] >>  5) & 0x1f;
		UINT8 b = (p[i] >>  0) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		TaitoPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites(int x_offs,int y_offs)
{
	static const int primasks[4] = {0xfffc, 0xfff0, 0xff00, 0x0};

	UINT32 *spriteram32 = (UINT32*)TaitoSpriteRam, data;
	UINT16 *spritemap = (UINT16 *)TaitoSpriteMapRom;
	int offs, tilenum, color, flipx, flipy;
	int x, y, priority, dblsize, curx, cury;
	int sprites_flipscreen = 0;
	int zoomx, zoomy, zx, zy;
	int sprite_chunk,map_offset,code,j,k,px,py;
	int dimension,total_chunks;

	struct TaitoF2SpriteEntry *sprite_ptr = &TaitoF2SpriteList[0];

	for (offs = (0x2000/4-4);offs >= 0;offs -= 4)
	{
		data = spriteram32[offs+0];
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

		color |= 0x80;

		if (!tilenum) continue;

		flipy = !flipy;
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

			if (code==0xffff) {
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

			sprite_ptr->Code = code & 0x7fff;
			sprite_ptr->Colour = color*16;
			sprite_ptr->xFlip = !flipx;
			sprite_ptr->yFlip = flipy;
			sprite_ptr->x = curx;
			sprite_ptr->y = cury - 16;
			sprite_ptr->xZoom = zx << 12;
			sprite_ptr->yZoom = zy << 12;
			sprite_ptr->Priority = primasks[priority];
			sprite_ptr++;
		}
	}

	while (sprite_ptr != &TaitoF2SpriteList[0])
	{
		sprite_ptr--;

		RenderZoomedPrioSprite(pTransDraw, TaitoSpritesA,
			sprite_ptr->Code,
			sprite_ptr->Colour, 0,
			sprite_ptr->x, sprite_ptr->y,
			sprite_ptr->xFlip,sprite_ptr->yFlip,
			16, 16,
			sprite_ptr->xZoom,sprite_ptr->yZoom, sprite_ptr->Priority);
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
//	}

	UINT8 layer[5];
	UINT16 priority = TC0480SCPGetBgPriority();

	layer[0] = (priority & 0xf000) >> 12;
	layer[1] = (priority & 0x0f00) >>  8;
	layer[2] = (priority & 0x00f0) >>  4;
	layer[3] = (priority & 0x000f) >>  0;
	layer[4] = 4;   // text layer always on top

	BurnTransferClear(); // pTransDraw & pPrioDraw

	if (nBurnLayer & 1) TC0480SCPTilemapRenderPrio(layer[0], 1, 0, TaitoChars);
	if (nBurnLayer & 2) TC0480SCPTilemapRenderPrio(layer[1], 0, 1, TaitoChars);
	if (nBurnLayer & 4) TC0480SCPTilemapRenderPrio(layer[2], 0, 2, TaitoChars);
	if (nBurnLayer & 8) TC0480SCPTilemapRenderPrio(layer[3], 0, 4, TaitoChars);

	draw_sprites(48,-116);

	if (nSpriteEnable & 1) TC0480SCPRenderCharLayer();

	// flip horizontally-FBA's video blitter's dont support this :(
	{
		for (INT32 y = 0; y < nScreenHeight; y++) {
			UINT16 *pos0 = pTransDraw + y * nScreenWidth;
			UINT16 *pos1 = pos0 + (nScreenWidth - 1);
			for (INT32 x = 0; x < nScreenWidth/2; x++) {
				INT32 tmp = pos0[x];
				pos0[x] = pos1[-x];
				pos1[-x] = tmp;
			}
		}
	}

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
		TaitoInput[0] = 0x7e;
		TaitoInput[1] = 0xff;
		TaitoInput[2] = 0xff;
		TaitoInput[3] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			TaitoInput[0] ^= (TaitoInputPort0[i] & 1) << i;
			TaitoInput[1] ^= (TaitoInputPort1[i] & 1) << i;
			TaitoInput[2] ^= (TaitoInputPort2[i] & 1) << i;
			TaitoInput[3] ^= (TaitoInputPort3[i] & 1) << i;
		}

		BurnGunMakeInputs(0, TaitoAnalogPort0, TaitoAnalogPort1);
		BurnGunMakeInputs(1, TaitoAnalogPort2, TaitoAnalogPort3);

	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 16000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		SekOpen(0);
		CPU_RUN(0, Sek);

		if (i == 255)
		{
			SekSetIRQLine(4, CPU_IRQSTATUS_AUTO);

			gun_interrupt_timer = 20;
		}
		else if (gun_interrupt_timer >= 0)
		{
			if (gun_interrupt_timer == 0) {
				SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
			}
			gun_interrupt_timer--;
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

	if (pnMin != NULL) {
		*pnMin = 0x029740;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = TaitoRamStart;
		ba.nLen	  = TaitoRamEnd-TaitoRamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	TaitoICScan(nAction);

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		TaitoF3SoundScan(nAction, pnMin);
		TaitoICScan(nAction);
		EEPROMScan(nAction, pnMin);

		BurnGunScan();

		SCAN_VAR(gun_interrupt_timer);
	}

	return 0;
}


// Gunbuster (World)

static struct BurnRomInfo gunbustrRomDesc[] = {
	{ "d27-23.bin",				0x040000, 0xcd1037cc, TAITO_68KROM1_BYTESWAP32 },  //  0 Main 68K Code
	{ "d27-22.bin",				0x040000, 0x475949fc, TAITO_68KROM1_BYTESWAP32 },  //  1
	{ "d27-21.bin",				0x040000, 0x60950a8a, TAITO_68KROM1_BYTESWAP32 },  //  2
	{ "d27-27.bin",				0x040000, 0xfd7d3d4c, TAITO_68KROM1_BYTESWAP32 },  //  3

	{ "d27-25.bin",				0x020000, 0xc88203cf, TAITO_68KROM2_BYTESWAP },    //  4 Sound 68K Code
	{ "d27-24.bin",				0x020000, 0x084bd8bd, TAITO_68KROM2_BYTESWAP },    //  5

	{ "d27-01.bin",				0x080000, 0xf41759ce, TAITO_CHARS },               //  6 Background Tiles
	{ "d27-02.bin",				0x080000, 0x92ab6430, TAITO_CHARS },               //  7

	{ "d27-04.bin",				0x100000, 0xff8b9234, TAITO_SPRITESA_BYTESWAP32 }, //  8 Sprites
	{ "d27-05.bin",				0x100000, 0x96d7c1a5, TAITO_SPRITESA_BYTESWAP32 }, //  9
	{ "d27-06.bin",				0x100000, 0xbbb934db, TAITO_SPRITESA_BYTESWAP32 }, // 10
	{ "d27-07.bin",				0x100000, 0x8ab4854e, TAITO_SPRITESA_BYTESWAP32 }, // 11

	{ "d27-03.bin",				0x080000, 0x23bf2000, TAITO_SPRITEMAP },           // 12 Sprite Map

	{ "d27-08.bin",				0x100000, 0x7c147e30, TAITO_ES5505_BYTESWAP },     // 13 Ensoniq Sample Data
	{ "d27-09.bin",				0x100000, 0x3e060304, TAITO_ES5505_BYTESWAP },     // 14
	{ "d27-10.bin",				0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },     // 15

	{ "eeprom-gunbustr.bin",	0x000080, 0xef3685a1, TAITO_DEFAULT_EEPROM },      // 16 Default EEPROM
};

STD_ROM_PICK(gunbustr)
STD_ROM_FN(gunbustr)

struct BurnDriver BurnDrvGunbustr = {
	"gunbustr", NULL, NULL, NULL, "1992",
	"Gunbuster (World)\0", NULL, "Taito Corporation Japan", "K11J0717A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, gunbustrRomInfo, gunbustrRomName, NULL, NULL, NULL, NULL, GunbustrInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 240, 4, 3
};


// Gunbuster (US)

static struct BurnRomInfo gunbustruRomDesc[] = {
	{ "d27-23.bin",				0x040000, 0xcd1037cc, TAITO_68KROM1_BYTESWAP32 },  //  0 Main 68K Code
	{ "d27-22.bin",				0x040000, 0x475949fc, TAITO_68KROM1_BYTESWAP32 },  //  1
	{ "d27-21.bin",				0x040000, 0x60950a8a, TAITO_68KROM1_BYTESWAP32 },  //  2
	{ "d27-26.bin",				0x040000, 0x8a7a0dda, TAITO_68KROM1_BYTESWAP32 },  //  3

	{ "d27-25.bin",				0x020000, 0xc88203cf, TAITO_68KROM2_BYTESWAP },    //  4 Sound 68K Code
	{ "d27-24.bin",				0x020000, 0x084bd8bd, TAITO_68KROM2_BYTESWAP },    //  5

	{ "d27-01.bin",				0x080000, 0xf41759ce, TAITO_CHARS },               //  6 Background Tiles
	{ "d27-02.bin",				0x080000, 0x92ab6430, TAITO_CHARS },               //  7

	{ "d27-04.bin",				0x100000, 0xff8b9234, TAITO_SPRITESA_BYTESWAP32 }, //  8 Sprites
	{ "d27-05.bin",				0x100000, 0x96d7c1a5, TAITO_SPRITESA_BYTESWAP32 }, //  9
	{ "d27-06.bin",				0x100000, 0xbbb934db, TAITO_SPRITESA_BYTESWAP32 }, // 10
	{ "d27-07.bin",				0x100000, 0x8ab4854e, TAITO_SPRITESA_BYTESWAP32 }, // 11

	{ "d27-03.bin",				0x080000, 0x23bf2000, TAITO_SPRITEMAP },           // 12 Sprite Map

	{ "d27-08.bin",				0x100000, 0x7c147e30, TAITO_ES5505_BYTESWAP },     // 13 Ensoniq Sample Data
	{ "d27-09.bin",				0x100000, 0x3e060304, TAITO_ES5505_BYTESWAP },     // 14
	{ "d27-10.bin",				0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },     // 15

	{ "eeprom-gunbustr.bin",	0x000080, 0xef3685a1, TAITO_DEFAULT_EEPROM },      // 16 Default EEPROM
};

STD_ROM_PICK(gunbustru)
STD_ROM_FN(gunbustru)

struct BurnDriver BurnDrvGunbustru = {
	"gunbustru", "gunbustr", NULL, NULL, "1992",
	"Gunbuster (US)\0", NULL, "Taito America Corporation", "K11J0717A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, gunbustruRomInfo, gunbustruRomName, NULL, NULL, NULL, NULL, GunbustrInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 240, 4, 3
};


// Gunbuster (Japan)

static struct BurnRomInfo gunbustrjRomDesc[] = {
	{ "d27-23.bin",				0x040000, 0xcd1037cc, TAITO_68KROM1_BYTESWAP32 },  //  0 Main 68K Code
	{ "d27-22.bin",				0x040000, 0x475949fc, TAITO_68KROM1_BYTESWAP32 },  //  1
	{ "d27-21.bin",				0x040000, 0x60950a8a, TAITO_68KROM1_BYTESWAP32 },  //  2
	{ "d27-20.bin",				0x040000, 0x13735c60, TAITO_68KROM1_BYTESWAP32 },  //  3

	{ "d27-25.bin",				0x020000, 0xc88203cf, TAITO_68KROM2_BYTESWAP },    //  4 Sound 68K Code
	{ "d27-24.bin",				0x020000, 0x084bd8bd, TAITO_68KROM2_BYTESWAP },    //  5

	{ "d27-01.bin",				0x080000, 0xf41759ce, TAITO_CHARS },               //  6 Background Tiles
	{ "d27-02.bin",				0x080000, 0x92ab6430, TAITO_CHARS },               //  7

	{ "d27-04.bin",				0x100000, 0xff8b9234, TAITO_SPRITESA_BYTESWAP32 }, //  8 Sprites
	{ "d27-05.bin",				0x100000, 0x96d7c1a5, TAITO_SPRITESA_BYTESWAP32 }, //  9
	{ "d27-06.bin",				0x100000, 0xbbb934db, TAITO_SPRITESA_BYTESWAP32 }, // 10
	{ "d27-07.bin",				0x100000, 0x8ab4854e, TAITO_SPRITESA_BYTESWAP32 }, // 11

	{ "d27-03.bin",				0x080000, 0x23bf2000, TAITO_SPRITEMAP },           // 12 Sprite Map

	{ "d27-08.bin",				0x100000, 0x7c147e30, TAITO_ES5505_BYTESWAP },     // 13 Ensoniq Sample Data
	{ "d27-09.bin",				0x100000, 0x3e060304, TAITO_ES5505_BYTESWAP },     // 14
	{ "d27-10.bin",				0x100000, 0xed894fe1, TAITO_ES5505_BYTESWAP },     // 15

	{ "eeprom-gunbustr.bin",	0x000080, 0xef3685a1, TAITO_DEFAULT_EEPROM },      // 16 Default EEPROM
};

STD_ROM_PICK(gunbustrj)
STD_ROM_FN(gunbustrj)

struct BurnDriver BurnDrvGunbustrj = {
	"gunbustrj", "gunbustr", NULL, NULL, "1992",
	"Gunbuster (Japan)\0", NULL, "Taito Corporation", "K11J0717A",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, gunbustrjRomInfo, gunbustrjRomName, NULL, NULL, NULL, NULL, GunbustrInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 240, 4, 3
};
