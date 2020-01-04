// FB Alpha Atari Xybots driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarijsa.h"
#include "slapstic.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvPalRAM;
static UINT8 *DrvPfRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 video_int_state;
static INT32 h256_flip;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo XybotsInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},

	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Twist Left",	BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 2"	},
	{"P1 Twist Right",	BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 3"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 1"	},
	{"P2 Twist Left",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 2"	},
	{"P2 Twist Right",	BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Xybots)

static struct BurnDIPInfo XybotsDIPList[]=
{
	{0x14, 0xff, 0xff, 0x01, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x14, 0x01, 0x01, 0x01, "Off"				},
	{0x14, 0x01, 0x01, 0x00, "On"				},
};

STDDIPINFO(Xybots)

static void update_interrupts()
{
	INT32 newstate = 0;

	if (video_int_state) newstate = 1;
	if (atarijsa_int_state) newstate = 2;

	if (newstate)
		SekSetIRQLine(newstate, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void __fastcall xybots_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffc00) == 0xffac00) {
		address = 0x1c00 + (address & 0x3fe);
		*((UINT16*)(Drv68KRAM + address)) = data;
		if (address >= 0x1e00) {
			AtariMoWrite(0, (address / 2) & 0xff, data);
		}
		return;
	}

	switch (address & ~0xff)
	{
		case 0xffe800:
			AtariEEPROMUnlockWrite();
		return;

		case 0xffe900:
			AtariJSAWrite(data);
		return;

		case 0xffea00:
			BurnWatchdogWrite();
		return;

		case 0xffeb00:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0xffee00:
			AtariJSAResetWrite(0);
		return;
	}
}

static void __fastcall xybots_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc00) == 0xffac00) {
		address = 0x1c00 + (address & 0x3ff);
		Drv68KRAM[address^1] = data;
		if (address >= 0x1e00) {
			AtariMoWrite(0, (address / 2) & 0xff, *((UINT16*)(Drv68KRAM + (address & ~1))));
		}
		return;
	}

	switch (address & ~0xff)
	{
		case 0xffe800:
			AtariEEPROMUnlockWrite();
		return;

		case 0xffe900:
			AtariJSAWrite(data);
		return;

		case 0xffea00:
			BurnWatchdogWrite();
		return;

		case 0xffeb00:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0xffee00:
			AtariJSAResetWrite(0);
		return;
	}
}

static inline UINT16 special_read()
{
	UINT16 result = 0xf2ff | ((DrvDips[0] & 1) << 8);

	if (atarigen_cpu_to_sound_ready) result ^= 0x0200;
	result ^= h256_flip;
	result ^= vblank ? 0x0800 : 0;
	h256_flip ^= 0x0400;

	return result;
}

static UINT16 __fastcall xybots_main_read_word(UINT32 address)
{
	switch (address & ~0xff)
	{
		case 0xffe000:
			return AtariJSARead();

		case 0xffe100:
			return DrvInputs[0];

		case 0xffe200:
			return special_read();
	}

	return 0;
}

static UINT8 __fastcall xybots_main_read_byte(UINT32 address)
{
	switch (address & ~0xff)
	{
		case 0xffe000:
			return AtariJSARead() >> ((~address & 1) * 8);

		case 0xffe100:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0xffe200:
			return special_read() >> ((~address & 1) * 8);
	}

	return 0;
}

static tilemap_callback( alpha )
{
	INT32 data = *((UINT16*)(DrvAlphaRAM + offs * 2));

	TILE_SET_INFO(2, data, data >> 12, (data & 0x8000) ? TILE_OPAQUE : 0);
}

