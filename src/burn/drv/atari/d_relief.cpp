// FB Alpha Atari Relief Pitcher driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarivad.h"
#include "msm6295.h"
#include "burn_ym2413.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvMobRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 oki_bank;

static INT32 vblank, hblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT16 DrvInputs[4];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static INT32 lastline = 0;

static struct BurnInputInfo ReliefInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 15,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 13,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 12,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 10,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 0,	"service"	},
	{"Service 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"service"	},
	{"Service 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"service"	},
	{"Service 3",		BIT_DIGITAL,	DrvJoy2 + 7,	"service"	},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Relief)

static struct BurnDIPInfo ReliefDIPList[]=
{
	{0x17, 0xff, 0xff, 0x40, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x17, 0x01, 0x40, 0x40, "Off"				},
	{0x17, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFO(Relief)

static UINT16 special_port()
{
	UINT16 ret = (DrvInputs[2] & ~0x40) | (DrvDips[0] & 0x40);
	if (vblank) ret &= ~0x0081;
	if (hblank) ret &= ~0x0001;
	return ret;
}

static UINT16 __fastcall relief_read_word(UINT32 address)
{
	if (address & 0xc00000) {
		return SekReadWord(address & 0x3fffff);
	}

	switch (address)
	{
		case 0x140010:
			return MSM6295Read(0);

		case 0x260000:
			return DrvInputs[0];

		case 0x260002:
			return DrvInputs[1];

		case 0x260010:
			return special_port();

		case 0x260012:
			return DrvInputs[3];
	}

	return 0;
}

static UINT8 __fastcall relief_read_byte(UINT32 address)
{
	if (address & 0xc00000) {
		return SekReadByte(address & 0x3fffff);
	}

	switch (address)
	{
		case 0x140010:
		case 0x140011:
			return MSM6295Read(0);

		case 0x260000:
		case 0x260001:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0x260002:
		case 0x260003:
			return DrvInputs[1] >> ((~address & 1) * 8);

		case 0x260010:
		case 0x260011:
			return special_port() >> ((~address & 1) * 8);

		case 0x260012:
		case 0x260013:
			return DrvInputs[3] >> ((~address & 1) * 8);
	}

	return 0;
}

static void set_oki_bank(INT32 data)
{
	oki_bank = data;

	MSM6295SetBank(0, DrvSndROM + (oki_bank * 0x20000), 0, 0x1ffff);
}

static void __fastcall relief_write_word(UINT32 address, UINT16 data)
{
	if (address & 0xc00000) {
		SekWriteWord(address & 0x3fffff, data);
		return;
	}

	if ((address & 0xfff800) == 0x3f6000) {
		*((UINT16*)(DrvMobRAM + (address & 0x7fe))) = data;
		AtariMoWrite(0, (address / 2) & 0x3ff, data);
		return;
	}

	switch (address)
	{
		case 0x140000:
		case 0x140002:
			BurnYM2413Write((address / 2) & 1, data);
		return;

		case 0x140010:
			MSM6295Write(0, data);
		return;

		case 0x140020:
			/// volume_w
		return;

		case 0x140030:
			set_oki_bank((data >> 6) & 7);
		return;

		case 0x1c0030:
			AtariEEPROMUnlockWrite();
		return;

		case 0x2a0001:
			BurnWatchdogWrite();
		return;
	}
}

static void __fastcall relief_write_byte(UINT32 address, UINT8 data)
{
	if (address & 0xc00000) {
		SekWriteByte(address & 0x3fffff, data);
		return;
	}

	if ((address & 0xfff800) == 0x3f6000) {
		DrvMobRAM[(address & 0x7ff) ^ 1] = data;
		AtariMoWrite(0, (address / 2) & 0x3ff, *((UINT16*)(DrvMobRAM + (address & 0x7fe))));
		return;
	}

	switch (address)
	{
		case 0x140000:
		case 0x140001:
		case 0x140002:
		case 0x140003:
			BurnYM2413Write((address / 2) & 1, data);
		return;

		case 0x140010:
		case 0x140011:
			MSM6295Write(0, data);
		return;

		case 0x140020:
		case 0x140021:
			/// volume_w
		return;

		case 0x140030:
			set_oki_bank(((data << 2) & 4) | (oki_bank & 3)); // wrong?
		return;

		case 0x140031:
			set_oki_bank(((data >> 6) & 3) | (oki_bank & 4));
		return;

		case 0x1c0030:
		case 0x1c0031:
			AtariEEPROMUnlockWrite();
		return;

		case 0x2a0001:
		case 0x2a0000:
			BurnWatchdogWrite();
		return;
	}
}

static void palette_write(INT32 offset, UINT16 data)
{
	UINT8 i = data >> 15;
	UINT8 r = ((data >>  9) & 0x3e) | i;
	UINT8 g = ((data >>  4) & 0x3e) | i;
	UINT8 b = ((data <<  1) & 0x3e) | i;

	r = (r << 2) | (r >> 4);
	g = (g << 2) | (g >> 4);
	b = (b << 2) | (b >> 4);

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
}

static void scanline_timer(INT32 state)
{
	SekSetIRQLine(4, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	BurnWatchdogReset();

	AtariEEPROMReset();
	AtariVADReset();

	BurnYM2413Reset();
	set_oki_bank(1);
	MSM6295Reset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x080000;

	DrvGfxROM0			= Next; Next += 0x280000;
	DrvGfxROM1			= Next; Next += 0x200000;

	MSM6295ROM			= Next;
	DrvSndROM			= Next; Next += 0x100000;

	DrvPalette			= (UINT32*)Next; Next += 0x0800 * sizeof(UINT32);

	AllRam				= Next;

	atarimo_0_spriteram = (UINT16*)Next;

	DrvMobRAM			= Next; Next += 0x00a000;

	atarimo_0_slipram	= (UINT16*)(DrvMobRAM + 0x2f80);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode() // 0, 100
{
	INT32 Plane[5] = { 0x40000*8*4, 0x40000*8*3, 0x40000*8*2, 0x40000*8*1, 0x40000*8*0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8(0,8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x140000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x280000; i++) {
		INT32 p = DrvGfxROM0[i] ^= 0xff;
		if (i & 1) {
			DrvGfxROM1[i/2] = p;
		} else {
			DrvGfxROM0[i/2] = p;
		}
	}

	memcpy (tmp, DrvGfxROM0, 0x100000);

	GfxDecode(0x8000, 4, 8, 8, Plane + 1, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x140000);

	GfxDecode(0x8000, 5, 8, 8, Plane + 0, XOffs, YOffs, 0x040, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		2,					// index to which gfx system
		1,					// number of motion object banks
		1,					// are the entries linked?
		0,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		8,					// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x100,				// base palette entry
		0x100,				// maximum number of colors
		0,					// transparent pen index

		{{ 0x00ff,0,0,0 }},	// mask for the link
		{{ 0 }},			// mask for the graphics bank
		{{ 0,0x7fff,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0,0x000f,0 }},	// mask for the color
		{{ 0,0,0xff80,0 }},	// mask for the X position
		{{ 0,0,0,0xff80 }},	// mask for the Y position
		{{ 0,0,0,0x0070 }},	// mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	// mask for the height, in tiles
		{{ 0,0x8000,0,0 }},	// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0 }},			// mask for the priority
		{{ 0 }},			// mask for the neighbor
		{{ 0 }},			// mask for absolute coordinates

		{{ 0 }},			// mask for the special value
		0,					// resulting value to indicate "special"
		0					// callback routine for special entries
	};

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x180000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x200001,  k++, 2)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x080000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvMobRAM  + 0x000000,  k++, 1)) return 1; // temp memory

		DrvGfxDecode();
	}

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x200000, 0x200, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM0, 4, 8, 8, 0x200000, 0x000, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM1, 4, 8, 8, 0x200000, 0x100, 0x0f);

	AtariVADInit(0, 1, 0, scanline_timer, palette_write);
	AtariMoInit(0, &modesc);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvNVRAM,			0x180000, 0x180fff, MAP_ROM);
	SekMapMemory(DrvMobRAM,			0x3f6000, 0x3f67ff, MAP_ROM);
	SekMapMemory(DrvMobRAM + 0x800,	0x3f6800, 0x3fffff, MAP_RAM);
	SekSetWriteWordHandler(0,		relief_write_word);
	SekSetWriteByteHandler(0,		relief_write_byte);
	SekSetReadWordHandler(0,		relief_read_word);
	SekSetReadByteHandler(0,		relief_read_byte);

	AtariVADMap(0x3e0000, 0x3f5fff, 0);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1,		0x180000, 0x180fff);
	AtariEEPROMLoad(DrvMobRAM); // temp memory
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	BurnYM2413Init(2500000);
	BurnYM2413SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1193181 / MSM6295_PIN7_LOW, 1);
	MSM6295SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	BurnYM2413Exit();
	MSM6295Exit();

	AtariVADExit();
	AtariMoExit();
	AtariEEPROMExit();

	BurnFree (AllMem);

	return 0;
}

