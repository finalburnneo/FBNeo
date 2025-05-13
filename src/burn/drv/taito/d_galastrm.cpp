// FinalBurn Neo Taito Galactic Storm driver module
// Based on MAME driver by Hau

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "taito.h"
#include "taito_ic.h"
#include "taitof3_snd.h"
#include "eeprom.h"
#include "watchdog.h"
#include "poly.h"

static INT32 coin_word;
static INT16 *tc0610_reg[2];
static INT16 *tc0610_addr;

static poly_manager *poly = NULL;

struct poly_extra_data
{
	UINT16 *texbase;
};

static INT32 sprite_count = 0;
static INT32 do_adcirq = 0;
static INT32 scanline = 0;

static INT32 rsyb = 0;
static INT32 rsxb = 0;
static INT32 rsxoffs = 0;
static INT32 rsyoffs = 0;

static UINT8 DrvRecalc;

#define A(a, b, c, d) { a, b, (UINT8*)(c), d }
static struct BurnInputInfo GalastrmInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	TaitoInputPort1 + 2,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	TaitoInputPort1 + 3,	"p2 coin"	},
	{"P1 Start",		BIT_DIGITAL,	TaitoInputPort0 + 0,	"p1 start"	},
	{"P1 Button 1",		BIT_DIGITAL,	TaitoInputPort1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	TaitoInputPort1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	TaitoInputPort1 + 6,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	TaitoInputPort1 + 7,	"p1 fire 4"	},

	A("P1 Stick X",		BIT_ANALOG_REL, &TaitoAnalogPort0,      "p1 x-axis"),
	A("P1 Stick Y",		BIT_ANALOG_REL, &TaitoAnalogPort1,      "p1 y-axis"),

	{"Reset",			BIT_DIGITAL,	&TaitoReset,			"reset"		},
	{"Service",			BIT_DIGITAL,	TaitoInputPort1 + 1,	"service"	},
	{"Service Mode",	BIT_DIGITAL,	TaitoInputPort1 + 0,	"diag"		},
};

STDINPUTINFO(Galastrm)
#undef A

static void __fastcall galastrm_main_write_long(UINT32 a, UINT32 d)
{
	TC0100SCN0LongWrite_Map(0xd00000, 0xd0ffff)
}

static void __fastcall galastrm_main_write_word(UINT32 a, UINT16 d)
{
	TC0100SCN0WordWrite_Map(0xd00000, 0xd0ffff)

	if ((a & 0xffffc0) == 0x830000) {
		TC0480SCPCtrlWordWrite((a / 2) & 0x1f, d);
		return;
	}

	if ((a & 0xfffff0) == 0xd20000) {
		TC0100SCNCtrlWordWrite(0, (a / 2) & 7, d);
		return;
	}

	switch (a)
	{
		case 0x40fff0:
		return; // nop

		case 0x900000:
		case 0x900002:
			TC0110PCRStep1RBSwapWordWrite(0, (a/2) & 1, d);
		return;

		case 0xb00000:
			tc0610_addr[0] = d;
		return;

		case 0xb00002:
			if (tc0610_addr[0] < 8) tc0610_reg[0][tc0610_addr[0]] = d;
		return;

		case 0xc00000:
			tc0610_addr[1] = d;
		return;

		case 0xc00002:
			if (tc0610_addr[1] < 8) tc0610_reg[1][tc0610_addr[1]] = d;
		return;
	}
}

