// FB Alpha Atari Vindicators Driver Module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarijsa.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvPalRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 mob_scroll_x;
static INT32 mob_scroll_y;
static INT32 bg_scroll_x;
static INT32 bg_scroll_y;
static INT32 playfield_tile_bank;
static INT32 scanline_int_state;

static INT32 vblank, scanline;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT8 DrvJoy5f[16];
static UINT8 DrvJoy6f[16];
static UINT16 DrvInputs[6];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo VindictrInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},

	{"P1 Start",			BIT_DIGITAL,	DrvJoy3 + 8,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvJoy1 + 12,	"p1 up_alt"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down_alt"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvJoy1 + 13,	"p3 up_alt"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvJoy1 + 15,	"p3 down_alt"	},
	{"P1 Left Stick Fire",	BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},
	{"P1 Right Stick Fire",	BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 2"	},
	{"P1 Left Stick Thumb",	BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 3"	},
	{"P1 Right Stick Thumb",BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 4"	},
	{"P1 -- Alt. Input --", BIT_DIGITAL,    DrvJoy5f + 10,  "p1 altcontrol" },
	{"P1 Up",			    BIT_DIGITAL,	DrvJoy5f + 0,	"p1 up"		},
	{"P1 Down",			    BIT_DIGITAL,	DrvJoy5f + 1,	"p1 down"	},
	{"P1 Left",			    BIT_DIGITAL,	DrvJoy5f + 2,	"p1 left"	},
	{"P1 Right",		    BIT_DIGITAL,	DrvJoy5f + 3,	"p1 right"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy3 + 9,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvJoy2 + 12,	"p2 up_alt"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvJoy2 + 14,	"p2 down_alt"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvJoy2 + 13,	"p4 up_alt"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvJoy2 + 15,	"p4 down_alt"	},
	{"P2 Left Stick Fire",	BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 1"	},
	{"P2 Right Stick Fire",	BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 2"	},
	{"P2 Left Stick Thumb",	BIT_DIGITAL,	DrvJoy2 + 10,	"p2 fire 3"	},
	{"P2 Right Stick Thumb",BIT_DIGITAL,	DrvJoy2 + 11,	"p2 fire 4"	},
	{"P2 -- Alt. Input --", BIT_DIGITAL,    DrvJoy6f + 10,  "p2 altcontrol" },
	{"P2 Up",			    BIT_DIGITAL,	DrvJoy6f + 0,	"p2 up"		},
	{"P2 Down",			    BIT_DIGITAL,	DrvJoy6f + 1,	"p2 down"	},
	{"P2 Left",			    BIT_DIGITAL,	DrvJoy6f + 2,	"p2 left"	},
	{"P2 Right",		    BIT_DIGITAL,	DrvJoy6f + 3,	"p2 right"	},

	{"P3 Coin",				BIT_DIGITAL,	DrvJoy4 + 2,	"p3 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Vindictr)

static struct BurnDIPInfo VindictrDIPList[]=
{
	{0x20, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x20, 0x01, 0x02, 0x02, "Off"			},
	{0x20, 0x01, 0x02, 0x00, "On"			},
};

STDDIPINFO(Vindictr)

static void update_interrupts()
{
	INT32 state = 0;
	if (scanline_int_state) state |= 4;
	if (atarijsa_int_state) state |= 6;

	if (state) {
		SekSetIRQLine(state, CPU_IRQSTATUS_ACK);
	} else {
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
	}
}

static void __fastcall vindictr_write_word(UINT32 address, UINT16 data)
{
	if (address & 0xc00000) {
		SekWriteWord(address & 0x3fffff, data);
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xff6000) == 0x3f2000) {
		*((UINT16*)(DrvMobRAM + (address & 0x1ffe))) = data;
		AtariMoWrite(0, ((address & 0x1fff) / 2) & 0xfff, data);
		return;
	}

	switch (address)
	{
		case 0x2e0000:
			BurnWatchdogWrite();
		return;

		case 0x360000:
			scanline_int_state = 0;
			update_interrupts();
		return;

		case 0x360010:
			// nop
		return;

		case 0x360020:
			AtariJSAResetWrite(0);
		return;

		case 0x360030:
			AtariJSAWrite(data);
		return;
	}
}

static void __fastcall vindictr_write_byte(UINT32 address, UINT8 data)
{
	if (address & 0xc00000) {
		SekWriteByte(address & 0x3fffff, data);
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xff6000) == 0x3f2000) {
		DrvMobRAM[(address & 0x1fff)^1] = data;
		AtariMoWrite(0, ((address & 0x1fff) / 2) & 0xfff, *((UINT16*)(DrvMobRAM + (address & 0x1ffe))));
		return;
	}

	switch (address)
	{
		case 0x2e0000:
		case 0x2e0001:
			BurnWatchdogWrite();
		return;

		case 0x360000:
		case 0x360001:
			scanline_int_state = 0;
			update_interrupts();
		return;

		case 0x360010:
		case 0x360011:
			// nop
		return;

		case 0x360020:
		case 0x360021:
			AtariJSAResetWrite(0);
		return;

		case 0x360031:
		case 0x360030:
			AtariJSAWrite(data);
		return;
	}
}

static inline UINT16 special_read()
{
	UINT16 ret = (DrvInputs[1] & ~1) | (vblank^1);
	if (atarigen_cpu_to_sound_ready) ret ^= 0x0008;
	if (atarigen_sound_to_cpu_ready) ret ^= 0x0004;
	return ret;
}

static UINT16 __fastcall vindictr_read_word(UINT32 address)
{
	if (address & 0xc00000) {
		return SekReadWord(address & 0x3fffff);
	}

	switch (address & ~0xf)
	{
		case 0x260000:
			return DrvInputs[0];

		case 0x260010:
			return special_read();

		case 0x260020:
			return DrvInputs[2];

		case 0x260030:
			return AtariJSARead();
	}

	return 0;
}

static UINT8 __fastcall vindictr_read_byte(UINT32 address)
{
	if (address & 0xc00000) {
		return SekReadByte(address & 0x3fffff);
	}

	switch (address & ~0xf)
	{
		case 0x260000:
			return DrvInputs[0] >> ((~address & 1) * 8);

		case 0x260010:
			return special_read() >> ((~address & 1) * 8);

		case 0x260020:
			return DrvInputs[2] >> ((~address & 1) * 8);

		case 0x260030:
			return AtariJSARead() >> ((~address & 1) * 8);
	}

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 data = *((UINT16*)(DrvVidRAM + offs * 2));

	INT32 code = (data & 0xfff) | (playfield_tile_bank * 0x1000);
	INT32 color = (data & 0x7000) >> 11;

	TILE_SET_INFO(0, code, color, TILE_FLIPYX(data >> 15));
}

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + offs * 2));

	INT32 color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);

	TILE_SET_INFO(1, data, color, (data & 0x8000) ? TILE_OPAQUE : 0);
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
	AtariJSAReset();

	playfield_tile_bank = 0;
	mob_scroll_x = 0;
	mob_scroll_y = 0;
	bg_scroll_x = 0;
	bg_scroll_y = 0;
	scanline_int_state = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x060000;
	DrvM6502ROM			= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x200000;
	DrvGfxROM1			= Next; Next += 0x010000;

	DrvPalette			= (UINT32*)Next; Next += 0x4000 * sizeof(UINT32);

	AllRam				= Next;

	DrvPalRAM			= Next; Next += 0x001000;
	DrvVidRAM			= Next; Next += 0x002000;
	DrvMobRAM			= Next; Next += 0x002000;
	DrvAlphaRAM			= Next; Next += 0x001000;
	Drv68KRAM			= Next; Next += 0x003000;

	atarimo_0_slipram 	= (UINT16*)(DrvAlphaRAM + 0xf80);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0, 0x40000*8) };
	INT32 XOffs0[8] = { STEP8(0,1) };
	INT32 YOffs0[8] = { STEP8(0,8) };

	INT32 Plane1[3] = { 0, 4 };
	INT32 XOffs1[8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs1[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x100000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x8000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x004000);

	GfxDecode(0x0400, 2, 8, 8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		0,					// index to which gfx system
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

		{{ 0,0,0,0x03ff }},	// mask for the link
		{{ 0 }},			// mask for the graphics bank
		{{ 0x7fff,0,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0x000f,0,0 }},	// mask for the color
		{{ 0,0xff80,0,0 }},	// mask for the X position
		{{ 0,0,0xff80,0 }},	// mask for the Y position
		{{ 0,0,0x0038,0 }},	// mask for the width, in tiles*/
		{{ 0,0,0x0007,0 }},	// mask for the height, in tiles
		{{ 0,0,0x0040,0 }},	// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0,0x0070,0,0 }},	// mask for the priority
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
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x30000, DrvGfxROM0 + 0x20000, 0x10000);
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 1)) return 1;
		memcpy (DrvGfxROM0 + 0x70000, DrvGfxROM0 + 0x60000, 0x10000);
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a0000, k++, 1)) return 1;
		memcpy (DrvGfxROM0 + 0xb0000, DrvGfxROM0 + 0xa0000, 0x10000);
		if (BurnLoadRom(DrvGfxROM0 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e0000, k++, 1)) return 1;
		memcpy (DrvGfxROM0 + 0xf0000, DrvGfxROM0 + 0xe0000, 0x10000);

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68010);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x05ffff, MAP_ROM);
	SekMapMemory(DrvPalRAM,			0x3e0000, 0x3e0fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,			0x3f0000, 0x3f1fff, MAP_RAM);
	SekMapMemory(DrvMobRAM,			0x3f2000, 0x3f3fff, MAP_ROM);
	SekMapMemory(DrvAlphaRAM,		0x3f4000, 0x3f4fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x3f5000, 0x3f7fff, MAP_RAM);
	SekMapMemory(DrvVidRAM,			0x3f8000, 0x3f9fff, MAP_RAM); // mirroring
	SekMapMemory(DrvMobRAM,			0x3fa000, 0x3fbfff, MAP_ROM);
	SekMapMemory(DrvAlphaRAM,		0x3fc000, 0x3fcfff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0x3fd000, 0x3fffff, MAP_RAM);
	SekSetWriteWordHandler(0,		vindictr_write_word);
	SekSetWriteByteHandler(0,		vindictr_write_byte);
	SekSetReadWordHandler(0,		vindictr_read_word);
	SekSetReadByteHandler(0,		vindictr_read_byte);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1, 0x0e0000, 0x0e0fff);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, NULL, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, alpha_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x200000, 0x200, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM1, 2, 8, 8, 0x010000, 0x000, 0x3f);
	GenericTilemapSetTransparent(1, 0);

	AtariMoInit(0, &modesc);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	AtariJSAExit();
	AtariMoExit();
	AtariEEPROMExit();

	BurnFree(AllMem);

	return 0;
}