static tilemap_callback( bg )
{
	INT32 data = *((UINT16*)(DrvPfRAM + offs * 2));

	TILE_SET_INFO(0, data, data >> 11, TILE_FLIPYX(data >> 15));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	AtariEEPROMReset();
	AtariJSAReset();
	AtariSlapsticReset();

	BurnWatchdogReset();

	video_int_state = 0;
	h256_flip = 0x400;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM		= Next; Next += 0x040000;
	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x080000;
	DrvGfxROM1		= Next; Next += 0x100000;
	DrvGfxROM2		= Next; Next += 0x008000;

	DrvPalette		= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	AllRam			= Next;

	DrvPalRAM		= Next; Next += 0x000800;
	DrvPfRAM		= Next; Next += 0x001000;
	DrvAlphaRAM		= Next; Next += 0x001000;
	Drv68KRAM		= Next; Next += 0x002000;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0,1) };
	INT32 XOffs0[8] = { STEP8(0,4) };
	INT32 YOffs0[8] = { STEP8(0,32) };

	INT32 Plane1[2] = { 0, 4 };
	INT32 XOffs1[8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs1[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x80000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x40000);

	GfxDecode(0x2000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x80000);

	GfxDecode(0x4000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM1);

	memcpy (tmp, DrvGfxROM2, 0x02000);

	GfxDecode(0x0200, 2, 8, 8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM2);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		1,					// index to which gfx system
		1,					// number of motion object banks
		0,					// are the entries linked?
		0,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		0,					// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x100,				// base palette entry
		0x300,				// maximum number of colors
		0,					// transparent pen index

		{{ 0x3f }},			// mask for the link (dummy)
		{{ 0 }},			// mask for the graphics bank
		{{ 0x3fff,0,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0,0,0x000f }},	// mask for the color
		{{ 0,0,0,0xff80 }},	// mask for the X position
		{{ 0,0,0xff80,0 }},	// mask for the Y position
		{{ 0 }},			// mask for the width, in tiles*/
		{{ 0,0,0x0007,0 }},	// mask for the height, in tiles
		{{ 0x8000,0,0,0 }},	// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0,0x000f,0,0 }},	// mask for the priority
		{{ 0 }},			// mask for the neighbor
		{{ 0 }},			// mask for absolute coordinates

		{{ 0 }},			// mask for the special value
		0,					// resulting value to indicate "special"
		NULL				// callback routine for special entries
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
		if (BurnLoadRom(Drv68KROM  + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x08000, DrvGfxROM0 + 0x00000, 0x08000);
		if (BurnLoadRom(DrvGfxROM0 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x030000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,				0x000000, 0x007fff, MAP_ROM);
	SekMapMemory(Drv68KROM + 0x10000,	0x010000, 0x03ffff, MAP_ROM);
	SekMapMemory(DrvAlphaRAM,			0xff8000, 0xff8fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,				0xff9000, 0xffabff, MAP_RAM);
	SekMapMemory(Drv68KRAM + 0x1c00,	0xffac00, 0xffafff, MAP_ROM); // handler mob is >=0xe00
	SekMapMemory(DrvPfRAM,				0xffb000, 0xffbfff, MAP_RAM);
	SekMapMemory(DrvPalRAM,				0xffc000, 0xffc7ff, MAP_RAM);
	SekSetWriteWordHandler(0,			xybots_main_write_word);
	SekSetWriteByteHandler(0,			xybots_main_write_byte);
	SekSetReadWordHandler(0,			xybots_main_read_word);
	SekSetReadByteHandler(0,			xybots_main_read_byte);

	AtariSlapsticInit(Drv68KROM + 0x8000, 107);
	AtariSlapsticInstallMap(1, 0x008000);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(2, 0xffd000, 0xffdfff);

	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	SlapsticInit(107);
	AtariJSAInit(DrvM6502ROM, &update_interrupts, NULL, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, alpha_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x080000, 0x200, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x100000, 0x100, 0x0f);
	GenericTilemapSetGfx(2, DrvGfxROM2, 2, 8, 8, 0x008000, 0x000, 0x07);
	GenericTilemapSetTransparent(1, 0);

	AtariMoInit(0, &modesc);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	SekExit();

	AtariSlapsticExit();
	AtariMoExit();
	AtariEEPROMExit();
	AtariJSAExit();

	BurnFree(AllMem);

	return 0;
}

static void copy_sprites()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				INT32 mopriority = (mo[x] >> 12) ^ 15;
				INT32 pfcolor = (pf[x] >> 4) & 0x0f;
				INT32 prien = ((mo[x] & 0x0f) > 1);

				if (prien)
				{
					if (mopriority <= pfcolor)
					{
						if (mo[x] & 0x80)
							pf[x] = (mo[x] ^ 0x2f0) & 0x3ff;
						else
							pf[x] = mo[x] & 0x3ff;
					}
				}
				else
				{
					if (mopriority < pfcolor)
						pf[x] = mo[x] & 0x3ff;
				}

				mo[x] = 0xffff;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		AtariPaletteUpdate4IRGB(DrvPalRAM, DrvPalette, 0x800);
		DrvRecalc = 1; // force!!
	}

	BurnTransferClear();

	if (nSpriteEnable & 4) AtariMoRender(0);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) copy_sprites();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	M6502NewFrame();

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xf2ff | ((DrvDips[0] & 0x01) << 8);
		DrvInputs[2] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2];
		atarijsa_test_mask = 0x01;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(14318180 / 2 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

		if (i == 239) {
			vblank = 1;
			video_int_state = 1;
			update_interrupts();

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && i&1) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 2);
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment >= 0) {
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
		}
	}

	SekClose();
	M6502Close();

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

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		SekScan(nAction);

		BurnWatchdogScan(nAction);

		AtariSlapsticScan(nAction, pnMin);
		AtariJSAScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		SCAN_VAR(video_int_state);
		SCAN_VAR(h256_flip);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Xybots (rev 2)