static void __fastcall galastrm_main_write_byte(UINT32 a, UINT8 d)
{
	TC0100SCN0ByteWrite_Map(0xd00000, 0xd0ffff)

	switch (a)
	{
		case 0x400000:
			BurnWatchdogWrite();
		return;

		case 0x400001:
		case 0x400002:
		return;

		case 0x400003:
			EEPROMSetCSLine((~d & 0x10) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMSetClockLine((d & 0x20) ? EEPROM_ASSERT_LINE : EEPROM_CLEAR_LINE);
			EEPROMWriteBit((d >> 6) & 1);
		return;

		case 0x400004:
			// coin counter / lockout
		return;

		case 0x400005:
		case 0x400006:
		case 0x400007:
		return;

		case 0x500000:
		case 0x500001:
		case 0x500002:
		case 0x500003:
		case 0x500004:
		case 0x500005:
		case 0x500006:
		case 0x500007:
			do_adcirq = scanline+1;
		return;
	}
}

static UINT32 __fastcall galastrm_main_read_long(UINT32 address)
{
	bprintf (0, _T("RL: %5.5x\n"), address);

	return 0;
}

static UINT16 __fastcall galastrm_main_read_word(UINT32 address)
{
	if ((address & 0xffffc0) == 0x830000) {
		return TC0480SCPCtrl[(address / 2) & 0x1f];
	}

	if ((address & 0xfffff0) == 0x920000) {
		return TC0100SCNCtrl[0][(address / 2) & 7];
	}

	switch (address)
	{
		case 0x900002:
			return TC0110PCRWordRead(0);
	}

	return 0;
}

static UINT8 __fastcall galastrm_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x400000:
			return 0xff;
		case 0x400001:
			return 0xff;

		case 0x400002:
			return TaitoInput[0] | ((nCurrentFrame & 1)*2); // in0

		case 0x400003:
			return (EEPROMRead() ? 0x80 : 0) | 0x7e; // or 0x7f??

		case 0x400004:
			return 0xff;

		case 0x400005:
			return 0xff;

		case 0x400006:
			return 0xff;

		case 0x400007:
			return TaitoInput[1]; // system

		case 0x500000:
			return ProcessAnalog(TaitoAnalogPort0, 0, INPUT_DEADZONE, 0x00, 0xff);
		case 0x500001:
			return ProcessAnalog(TaitoAnalogPort1, 1, INPUT_DEADZONE, 0x00, 0xff);
		case 0x500002:
		case 0x500003:
		case 0x500004:
		case 0x500005:
		case 0x500006:
		case 0x500007:
			return 0; // analog
	}

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

	coin_word = 0;
	do_adcirq = -1;

	sprite_count = 0;
	scanline = 0;

	rsyb = 0;
	rsxb = 0;
	rsxoffs = 0;
	rsyoffs = 0;

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = TaitoMem;

	Taito68KRom1		= Next; Next += 0x100000;
	Taito68KRom2		= Next;
	TaitoF3SoundRom		= Next; Next += 0x100000;

	TaitoChars			= Next; Next += 0x400000;

	TaitoSpritesA		= Next; Next += 0x800000;

	TaitoSpriteMapRom	= Next; Next += 0x080000;

	TaitoDefaultEEProm	= Next; Next += 0x0000080;

	TaitoES5505Rom		= Next;
	TaitoF3ES5506Rom	= Next; Next += 0x1000000;

	TaitoF2SpriteList	= (TaitoF2SpriteEntry*)Next; Next += 0x4000 * sizeof(TaitoF2SpriteEntry);

	TaitoRamStart		= Next;

	TaitoSpriteRam		= Next; Next += 0x004000;
	Taito68KRam1		= Next; Next += 0x020000;
	TaitoPaletteRam		= Next; Next += 0x010000;

	TaitoF3SoundRam		= Next; Next += 0x010000;	// 64 KB
	TaitoF3SharedRam	= Next; Next += 0x000800;	// 2 KB
	TaitoES5510DSPRam	= Next; Next += 0x000200;	// 512 Bytes
	TaitoES5510GPR		= (UINT32 *)Next; Next += 0x000300;	// 192x4 Bytes
	TaitoES5510DRAM		= (UINT16 *)Next; Next += 0x400000;	// 4 MB

	tc0610_reg[0]		= (INT16 *)Next; Next += 0x000008 * sizeof(INT16);
	tc0610_reg[1]		= (INT16 *)Next; Next += 0x000008 * sizeof(INT16);
	tc0610_addr			= (INT16 *)Next; Next += 0x000002 * sizeof(INT16);

	TaitoRamEnd			= Next;

	TaitoMemEnd			= Next;

	return 0;
}