static void sprite_copy()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);
		UINT8 *pri = BurnBitmapGetPrimapPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				INT32 cs0 = ((mo[x] & 0x0f) == 0);

				if ((!cs0 && (mo[x] & 0xe0) == 0xe0) ||
					((mo[x] & 0xf0) == 0xe0) ||
					(!pri[x] && !cs0) ||
					(!pri[x] && !(mo[x] & 0x10)))
					pf[x] = mo[x];

				mo[x] = 0xffff;
			}
		}
	}
}

static void draw_scanline(INT32 line)
{
	if (!pBurnDraw || line > 239) return;

	GenericTilesSetClip(-1, -1, lastline, line + 1);

	AtariVADDraw(pTransDraw, 0);

	AtariMoRender(0);

	if (nSpriteEnable & 1) sprite_copy();

	GenericTilesClearClip();

	lastline = line + 1;
}

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		AtariVADRecalcPalette();
		DrvRecalc = 0;
	}

	if (pBurnDraw)
		BurnTransferClear();

	lastline = 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) { // bpp changed, let's not present a blank screen
		DrvDrawBegin();
	}

	draw_scanline(239); // 0 - 240

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[1] = { 7159090 / 60 }; // 59.92HZ
	INT32 nCyclesDone[1] = { 0 };

	vblank = 0;

	DrvDrawBegin();

	SekOpen(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		atarivad_scanline = i;
		if (i == 0) AtariVADEOFUpdate((UINT16*)(DrvMobRAM + 0x2f00));

		hblank = 0;
		INT32 sek_line = ((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0];
		nCyclesDone[0] += SekRun((INT32)((double)sek_line * 0.9));
		hblank = 1;
		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);

		if ((i % 64) == 0) draw_scanline(i);

		if (atarivad_scanline_timer_enabled) {
			if (atarivad_scanline_timer == atarivad_scanline) {
				draw_scanline(i);
				scanline_timer(CPU_IRQSTATUS_ACK);
			}
		}

		if (i == 239) {
			vblank = 1;
		}

		if (pBurnSoundOut && (i & 1)) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave / 2);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2413Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	SekClose();

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		BurnYM2413Render(pSoundBuf, nSegmentLength);
		MSM6295Render(pBurnSoundOut, nBurnSoundLen);
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
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);

		MSM6295Scan(nAction, pnMin);
		BurnYM2413Scan(nAction, pnMin);

		BurnWatchdogScan(nAction);
		AtariVADScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		SCAN_VAR(oki_bank);
	}

	if (nAction & ACB_WRITE) {
		set_oki_bank(oki_bank);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Relief Pitcher (set 1, 07 Jun 1992 / 28 May 1992)

static struct BurnRomInfo reliefRomDesc[] = {
	{ "136093-0011d.19e",			0x20000, 0xcb3f73ad, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "136093-0012d.19j",			0x20000, 0x90655721, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136093-0013.17e",			0x20000, 0x1e1e82e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136093-0014.17j",			0x20000, 0x19e5decd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136093-0025a.14s",			0x80000, 0x1b9e5ef2, 2 | BRF_GRA },           //  4 Graphics (Interleaved)
	{ "136093-0026a.8d",			0x80000, 0x09b25d93, 2 | BRF_GRA },           //  5
	{ "136093-0027a.18s",			0x80000, 0x5bc1c37b, 2 | BRF_GRA },           //  6
	{ "136093-0028a.10d",			0x80000, 0x55fb9111, 2 | BRF_GRA },           //  7
	{ "136093-0029a.4d",			0x40000, 0xe4593ff4, 2 | BRF_GRA },           //  8

	{ "136093-0030a.9b",			0x80000, 0xf4c567f5, 3 | BRF_SND },           //  9 Samples
	{ "136093-0031a.10b",			0x80000, 0xba908d73, 3 | BRF_SND },           // 10

	{ "relief-eeprom.bin",			0x00800, 0x66069f60, 4 | BRF_PRG | BRF_ESS }, // 11 Default EEPROM Data

	{ "gal16v8a-136093-0002.15f",	0x00117, 0xb111d5f2, 5 | BRF_OPT },           // 12 PLDs
	{ "gal16v8a-136093-0003.11r",	0x00117, 0x67165ed2, 5 | BRF_OPT },           // 13
	{ "gal16v8a-136093-0004.5n",	0x00117, 0x047b384a, 5 | BRF_OPT },           // 14
	{ "gal16v8a-136093-0005.3f",	0x00117, 0xf76825b7, 5 | BRF_OPT },           // 15
	{ "gal16v8a-136093-0006.5f",	0x00117, 0xc580d2a9, 5 | BRF_OPT },           // 16
	{ "gal16v8a-136093-0008.3e",	0x00117, 0x0117910e, 5 | BRF_OPT },           // 17
	{ "gal16v8a-136093-0009.8a",	0x00117, 0xb8679030, 5 | BRF_OPT },           // 18
	{ "gal16v8a-136093-0010.15a",	0x00117, 0x5f49c736, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(relief)
STD_ROM_FN(relief)

struct BurnDriver BurnDrvRelief = {
	"relief", NULL, NULL, NULL, "1992",
	"Relief Pitcher (set 1, 07 Jun 1992 / 28 May 1992)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, reliefRomInfo, reliefRomName, NULL, NULL, NULL, NULL, ReliefInputInfo, ReliefDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};


// Relief Pitcher (set 2, 26 Apr 1992 / 08 Apr 1992)

static struct BurnRomInfo relief2RomDesc[] = {
	{ "19e",						0x20000, 0x41373e02, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "19j",						0x20000, 0x8187b026, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136093-0013.17e",			0x20000, 0x1e1e82e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136093-0014.17j",			0x20000, 0x19e5decd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136093-0025a.14s",			0x80000, 0x1b9e5ef2, 2 | BRF_GRA },           //  4 Graphics (Interleaved)
	{ "136093-0026a.8d",			0x80000, 0x09b25d93, 2 | BRF_GRA },           //  5
	{ "136093-0027a.18s",			0x80000, 0x5bc1c37b, 2 | BRF_GRA },           //  6
	{ "136093-0028a.10d",			0x80000, 0x55fb9111, 2 | BRF_GRA },           //  7
	{ "136093-0029.4d",				0x40000, 0xe4593ff4, 2 | BRF_GRA },           //  8

	{ "136093-0030a.9b",			0x80000, 0xf4c567f5, 3 | BRF_SND },           //  9 Samples
	{ "136093-0031a.10b",			0x80000, 0xba908d73, 3 | BRF_SND },           // 10

	{ "relief2-eeprom.bin",			0x00800, 0x2131fc40, 4 | BRF_PRG | BRF_ESS }, // 11 Default EEPROM Data

	{ "gal16v8a-136093-0002.15f",	0x00117, 0xb111d5f2, 5 | BRF_OPT },           // 12 PLDs
	{ "gal16v8a-136093-0003.11r",	0x00117, 0x67165ed2, 5 | BRF_OPT },           // 13
	{ "gal16v8a-136093-0004.5n",	0x00117, 0x047b384a, 5 | BRF_OPT },           // 14
	{ "gal16v8a-136093-0005.3f",	0x00117, 0xf76825b7, 5 | BRF_OPT },           // 15
	{ "gal16v8a-136093-0006.5f",	0x00117, 0xc580d2a9, 5 | BRF_OPT },           // 16
	{ "gal16v8a-136093-0008.3e",	0x00117, 0x0117910e, 5 | BRF_OPT },           // 17
	{ "gal16v8a-136093-0009.8a",	0x00117, 0xb8679030, 5 | BRF_OPT },           // 18
	{ "gal16v8a-136093-0010.15a",	0x00117, 0x5f49c736, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(relief2)
STD_ROM_FN(relief2)

struct BurnDriver BurnDrvRelief2 = {
	"relief2", "relief", NULL, NULL, "1992",
	"Relief Pitcher (set 2, 26 Apr 1992 / 08 Apr 1992)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, relief2RomInfo, relief2RomName, NULL, NULL, NULL, NULL, ReliefInputInfo, ReliefDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};


// Relief Pitcher (set 3, 10 Apr 1992 / 08 Apr 1992)

static struct BurnRomInfo relief3RomDesc[] = {
	{ "136093-0011b.19e",			0x20000, 0x794cea33, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "136093-0012b.19j",			0x20000, 0x577495f8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136093-0013.17e",			0x20000, 0x1e1e82e5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136093-0014.17j",			0x20000, 0x19e5decd, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136093-0025a.14s",			0x80000, 0x1b9e5ef2, 2 | BRF_GRA },           //  4 Graphics (Interleaved)
	{ "136093-0026a.8d",			0x80000, 0x09b25d93, 2 | BRF_GRA },           //  5
	{ "136093-0027a.18s",			0x80000, 0x5bc1c37b, 2 | BRF_GRA },           //  6
	{ "136093-0028a.10d",			0x80000, 0x55fb9111, 2 | BRF_GRA },           //  7
	{ "136093-0029.4d",				0x40000, 0xe4593ff4, 2 | BRF_GRA },           //  8

	{ "136093-0030a.9b",			0x80000, 0xf4c567f5, 3 | BRF_SND },           //  9 Samples
	{ "136093-0031a.10b",			0x80000, 0xba908d73, 3 | BRF_SND },           // 10

	{ "relief3-eeprom.bin",			0x00800, 0x2131fc40, 4 | BRF_PRG | BRF_ESS }, // 11 Default EEPROM Data

	{ "gal16v8a-136093-0002.15f",	0x00117, 0xb111d5f2, 5 | BRF_OPT },           // 12 PLDs
	{ "gal16v8a-136093-0003.11r",	0x00117, 0x67165ed2, 5 | BRF_OPT },           // 13
	{ "gal16v8a-136093-0004.5n",	0x00117, 0x047b384a, 5 | BRF_OPT },           // 14
	{ "gal16v8a-136093-0005.3f",	0x00117, 0xf76825b7, 5 | BRF_OPT },           // 15
	{ "gal16v8a-136093-0006.5f",	0x00117, 0xc580d2a9, 5 | BRF_OPT },           // 16
	{ "gal16v8a-136093-0008.3e",	0x00117, 0x0117910e, 5 | BRF_OPT },           // 17
	{ "gal16v8a-136093-0009.8a",	0x00117, 0xb8679030, 5 | BRF_OPT },           // 18
	{ "gal16v8a-136093-0010.15a",	0x00117, 0x5f49c736, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(relief3)
STD_ROM_FN(relief3)

struct BurnDriver BurnDrvRelief3 = {
	"relief3", "relief", NULL, NULL, "1992",
	"Relief Pitcher (set 3, 10 Apr 1992 / 08 Apr 1992)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, relief3RomInfo, relief3RomName, NULL, NULL, NULL, NULL, ReliefInputInfo, ReliefDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};