static struct BurnRomInfo xybotsRomDesc[] = {
	{ "136054-2112.17cd",	0x10000, 0x16d64748, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136054-2113.19cd",	0x10000, 0x2677d44a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136054-2114.17b",	0x08000, 0xd31890cb, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136054-2115.19b",	0x08000, 0x750ab1b0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136054-1116.2k",		0x10000, 0x3b9f155d, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136054-2102.12l",	0x08000, 0xc1309674, 3 | BRF_GRA },           //  5 Background Tiles
	{ "136054-2103.11l",	0x10000, 0x907c024d, 3 | BRF_GRA },           //  6
	{ "136054-2117.8l",		0x10000, 0x0cc9b42d, 3 | BRF_GRA },           //  7
	
	{ "136054-1105.2e",		0x10000, 0x315a4274, 4 | BRF_GRA },           //  8 Sprites
	{ "136054-1106.2ef",	0x10000, 0x3d8c1dd2, 4 | BRF_GRA },           //  9
	{ "136054-1107.2f",		0x10000, 0xb7217da5, 4 | BRF_GRA },           // 10
	{ "136054-1108.2fj",	0x10000, 0x77ac65e1, 4 | BRF_GRA },           // 11
	{ "136054-1109.2jk",	0x10000, 0x1b482c53, 4 | BRF_GRA },           // 12
	{ "136054-1110.2k",		0x10000, 0x99665ff4, 4 | BRF_GRA },           // 13
	{ "136054-1111.2l",		0x10000, 0x416107ee, 4 | BRF_GRA },           // 14

	{ "136054-1101.5c",		0x02000, 0x59c028a2, 5 | BRF_GRA },           // 15 Characters
};

STD_ROM_PICK(xybots)
STD_ROM_FN(xybots)

struct BurnDriver BurnDrvXybots = {
	"xybots", NULL, NULL, NULL, "1987",
	"Xybots (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, xybotsRomInfo, xybotsRomName, NULL, NULL, NULL, NULL, XybotsInputInfo, XybotsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Xybots (German, rev 3)

static struct BurnRomInfo xybotsgRomDesc[] = {
	{ "136054-3212.17cd",	0x10000, 0x4cac5d7c, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136054-3213.19cd",	0x10000, 0xbfcb0b00, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136054-3214.17b",	0x08000, 0x4ad35093, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136054-3215.19b",	0x08000, 0x3a2afbaf, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136054-1116.2k",		0x10000, 0x3b9f155d, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136054-2102.12l",	0x08000, 0xc1309674, 3 | BRF_GRA },           //  5 Background Tiles
	{ "136054-2103.11l",	0x10000, 0x907c024d, 3 | BRF_GRA },           //  6
	{ "136054-2117.8l",		0x10000, 0x0cc9b42d, 3 | BRF_GRA },           //  7