static void copy_sprites_step1()
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
				INT32 mopriority = mo[x] >> 12;

				if (mopriority & 4)
					continue;

				if ((mo[x] & 0x0f) == 1)
				{
					if ((mo[x] & 0xf0) != 0)
						pf[x] |= 0x100;
				}
				else
					pf[x] = mo[x] & 0x7ff;
			}
		}
	}
}

static void copy_sprites_step2()
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
				INT32 mopriority = mo[x] >> 12;

				if (mopriority & 4)
				{
					if (mo[x] & 2)
						AtariMoApplyStain(pf, mo, x);

					if (mo[x] & 8)
						pf[x] |= (~mo[x] & 0xe0) << 6;
				}

				mo[x] = 0xffff;
			}
		}
	}
}

static void partial_update_sprite(INT32 line);
static void partial_update(INT32 line);

static void scanline_update()
{
	INT32 offset = ((scanline - 8) / 8) * 64 + 42;
	if (offset < 0)	offset += 0x7c0;
	else if (offset >= 0x7c0)
		return;

	partial_update(scanline);

	for (INT32 x = 42; x < 64; x++, offset++)
	{
		UINT16 data = *((UINT16*)(DrvAlphaRAM + offset * 2));

		switch ((data >> 9) & 7)
		{
			case 2:
				playfield_tile_bank = data & 7;
			break;

			case 3: 
				bg_scroll_x = data & 0x1ff;
				GenericTilemapSetScrollX(0, bg_scroll_x);
			break;

			case 4:
				mob_scroll_x = data & 0x1ff;
				atarimo_set_xscroll(0, mob_scroll_x);
			break;

			case 6:
				partial_update_sprite(scanline);
				scanline_int_state = 1;
				update_interrupts();
			break;

			case 7:
			{
				INT32 sl = scanline;
				if (sl >= nScreenHeight) sl -= nScreenHeight;
				bg_scroll_y = (data - sl) & 0x1ff;
				GenericTilemapSetScrollY(0, bg_scroll_y);
				atarimo_set_yscroll(0, bg_scroll_y);
			}
			break;
		}
	}
}