static INT32 DrvSpriteDecode()
{
	INT32 Plane[4]  = { STEP4(0,16) };
	INT32 XOffs[16] = { STEP16(0,1) };
	INT32 YOffs[16] = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x400000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, TaitoSpritesA, 0x400000);

	GfxDecode(0x8000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, TaitoSpritesA);

	BurnFree(tmp);

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

		if (BurnLoadRom(Taito68KRom2 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Taito68KRom2 + 0x000000, k++, 2)) return 1;

		if (BurnLoadRomExt(TaitoChars + 0x000000, k++, 4, LD_GROUP(2))) return 1;
		if (BurnLoadRomExt(TaitoChars + 0x000002, k++, 4, LD_GROUP(2))) return 1;

		if (BurnLoadRomExt(TaitoSpritesA + 0x000000, k++, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;
		if (BurnLoadRomExt(TaitoSpritesA + 0x000002, k++, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;
		if (BurnLoadRomExt(TaitoSpritesA + 0x000004, k++, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;
		if (BurnLoadRomExt(TaitoSpritesA + 0x000006, k++, 8, LD_GROUP(2) | LD_BYTESWAP)) return 1;

		if (BurnLoadRom(TaitoSpriteMapRom + 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(TaitoF3ES5506Rom + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x200001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x400001, k++, 2)) return 1;
		if (BurnLoadRom(TaitoF3ES5506Rom + 0x800001, k++, 2)) return 1;
		memcpy (TaitoF3ES5506Rom + 0x680000, TaitoF3ES5506Rom + 0x800000, 0x080000);
		memcpy (TaitoF3ES5506Rom + 0x600000, TaitoF3ES5506Rom + 0x880000, 0x080000);
		memcpy (TaitoF3ES5506Rom + 0x780000, TaitoF3ES5506Rom + 0x900000, 0x080000);
		memcpy (TaitoF3ES5506Rom + 0x700000, TaitoF3ES5506Rom + 0x980000, 0x080000);
		memset (TaitoF3ES5506Rom + 0x800000, 0, 0x200000);

		if (BurnLoadRom(TaitoDefaultEEProm  + 0x000000, k++, 1)) return 1;

		BurnNibbleExpand(TaitoChars, NULL, 0x200000, 1, 0);
		DrvSpriteDecode();
	}

	GenericTilesInit();

	nScreenWidth = 512;
	nScreenHeight = 400;
	BurnBitmapAllocate(1, 512, 400, true); // tmp
	BurnBitmapAllocate(2, 512, 400, true); // poly
	BurnBitmapAllocate(3, 512, 400, true); // tmp2

	poly = poly_alloc(16, sizeof(poly_extra_data), POLYFLAG_ALLOW_QUADS);

	TC0100SCNInit(0, 0x10000, -48, -56-8, 0, BurnBitmapGetPriomap(3));
	TC0100SCNSetColourDepth(0, 4);
	TC0100SCNSetClipArea(0, 512, 400, 0);
	TC0480SCPInit(0x4000, 0, -40, -3+7, 0, 0, 0);
	TC0110PCRInit(1, 0x1000);

	SekInit(0, 0x68ec020);
	SekOpen(0);
	SekMapMemory(Taito68KRom1,			0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Taito68KRam1,			0x200000, 0x21ffff, MAP_RAM);
	SekMapMemory(TaitoSpriteRam,		0x300000, 0x303fff, MAP_RAM);
	SekMapMemory(TaitoF3SharedRam,		0x600000, 0x6007ff, MAP_RAM);
	SekMapMemory(TC0480SCPRam,			0x800000, 0x80ffff, MAP_RAM);
	SekMapMemory(TC0100SCNRam[0],		0xd00000, 0xd0ffff, MAP_READ);
	SekSetWriteLongHandler(0,			galastrm_main_write_long);
	SekSetWriteWordHandler(0,			galastrm_main_write_word);
	SekSetWriteByteHandler(0,			galastrm_main_write_byte);
	SekSetReadLongHandler(0,			galastrm_main_read_long);
	SekSetReadWordHandler(0,			galastrm_main_read_word);
	SekSetReadByteHandler(0,			galastrm_main_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	TaitoF3ES5506RomSize = 0x1000000;
	TaitoF3SoundInit(1);
	TaitoF3SoundIRQConfig(1);

	EEPROMInit(&eeprom_interface_93C46);
	EEPROMIgnoreErrMessage(1);

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

	poly_free(poly);

	BurnFree(TaitoMem);

	return 0;
}

static void draw_sprites_pre(INT32 x_offs, INT32 y_offs)
{
	UINT32 data;
	INT32 offs, tilenum, color, flipx, flipy;
	INT32 x, y, priority, dblsize, curx, cury;
	INT32 sprites_flipscreen = 0;
	INT32 zoomx, zoomy, zx, zy;
	INT32 sprite_chunk,map_offset,code,j,k,px,py;
	INT32 dimension,total_chunks;

	UINT32 *spriteram = (UINT32*)TaitoSpriteRam;
	UINT16 *spritemap = (UINT16 *)TaitoSpriteMapRom;

	sprite_count = 0;
	TaitoF2SpriteEntry *sprite_ptr_pre = &TaitoF2SpriteList[sprite_count];

	for (offs = (0x4000/4-4);offs >= 0;offs -= 4)
	{
		data = spriteram[offs+0];
		data = (data << 16) | (data >> 16);
		flipx =    (data & 0x00800000) >> 23;
		zoomx =    (data & 0x007f0000) >> 16;
		tilenum =  (data & 0x00007fff);

		if (!tilenum) continue;

		data = spriteram[offs+2];
		data = (data << 16) | (data >> 16);
		priority = (data & 0x000c0000) >> 18;
		color =    (data & 0x0003fc00) >> 10;
		x =        (data & 0x000003ff);

		data = spriteram[offs+3];
		data = (data << 16) | (data >> 16);
		dblsize =  (data & 0x00040000) >> 18;
		flipy =    (data & 0x00020000) >> 17;
		zoomy =    (data & 0x0001fc00) >> 10;
		y =        (data & 0x000003ff);

		dimension = ((dblsize*2) + 2);  // 2 or 4
		total_chunks = ((dblsize*3) + 1) << 2;  // 4 or 16
		map_offset = tilenum << 2;

		zoomx += 1;
		zoomy += 1;

		if (x > 713) x -= 1024;     // 1024x512
		if (y < 117) y += 512;

		y = (-y & 0x3ff);
		x -= x_offs;
		y += y_offs;
		if (flipy) y += (128 - zoomy);

		for (sprite_chunk=0;sprite_chunk<total_chunks;sprite_chunk++)
		{
			j = sprite_chunk / dimension;
			k = sprite_chunk % dimension;

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

			sprite_ptr_pre->Code = code & 0x7fff;
			sprite_ptr_pre->Colour = color << 4;
			sprite_ptr_pre->xFlip = !flipx;
			sprite_ptr_pre->yFlip = flipy;
			sprite_ptr_pre->x = curx;
			sprite_ptr_pre->y = cury;
			sprite_ptr_pre->xZoom = zx << 12;
			sprite_ptr_pre->yZoom = zy << 12;
			sprite_ptr_pre->Priority = priority;
			sprite_ptr_pre++;
			sprite_count++;
		}
	}
}

static void draw_sprites(INT32 priority)
{
	struct TaitoF2SpriteEntry *sprite_ptr = &TaitoF2SpriteList[sprite_count];

	static const INT32 primasks[4] = {0xfffc, 0xfff0, 0xff00, 0x0};

	while (sprite_ptr != &TaitoF2SpriteList[0])
	{
		sprite_ptr--;

		if ((priority != 0 && sprite_ptr->Priority != 0) ||	(priority == 0 && sprite_ptr->Priority == 0))
		{
			RenderZoomedPrioSprite(pTransDraw, TaitoSpritesA,
				sprite_ptr->Code,
				sprite_ptr->Colour, 0,
				sprite_ptr->x, sprite_ptr->y,
				sprite_ptr->xFlip,sprite_ptr->yFlip,
				16, 16,
				sprite_ptr->xZoom,sprite_ptr->yZoom, primasks[sprite_ptr->Priority]);
		}
	}
}

static void tc0610_draw_scanline(void *dest, INT32 scan_line, const poly_extent *extent, const void *extradata, INT32 threadid)
{
	const poly_extra_data *extra = (const poly_extra_data *)extradata;
	UINT16 *framebuffer = ((UINT16*)dest) + scan_line * nScreenWidth;
	UINT16 *texbase = extra->texbase;
	INT32 startx = extent->startx;
	INT32 stopx = extent->stopx;
	INT32 u = extent->param[0].start;
	INT32 v = extent->param[1].start;
	INT32 dudx = extent->param[0].dpdx;
	INT32 dvdx = extent->param[1].dpdx;

	for (INT32 x = startx; x < stopx; x++)
	{
		INT32 srcy = (v >> 16);
		INT32 srcx = (u >> 16);

		if (x >= 0 && x < nScreenWidth) {
			if (srcy >= 0 && srcy < nScreenHeight && srcx >= 0 && srcx < nScreenWidth) {
				framebuffer[x] = texbase[srcy * nScreenWidth + srcx];
			}
		}
		u += dudx;
		v += dvdx;
	}
}

static void tc0610_rotate_draw()
{
	struct rectangle clip;
	clip.set(0, nScreenWidth-1, 0, nScreenHeight-1);

	struct polygon
	{
		float x;
		float y;
		float z;
	};

	poly_extra_data *extra = (poly_extra_data *)poly_get_extra_data(poly);
	poly_draw_scanline_func callback;
	poly_vertex vert[4];
	INT32 rsx = tc0610_reg[1][0];
	INT32 rsy = tc0610_reg[1][1];
	const INT32 rzx = tc0610_reg[1][2];
	const INT32 rzy = tc0610_reg[1][3];
	const INT32 ryx = tc0610_reg[1][5];
	const INT32 ryy = tc0610_reg[1][4];
	const INT32 lx  = nScreenWidth;
	const INT32 ly  = nScreenHeight;

	INT32 yx, /*yy,*/ zx, zy, pxx, pxy, pyx, pyy;
	float /*ssn, scs, ysn, ycs,*/ zsn, zcs;

	pxx = 0;
	pxy = 0;
	pyx = 0;
	pyy = 0;
	zx  = 0;
	zy  = 0;

	if (rzx != 0 || rzy != 0)
	{
		while (sqrt(pow((float)pxx/4096.0, 2.0) + pow((float)pxy/4096.0, 2.0)) < (float)(lx / 2))
		{
			pxx += rzx;
			pxy += rzy;
			zx++;
		}
		while (sqrt(pow((float)pyy/4096.0, 2.0) + pow((float)pyx/4096.0, 2.0)) < (float)(ly / 2))
		{
			pyy += rzx;
			pyx += -rzy;
			zy++;
		}
	}
	zsn = ((float)pyx/4096.0) / (float)(ly / 2);
	zcs = ((float)pxx/4096.0) / (float)(lx / 2);


	if ((rsx == -240 && rsy == 1072) || !tc0610_reg[1][7])
	{
		rsxoffs = 0;
		rsyoffs = 0;
	}
	else
	{
		if (rsx > rsxb && rsxb < 0 && rsx-rsxb > 0x8000)
		{
			if (rsxoffs == 0)
				rsxoffs = -0x10000;
			else
				rsxoffs = 0;
		}
		if (rsx < rsxb && rsxb > 0 && rsxb-rsx > 0x8000)
		{
			if (rsxoffs == 0)
				rsxoffs = 0x10000-1;
			else
				rsxoffs = 0;
		}
		if (rsy > rsyb && rsyb < 0 && rsy-rsyb > 0x8000)
		{
			if (rsyoffs == 0)
				rsyoffs = -0x10000;
			else
				rsyoffs = 0;
		}
		if (rsy < rsyb && rsyb > 0 && rsyb-rsy > 0x8000)
		{
			if (rsyoffs == 0)
				rsyoffs = 0x10000-1;
			else
				rsyoffs = 0;
		}
	}
	rsxb = rsx;
	rsyb = rsy;
	if (rsxoffs) rsx += rsxoffs;
	if (rsyoffs) rsy += rsyoffs;
	if (rsx < -0x14000 || rsx >= 0x14000) rsxoffs = 0;
	if (rsy < -0x14000 || rsy >= 0x14000) rsyoffs = 0;

	pxx = 0;
	pxy = 0;
	pyx = 0;
	pyy = 0;
	yx  = 0;

	if (tc0610_reg[1][7])
	{
		if (ryx != 0 || ryy != 0)
		{
			while (sqrt(pow((float)pxx/4096.0, 2.0) + pow((float)pxy/4096.0, 2.0)) < (float)(lx / 2))
			{
				pxx += ryx;
				pxy += ryy;
				yx++;
			}
			while (sqrt(pow((float)pyy/4096.0, 2.0) + pow((float)pyx/4096.0, 2.0)) < (float)(ly / 2))
			{
				pyy += ryx;
				pyx += -ryy;
			}
			if (yx >= 0.0)
			{
				yx = (int)((8.0 - log((double)yx) / log(2.0)) * 6.0);
			}
		}

		pxx = 0;
		pxy = 0;
		pyx = 0;
		pyy = 0;

		if (rsx != 0 || rsy != 0)
		{
			while (sqrt(pow((float)pxx/65536.0, 2.0) + pow((float)pxy/65536.0, 2.0)) < (float)(lx / 2))
			{
				pxx += rsx;
				pxy += rsy;
			}
			while (sqrt(pow((float)pyy/65536.0, 2.0) + pow((float)pyx/65536.0, 2.0)) < (float)(ly / 2))
			{
				pyy += rsx;
				pyx += -rsy;
			}
		}
	}


	{
		polygon tmpz[4];

		tmpz[0].x = ((float)(-zx)  * zcs) - ((float)(-zy)  * zsn);
		tmpz[0].y = ((float)(-zx)  * zsn) + ((float)(-zy)  * zcs);
		tmpz[0].z = 0.0;
		tmpz[1].x = ((float)(-zx)  * zcs) - ((float)(zy-1) * zsn);
		tmpz[1].y = ((float)(-zx)  * zsn) + ((float)(zy-1) * zcs);
		tmpz[1].z = 0.0;
		tmpz[2].x = ((float)(zx-1) * zcs) - ((float)(zy-1) * zsn);
		tmpz[2].y = ((float)(zx-1) * zsn) + ((float)(zy-1) * zcs);
		tmpz[2].z = 0.0;
		tmpz[3].x = ((float)(zx-1) * zcs) - ((float)(-zy)  * zsn);
		tmpz[3].y = ((float)(zx-1) * zsn) + ((float)(-zy)  * zcs);
		tmpz[3].z = 0.0;


		vert[0].x = tmpz[0].x + (float)(lx / 2);
		vert[0].y = tmpz[0].y + (float)(ly / 2);
		vert[1].x = tmpz[1].x + (float)(lx / 2);
		vert[1].y = tmpz[1].y + (float)(ly / 2);
		vert[2].x = tmpz[2].x + (float)(lx / 2);
		vert[2].y = tmpz[2].y + (float)(ly / 2);
		vert[3].x = tmpz[3].x + (float)(lx / 2);
		vert[3].y = tmpz[3].y + (float)(ly / 2);
	}

	vert[0].p[0] = 0.0;
	vert[0].p[1] = 0.0;
	vert[1].p[0] = 0.0;
	vert[1].p[1] = (float)(ly - 1) * 65536.0;
	vert[2].p[0] = (float)(lx - 1) * 65536.0;
	vert[2].p[1] = (float)(ly - 1) * 65536.0;
	vert[3].p[0] = (float)(lx - 1) * 65536.0;
	vert[3].p[1] = 0.0;

	extra->texbase = BurnBitmapGetBitmap(1);
	callback = tc0610_draw_scanline;

	poly_render_quad(poly, (void*)BurnBitmapGetBitmap(2), clip, callback, 2, &vert[0], &vert[1], &vert[2], &vert[3]);
	poly_wait(poly, NULL);
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		TC0110PCRRecalcPaletteStep1RBSwap();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	UINT8 Layer[5];
	UINT16 priority = TC0480SCPGetBgPriority();

	Layer[0] = (priority & 0xf000) >> 12;
	Layer[1] = (priority & 0x0f00) >>  8;
	Layer[2] = (priority & 0x00f0) >>  4;
	Layer[3] = (priority & 0x000f) >>  0;
	Layer[4] = 4;

	INT32 Disable = TC0100SCNCtrl[0][6] & 0x3;

	nScreenWidth = 512;
	nScreenHeight = 400;
	GenericTilesSetClip(0, nScreenWidth, 0, nScreenHeight);

	pTransDraw = BurnBitmapGetBitmap(3); // tmp2
	pPrioDraw = BurnBitmapGetPriomap(3);

	BurnBitmapFill(3, 0);
	BurnBitmapPrimapClear(3);

	if (TC0100SCNBottomLayer(0)) {
		if (nSpriteEnable & 8) if (!(Disable & 0x02)) TC0100SCNRenderFgLayer(0, 1, TaitoChars);
		if (nSpriteEnable & 4) if (!(Disable & 0x01)) TC0100SCNRenderBgLayer(0, 0, TaitoChars);
	} else {
		if (nSpriteEnable & 4) if (!(Disable & 0x01)) TC0100SCNRenderBgLayer(0, 1, TaitoChars);
		if (nSpriteEnable & 8) if (!(Disable & 0x02)) TC0100SCNRenderFgLayer(0, 0, TaitoChars);
	}

	pTransDraw = BurnBitmapGetBitmap(1); // tmp
	pPrioDraw = BurnBitmapGetPriomap(1);
	BurnBitmapFill(1, 0);
	BurnBitmapPrimapClear(1);

	TC0480SCPSetPriMap(pPrioDraw);

	if (Layer[0] == 0 && Layer[1] == 3 && Layer[2] == 2 && Layer[3] == 1)
	{
		if (nBurnLayer & 1) TC0480SCPTilemapRenderPrio(Layer[0], 0, 1, TaitoChars);
		if (nBurnLayer & 2) TC0480SCPTilemapRenderPrio(Layer[1], 0, 4, TaitoChars);
		if (nBurnLayer & 4) TC0480SCPTilemapRenderPrio(Layer[2], 0, 4, TaitoChars);
		if (nBurnLayer & 8) TC0480SCPTilemapRenderPrio(Layer[3], 0, 4, TaitoChars);
	}
	else
	{
		if (nBurnLayer & 1) TC0480SCPTilemapRenderPrio(Layer[0], 0, 1, TaitoChars);
		if (nBurnLayer & 2) TC0480SCPTilemapRenderPrio(Layer[1], 0, 2, TaitoChars);
		if (nBurnLayer & 4) TC0480SCPTilemapRenderPrio(Layer[2], 0, 4, TaitoChars);
		if (nBurnLayer & 8) TC0480SCPTilemapRenderPrio(Layer[3], 0, 8, TaitoChars);
	}

	if (Layer[0] == 3 && Layer[1] == 0 && Layer[2] == 1 && Layer[3] == 2)
	{
		for (INT32 y = 0; y < nScreenHeight; y++)
		{
			UINT16 *bmp = pTransDraw + y * nScreenWidth;
			UINT8 *pri = pPrioDraw + y * nScreenWidth;

			for (INT32 x = 0; x < nScreenWidth; x++)
			{
				if ((pri[x] & 0x02) == 0 && bmp[x] != 0)
					pri[x] |= 0x04;
			}
		}
	}

	draw_sprites_pre(42-96, -571+60);

	if (nSpriteEnable & 1) draw_sprites(1);

	pTransDraw = BurnBitmapGetBitmap(3); // tmp2
	pPrioDraw = BurnBitmapGetPriomap(3);

	BurnBitmapCopy(2, pTransDraw, NULL, 0, 0, 0xf, 0);
	BurnBitmapFill(2, 0); // poly bitmap

	tc0610_rotate_draw();

	BurnBitmapPrimapClear(3);

	if (nSpriteEnable & 2) draw_sprites(0);

	TC0480SCPSetPriMap(pPrioDraw);

	if (nBurnLayer & 8) TC0480SCPRenderCharLayer();
	if (nSpriteEnable & 0x80) TC0100SCNRenderCharLayer(0);

	nScreenWidth = 320;
	nScreenHeight = 232;
	GenericTilesSetClip(0, nScreenWidth, 0, nScreenHeight);

	pTransDraw = BurnBitmapGetBitmap(0);
	pPrioDraw = BurnBitmapGetPriomap(0);

	BurnBitmapFill(0, 0);

	BurnBitmapCopy(3, pTransDraw, NULL, 96, 84, -1, -1);

	BurnTransferCopy(TC0110PCRPalette);

	nScreenWidth = 512;
	nScreenHeight = 400;

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (TaitoReset) {
		DrvDoReset(1);
	}

	{
		TaitoInput[0] = 0xfd;
		TaitoInput[1] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			TaitoInput[0] ^= (TaitoInputPort0[i] & 1) << i;
			TaitoInput[1] ^= (TaitoInputPort1[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[1] = { 16000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		SekOpen(0);

		CPU_RUN(0, Sek);

		if (do_adcirq == i) {
			do_adcirq = -1;
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}
		if (i == 255) {
			SekSetIRQLine(5, CPU_IRQSTATUS_AUTO);
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
		SCAN_VAR(do_adcirq);

		SCAN_VAR(rsyb);
		SCAN_VAR(rsxb);
		SCAN_VAR(rsxoffs);
		SCAN_VAR(rsyoffs);
	}

	return 0;
}


// Galactic Storm (Japan)

static struct BurnRomInfo galastrmRomDesc[] = {
	{ "c99_15.ic105",			0x040000, 0x7eae8efd, TAITO_68KROM1_BYTESWAP32 },	//  0 Main 68ec020 Code
	{ "c99_12.ic102",			0x040000, 0xe059d1ee, TAITO_68KROM1_BYTESWAP32 },	//  1
	{ "c99_13.ic103",			0x040000, 0x885fcb35, TAITO_68KROM1_BYTESWAP32 },	//  2
	{ "c99_14.ic104",			0x040000, 0x457ef6b1, TAITO_68KROM1_BYTESWAP32 },	//  3

	{ "c99_23.ic8",				0x020000, 0x5718ee92, TAITO_68KROM2_BYTESWAP },		//  4 Sound 68k Code
	{ "c99_22.ic7",				0x020000, 0xb90f7c42, TAITO_68KROM2_BYTESWAP },		//  5

	{ "c99-05.ic1",				0x100000, 0xa91ffba4, TAITO_CHARS_BYTESWAP },		//  6 tc0100scn Tiles
	{ "c99-06.ic2",				0x100000, 0x812ed3ae, TAITO_CHARS_BYTESWAP },		//  7

	{ "c99-02.ic50",			0x100000, 0x81e9fc6f, TAITO_SPRITESA_BYTESWAP32 },	//  8 TC0480SCP Tiles & Sprites
	{ "c99-01.ic51",			0x100000, 0x9dda1267, TAITO_SPRITESA_BYTESWAP32 },	//  9
	{ "c99-04.ic66",			0x100000, 0xa681760f, TAITO_SPRITESA_BYTESWAP32 },	// 10
	{ "c99-03.ic67",			0x100000, 0xa2807a27, TAITO_SPRITESA_BYTESWAP32 },	// 11

	{ "c99-11.ic90",			0x080000, 0x26a6926c, TAITO_SPRITEMAP },			// 12 Sprite Map

	{ "c99-08.ic3",				0x100000, 0xfedb4187, TAITO_ES5505_BYTESWAP },		// 13 ENSONIQ Samples/Data
	{ "c99-09.ic4",				0x100000, 0xba70b86b, TAITO_ES5505_BYTESWAP },		// 14
	{ "c99-10.ic5",				0x100000, 0xda016f1e, TAITO_ES5505_BYTESWAP },		// 15
	{ "c99-07.ic2",				0x100000, 0x4cc3136f, TAITO_ES5505_BYTESWAP },		// 16

	{ "eeprom-galastrm.bin",	0x000080, 0x94efa7a6, TAITO_DEFAULT_EEPROM },		// 17 Default EEPROM

	{ "c99-16.bin",				0x000104, 0x9340e376, BRF_OPT },					// 18 PLDs
	{ "c99-17.bin",				0x000144, 0x81d55be5, BRF_OPT },					// 19
	{ "c99-18.bin",				0x000149, 0xeca1501d, BRF_OPT },					// 20
	{ "c99-19.bin",				0x000104, 0x6310ef1d, BRF_OPT },					// 21
	{ "c99-20.bin",				0x000144, 0x5d527b8b, BRF_OPT },					// 22
	{ "c99-21.bin",				0x000104, 0xeb2407a1, BRF_OPT },					// 23
	{ "c99-24.bin",				0x000144, 0xa0ec9b49, BRF_OPT },					// 24
	{ "c99-25.bin",				0x000144, 0xd7cbb8be, BRF_OPT },					// 25
	{ "c99-26.bin",				0x000144, 0xd65cbcb9, BRF_OPT },					// 26
};

STD_ROM_PICK(galastrm)
STD_ROM_FN(galastrm)

struct BurnDriver BurnDrvGalastrm = {
	"galastrm", NULL, NULL, NULL, "1992",
	"Galactic Storm (Japan)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_TAITO_MISC, GBF_SHOOT, 0,
	NULL, galastrmRomInfo, galastrmRomName, NULL, NULL, NULL, NULL, GalastrmInputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1000,
	320, 232, 4, 3
};