	{ "136054-1105.2e",		0x10000, 0x315a4274, 4 | BRF_GRA },           //  8 Sprites
	{ "136054-1106.2ef",	0x10000, 0x3d8c1dd2, 4 | BRF_GRA },           //  9
	{ "136054-1107.2f",		0x10000, 0xb7217da5, 4 | BRF_GRA },           // 10
	{ "136054-1108.2fj",	0x10000, 0x77ac65e1, 4 | BRF_GRA },           // 11
	{ "136054-1109.2jk",	0x10000, 0x1b482c53, 4 | BRF_GRA },           // 12
	{ "136054-1110.2k",		0x10000, 0x99665ff4, 4 | BRF_GRA },           // 13
	{ "136054-1111.2l",		0x10000, 0x416107ee, 4 | BRF_GRA },           // 14

	{ "136054-1101.5c",		0x02000, 0x59c028a2, 5 | BRF_GRA },           // 15 Characters
};

STD_ROM_PICK(xybotsg)
STD_ROM_FN(xybotsg)

struct BurnDriver BurnDrvXybotsg = {
	"xybotsg", "xybots", NULL, NULL, "1987",
	"Xybots (German, rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, xybotsgRomInfo, xybotsgRomName, NULL, NULL, NULL, NULL, XybotsInputInfo, XybotsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Xybots (French, rev 3)

static struct BurnRomInfo xybotsfRomDesc[] = {
	{ "136054-3612.17cd",	0x10000, 0xb03a3f3c, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136054-3613.19cd",	0x10000, 0xab33eb1f, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136054-3614.17b",	0x08000, 0x7385e0b6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136054-3615.19b",	0x08000, 0x8e37b812, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136054-1116.2k",		0x10000, 0x3b9f155d, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136054-2102.12l",	0x08000, 0xc1309674, 3 | BRF_GRA },           //  5 Background Tiles
	{ "136054-2103.11l",	0x10000, 0x907c024d, 3 | BRF_GRA },           //  6
	{ "136054-2117.8l",		0x10000, 0x0cc9b42d, 3 | BRF_GRA },           //  7

	{ "136054-1105.2e",		0x10000, 0x315a4274, 4 | BRF_GRA },           //  8 Sprites
	{ "136054-1106.2ef",	0x10000, 0x3d8c1dd2, 4 | BRF_GRA },           //  9
	{ "136054-1107.2f",		0x10000, 0xb7217da5, 4 | BRF_GRA },           // 10
	{ "136054-1108.2fj",	0x10000, 0x77ac65e1, 4 | BRF_GRA },           // 11
	{ "136054-1109.2jk",	0x10000, 0x1b482c53, 4 | BRF_GRA },           // 12
	{ "136054-1110.2k",		0x10000, 0x99665ff4, 4 | BRF_GRA },           // 13
	{ "136054-1111.2l",		0x10000, 0x416107ee, 4 | BRF_GRA },           // 14

	{ "136054-1101.5c",		0x02000, 0x59c028a2, 5 | BRF_GRA },           // 15 Characters
};

STD_ROM_PICK(xybotsf)
STD_ROM_FN(xybotsf)

struct BurnDriver BurnDrvXybotsf = {
	"xybotsf", "xybots", NULL, NULL, "1987",
	"Xybots (French, rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, xybotsfRomInfo, xybotsfRomName, NULL, NULL, NULL, NULL, XybotsInputInfo, XybotsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Xybots (rev 1)

static struct BurnRomInfo xybots1RomDesc[] = {
	{ "136054-1112.17cd",	0x10000, 0x2dbab363, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136054-1113.19cd",	0x10000, 0x847b056e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136054-1114.17b",	0x08000, 0x7444f88f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136054-1115.19b",	0x08000, 0x848d072d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136054-1116.2k",		0x10000, 0x3b9f155d, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136054-2102.12l",	0x08000, 0xc1309674, 3 | BRF_GRA },           //  5 Background Tiles
	{ "136054-2103.11l",	0x10000, 0x907c024d, 3 | BRF_GRA },           //  6
	{ "136054-2117.8l",		0x10000, 0x0cc9b42d, 3 | BRF_GRA },           //  7