static INT32 lastline_sprite = 0;
static INT32 lastline = 0;

static void DrvDrawBegin()
{
	lastline = 0;
	lastline_sprite = 0;
	if (pBurnDraw) BurnTransferClear();
}

static void partial_update_sprite(INT32 line)
{
	if (line >= 240 || !pBurnDraw) return;
	//bprintf(0, _T("%08X: (tmap) partial %d - %d\n"), nCurrentFrame, lastline, line);
	GenericTilesSetClip(0, nScreenWidth, lastline_sprite, line+1);
	if (nSpriteEnable & 4) AtariMoRender(0);
	GenericTilesClearClip();

	lastline_sprite = line+1;
}

static void partial_update(INT32 line)
{
	if (line >= 240 || !pBurnDraw) return;
	//bprintf(0, _T("%08X: (spr) partial %d - %d\n"), nCurrentFrame, lastline, line);
	GenericTilesSetClip(0, nScreenWidth, lastline, line+1);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);
	if (nSpriteEnable & 1) copy_sprites_step1();
	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 1);
	if (nSpriteEnable & 1) copy_sprites_step2();

	GenericTilesClearClip();

	lastline = line+1;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		AtariPaletteUpdate4IRGB(DrvPalRAM, DrvPalette, 0x1000);
		DrvRecalc = 1; // force!!
	}

	partial_update_sprite(239);
	partial_update(239);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static UINT16 udlr(UINT8 real_port, UINT8 fake_port)
{
	UINT16 result = DrvInputs[real_port];
	UINT16 fake = DrvInputs[fake_port];

	if (fake & 0x01)			// up
	{
		if (fake & 0x04)		// up and left
			result &= ~0x2000;
		else if (fake & 0x08)	// up and right
			result &= ~0x1000;
		else					// up only
			result &= ~0x3000;
	}
	else if (fake & 0x02)		// down
	{
		if (fake & 0x04)		// down and left
			result &= ~0x8000;
		else if (fake & 0x08)	// down and right
			result &= ~0x4000;
		else					// down only
			result &= ~0xc000;
	}
	else if (fake & 0x04)		// left only
		result &= ~0x6000;
	else if (fake & 0x08)		// right only
		result &= ~0x9000;

	return result;
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
		DrvInputs[1] = 0xfffe;
		DrvInputs[2] = 0xffff;
		DrvInputs[3] = 0x0040;
		DrvInputs[4] = 0x0000; // fake udlr
		DrvInputs[5] = 0x0000; // fake udlr

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5f[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6f[i] & 1) << i;
		}

		DrvInputs[0] = udlr(0, 4);
		DrvInputs[1] = udlr(1, 5);

		DrvInputs[1] = (DrvInputs[1] & ~2) | (DrvDips[0] & 2);

		atarijsa_input_port = DrvInputs[3];
		atarijsa_test_mask = 0x02;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(7159090 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;
	DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		scanline = i;

		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

		if ((i & 0x3f) == 0x3f) partial_update_sprite(i); // sprite update every 64 lines
		if ((i % 8) == 0) scanline_update(); // every 8th line

		if (i == 239) {
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut) {
			INT32 nSegment = nBurnSoundLen / nInterleave;
			AtariJSAUpdate(pBurnSoundOut + (nSoundBufferPos << 1), nSegment);
			nSoundBufferPos += nSegment;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegment = nBurnSoundLen - nSoundBufferPos;
		if (nSegment > 0) {
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

		AtariJSAScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(playfield_tile_bank);
		SCAN_VAR(mob_scroll_x);
		SCAN_VAR(mob_scroll_y);
		SCAN_VAR(bg_scroll_x);
		SCAN_VAR(bg_scroll_y);
		SCAN_VAR(scanline_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Vindicators (rev 5)

static struct BurnRomInfo vindictrRomDesc[] = {
	{ "136059-5117.d1",				0x10000, 0x2e5135e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-5118.d3",				0x10000, 0xe357fa79, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-5119.f1",				0x10000, 0x0deb7330, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-5120.f3",				0x10000, 0xa6ae4753, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-5121.k1",				0x10000, 0x96b150c5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-5122.k3",				0x10000, 0x6415d312, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1123.16n",			0x04000, 0xf99b631a, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictr)
STD_ROM_FN(vindictr)

struct BurnDriver BurnDrvVindictr = {
	"vindictr", NULL, NULL, NULL, "1988",
	"Vindicators (rev 5)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictrRomInfo, vindictrRomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};


// Vindicators (Europe, rev 5)

static struct BurnRomInfo vindictreRomDesc[] = {
	{ "136059-5717.d1",				0x10000, 0xaf5ba4a8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-5718.d3",				0x10000, 0xc87b0581, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-5719.f1",				0x10000, 0x1e5f94e1, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-5720.f3",				0x10000, 0xcace40d7, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-5721.k1",				0x10000, 0x96b150c5, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-5722.k3",				0x10000, 0x6415d312, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1123.16n",			0x04000, 0xf99b631a, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictre)
STD_ROM_FN(vindictre)

struct BurnDriver BurnDrvVindictre = {
	"vindictre", "vindictr", NULL, NULL, "1988",
	"Vindicators (Europe, rev 5)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictreRomInfo, vindictreRomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};


// Vindicators (German, rev 1)

static struct BurnRomInfo vindictrgRomDesc[] = {
	{ "136059-1217.d1",				0x10000, 0x0a589e9a, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-1218.d3",				0x10000, 0xe8b7959a, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-1219.f1",				0x10000, 0x2534fcbc, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-1220.f3",				0x10000, 0xd0947780, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-1221.k1",				0x10000, 0xee1b1014, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-1222.k3",				0x10000, 0x517b33f0, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1223.16n",			0x04000, 0xd27975bb, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictrg)
STD_ROM_FN(vindictrg)

struct BurnDriver BurnDrvVindictrg = {
	"vindictrg", "vindictr", NULL, NULL, "1988",
	"Vindicators (German, rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictrgRomInfo, vindictrgRomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};


// Vindicators (Europe, rev 4)

static struct BurnRomInfo vindictre4RomDesc[] = {
	{ "136059-1117.d1",				0x10000, 0x2e5135e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-1118.d3",				0x10000, 0xe357fa79, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-4719.f1",				0x10000, 0x3b27ab80, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-4720.f3",				0x10000, 0xe5ac9933, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-4121.k1",				0x10000, 0x9a0444ee, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-4122.k3",				0x10000, 0xd5022d78, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1123.16n",			0x04000, 0xf99b631a, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictre4)
STD_ROM_FN(vindictre4)

struct BurnDriver BurnDrvVindictre4 = {
	"vindictre4", "vindictr", NULL, NULL, "1988",
	"Vindicators (Europe, rev 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictre4RomInfo, vindictre4RomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};


// Vindicators (rev 4)

static struct BurnRomInfo vindictr4RomDesc[] = {
	{ "136059-1117.d1",				0x10000, 0x2e5135e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-1118.d3",				0x10000, 0xe357fa79, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-4119.f1",				0x10000, 0x44c77ee0, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-4120.f3",				0x10000, 0x4deaa77f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-4121.k1",				0x10000, 0x9a0444ee, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-4122.k3",				0x10000, 0xd5022d78, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1123.16n",			0x04000, 0xf99b631a, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictr4)
STD_ROM_FN(vindictr4)

struct BurnDriver BurnDrvVindictr4 = {
	"vindictr4", "vindictr", NULL, NULL, "1988",
	"Vindicators (rev 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictr4RomInfo, vindictr4RomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};


// Vindicators (Europe, rev 3)

static struct BurnRomInfo vindictre3RomDesc[] = {
	{ "136059-3117.d1",				0x10000, 0xaf5ba4a8, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-3118.d3",				0x10000, 0xc87b0581, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-3119.f1",				0x10000, 0xf0516142, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-3120.f3",				0x10000, 0x32a3729f, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-2121.k1",				0x10000, 0x9b6111e0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-2122.k3",				0x10000, 0x8d029a28, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1123.16n",			0x04000, 0xf99b631a, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictre3)
STD_ROM_FN(vindictre3)

struct BurnDriver BurnDrvVindictre3 = {
	"vindictre3", "vindictr", NULL, NULL, "1988",
	"Vindicators (Europe, rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictre3RomInfo, vindictre3RomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};


// Vindicators (rev 2)

static struct BurnRomInfo vindictr2RomDesc[] = {
	{ "136059-1117.d1",				0x10000, 0x2e5135e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-1118.d3",				0x10000, 0xe357fa79, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-2119.f1",				0x10000, 0x7f8c044e, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-2120.f3",				0x10000, 0x4260cd3b, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-2121.k1",				0x10000, 0x9b6111e0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-2122.k3",				0x10000, 0x8d029a28, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1123.16n",			0x04000, 0xf99b631a, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictr2)
STD_ROM_FN(vindictr2)

struct BurnDriver BurnDrvVindictr2 = {
	"vindictr2", "vindictr", NULL, NULL, "1988",
	"Vindicators (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictr2RomInfo, vindictr2RomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};


// Vindicators (rev 1)

static struct BurnRomInfo vindictr1RomDesc[] = {
	{ "136059-1117.d1",				0x10000, 0x2e5135e4, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136059-1118.d3",				0x10000, 0xe357fa79, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-1119.f1",				0x10000, 0x48938c95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-1120.f3",				0x10000, 0xed1de5e3, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-1121.k1",				0x10000, 0x9b6111e0, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-1122.k3",				0x10000, 0xa94773f1, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "136059-1124.2k",				0x10000, 0xd2212c0a, 2 | BRF_PRG | BRF_ESS }, //  6 M6502 Code (JSA)

	{ "136059-1104.12p",			0x20000, 0x062f8e52, 3 | BRF_GRA },           //  7 Background Tiles and Sprites
	{ "136059-1116.19p",			0x10000, 0x0e4366fa, 3 | BRF_GRA },           //  8
	{ "136059-1103.8p",				0x20000, 0x09123b57, 3 | BRF_GRA },           //  9
	{ "136059-1115.2p",				0x10000, 0x6b757bca, 3 | BRF_GRA },           // 10
	{ "136059-1102.12r",			0x20000, 0xa5268c4f, 3 | BRF_GRA },           // 11
	{ "136059-1114.19r",			0x10000, 0x609f619e, 3 | BRF_GRA },           // 12
	{ "136059-1101.8r",				0x20000, 0x2d07fdaa, 3 | BRF_GRA },           // 13
	{ "136059-1113.2r",				0x10000, 0x0a2aba63, 3 | BRF_GRA },           // 14

	{ "136059-1123.16n",			0x04000, 0xf99b631a, 4 | BRF_GRA },           // 15 Characters

	{ "pal16l8a-136059-1150.c3",	0x00104, 0x09d02b00, 5 | BRF_OPT },           // 16 PLDs
	{ "pal16l8a-136059-1151.d17",	0x00104, 0x797dcde7, 5 | BRF_OPT },           // 17
	{ "pal16l8a-136059-1152.e17",	0x00104, 0x56634c58, 5 | BRF_OPT },           // 18
	{ "pal16r6a-136059-1153.n7",	0x00104, 0x61076033, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(vindictr1)
STD_ROM_FN(vindictr1)

struct BurnDriver BurnDrvVindictr1 = {
	"vindictr1", "vindictr", NULL, NULL, "1988",
	"Vindicators (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, vindictr1RomInfo, vindictr1RomName, NULL, NULL, NULL, NULL, VindictrInputInfo, VindictrDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x4000,
	336, 240, 4, 3
};