	{ "136054-1105.2e",		0x10000, 0x315a4274, 4 | BRF_GRA },           //  8 Sprites
	{ "136054-1106.2ef",	0x10000, 0x3d8c1dd2, 4 | BRF_GRA },           //  9
	{ "136054-1107.2f",		0x10000, 0xb7217da5, 4 | BRF_GRA },           // 10
	{ "136054-1108.2fj",	0x10000, 0x77ac65e1, 4 | BRF_GRA },           // 11
	{ "136054-1109.2jk",	0x10000, 0x1b482c53, 4 | BRF_GRA },           // 12
	{ "136054-1110.2k",		0x10000, 0x99665ff4, 4 | BRF_GRA },           // 13
	{ "136054-1111.2l",		0x10000, 0x416107ee, 4 | BRF_GRA },           // 14

	{ "136054-1101.5c",		0x02000, 0x59c028a2, 5 | BRF_GRA },           // 15 Characters
};

STD_ROM_PICK(xybots1)
STD_ROM_FN(xybots1)

struct BurnDriver BurnDrvXybots1 = {
	"xybots1", "xybots", NULL, NULL, "1987",
	"Xybots (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, xybots1RomInfo, xybots1RomName, NULL, NULL, NULL, NULL, XybotsInputInfo, XybotsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};


// Xybots (rev 0)

static struct BurnRomInfo xybots0RomDesc[] = {
	{ "136054-0112.17cd",	0x10000, 0x4b830ac4, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136054-0113.19cd",	0x10000, 0xdcfbf8a7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136054-0114.17b",	0x08000, 0x18b875f7, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136054-0115.19b",	0x08000, 0x7f116360, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136054-0116.2k",		0x10000, 0x3b9f155d, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136054-1102.12l",	0x08000, 0x0d304e5b, 3 | BRF_GRA },           //  5 Background Tiles
	{ "136054-1103.11l",	0x10000, 0xa514da1d, 3 | BRF_GRA },           //  6
	{ "136054-1117.8l",		0x10000, 0x6b79154d, 3 | BRF_GRA },           //  7

	{ "136054-1105.2e",		0x10000, 0x315a4274, 4 | BRF_GRA },           //  8 Sprites
	{ "136054-1106.2ef",	0x10000, 0x3d8c1dd2, 4 | BRF_GRA },           //  9
	{ "136054-1107.2f",		0x10000, 0xb7217da5, 4 | BRF_GRA },           // 10
	{ "136054-1108.2fj",	0x10000, 0x77ac65e1, 4 | BRF_GRA },           // 11
	{ "136054-1109.2jk",	0x10000, 0x1b482c53, 4 | BRF_GRA },           // 12
	{ "136054-1110.2k",		0x10000, 0x99665ff4, 4 | BRF_GRA },           // 13
	{ "136054-1111.2l",		0x10000, 0x416107ee, 4 | BRF_GRA },           // 14

	{ "136054-1101.5c",		0x02000, 0x59c028a2, 5 | BRF_GRA },           // 15 Characters
};

STD_ROM_PICK(xybots0)
STD_ROM_FN(xybots0)

struct BurnDriver BurnDrvXybots0 = {
	"xybots0", "xybots", NULL, NULL, "1987",
	"Xybots (rev 0)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, xybots0RomInfo, xybots0RomName, NULL, NULL, NULL, NULL, XybotsInputInfo, XybotsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	336, 240, 4, 3
};
