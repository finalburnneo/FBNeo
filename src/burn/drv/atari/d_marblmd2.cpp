// FB Neo Marble Madness 2 driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarivad.h"
#include "atarijsa.h"
#include "burn_gun.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvPalRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvM6502RAM;
static UINT8 *DrvMOBRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 sound_cpu_halt;
static INT32 scanline_int_state;
static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4[16];
static UINT16 DrvInputs[4];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo Marblmd2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy4 + 0,	"p1 coin"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy3 + 7,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy3 + 6,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy3 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 1"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy4 + 1,	"p2 coin"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 1"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy4 + 2,	"p3 coin"	},
	{"P3 Up",			BIT_DIGITAL,	DrvJoy3 + 11,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy3 + 10,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy3 + 9,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy3 + 8,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p3 fire 1"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Marblmd2)

static struct BurnDIPInfo Marblmd2DIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0xff, NULL								},
	{0x01, 0xff, 0xff, 0x40, NULL								},

	{0   , 0xfe, 0   ,    2, "Unknown"							},
	{0x00, 0x01, 0x01, 0x01, "Off"								},
	{0x00, 0x01, 0x01, 0x00, "On"								},

	{0   , 0xfe, 0   ,    2, "Unknown"							},
	{0x00, 0x01, 0x02, 0x02, "Off"								},
	{0x00, 0x01, 0x02, 0x00, "On"								},

	{0   , 0xfe, 0   ,    2, "Unknown"							},
	{0x00, 0x01, 0x04, 0x04, "Off"								},
	{0x00, 0x01, 0x04, 0x00, "On"								},

	{0   , 0xfe, 0   ,    2, "Unknown"							},
	{0x00, 0x01, 0x08, 0x08, "Off"								},
	{0x00, 0x01, 0x08, 0x00, "On"								},

	{0   , 0xfe, 0   ,    2, "Unknown"							},
	{0x00, 0x01, 0x10, 0x10, "Off"								},
	{0x00, 0x01, 0x10, 0x00, "On"								},

	{0   , 0xfe, 0   ,    2, "Number of Players (Test Mode)"	},
	{0x00, 0x01, 0x20, 0x00, "2"								},
	{0x00, 0x01, 0x20, 0x20, "3"								},

	{0   , 0xfe, 0   ,    2, "Unknown"							},
	{0x00, 0x01, 0x40, 0x40, "Off"								},
	{0x00, 0x01, 0x40, 0x00, "On"								},

	{0   , 0xfe, 0   ,    2, "Number of Players (Game)"			},
	{0x00, 0x01, 0x80, 0x00, "2"								},
	{0x00, 0x01, 0x80, 0x80, "3"								},

	{0   , 0xfe, 0   ,    2, "Service Mode"						},
	{0x01, 0x01, 0x40, 0x40, "Off"								},
	{0x01, 0x01, 0x40, 0x00, "On"								},
};

STDDIPINFO(Marblmd2)

static void update_interrupts()
{
	INT32 interrupt = 0;
	if (scanline_int_state) interrupt |= 4;
	if (atarijsa_int_state)	interrupt |= 6;

	if (interrupt)
		SekSetIRQLine(interrupt, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void latch_write(UINT16 data)
{
	sound_cpu_halt = ~data & 0x10;

	if (sound_cpu_halt)
	{
		M6502Reset();
	}
}

static void __fastcall marblmd2_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffffc00) == 0x7c0000) {
		DrvPalRAM[(address / 2) & 0x1ff] = data >> 8;
		return;
	}

	if ((address & 0xffe000) == 0x7da000) {
		*((UINT16*)(DrvMOBRAM + (address & 0x1ffe))) = BURN_ENDIAN_SWAP_INT16(data);
		AtariMoWrite(0, (address / 2) & 0xfff, data);
		return;
	}

	switch (address)
	{
		case 0x600040:
			AtariJSAWrite(data);
		return;

		case 0x600050:
			latch_write(data);
		return;

		case 0x600060:
			AtariEEPROMUnlockWrite();
		return;

		case 0x607000:
		return; // ?
	}

	bprintf (0, _T("Missed!\n"));
}

static void __fastcall marblmd2_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffffc00) == 0x7c0000) {
		if ((address&1)==0) DrvPalRAM[(address / 2) & 0x1ff] = data;
		return;
	}

	if ((address & 0xffe000) == 0x7da000 ) {
		DrvMOBRAM[(address & 0x1fff)^1] = data;
		AtariMoWrite(0, (address / 2) & 0xfff, BURN_ENDIAN_SWAP_INT16(*((UINT16*)(DrvMOBRAM + (address & 0x1ffe)))));
		return;
	}

	switch (address)
	{
		case 0x600040:
		case 0x600041:
			AtariJSAWrite(data);
		return;

		case 0x600050:
		case 0x600051:
			latch_write(data);
		return;

		case 0x600060:
		case 0x600061:
			AtariEEPROMUnlockWrite();
		return;

		case 0x7c0400:
		case 0x7c0402:
		return; // palette overwrite
	}
}

static UINT16 __fastcall marblmd2_main_read_word(UINT32 address)
{
	if ((address & 0xfffc00) == 0x7c0000) {
		return DrvPalRAM[(address / 2) & 0x1ff];
	}

	switch (address)
	{
		case 0x600000:
			return DrvInputs[0];

		case 0x600002:
			return DrvInputs[1];

		case 0x600010:
		{
			UINT16 ret = 0xffbf;
			if (atarigen_sound_to_cpu_ready) ret ^= 0x0010;
			if (atarigen_cpu_to_sound_ready) ret ^= 0x0020;
			ret ^= (DrvDips[1] & 0x40);
			ret ^= (vblank ? 0x80 : 0);
			return ret;
		}

		case 0x600012:
			return DrvDips[0] | 0xff00;

		case 0x600020:
			return DrvInputs[2];

		case 0x600030:
			return AtariJSARead();
	}

	return 0;
}

static UINT8 __fastcall marblmd2_main_read_byte(UINT32 address)
{
	if ((address & 0xffffc00) == 0x7c0000) {
		return DrvPalRAM[(address / 2) & 0x1ff];
	}

	return marblmd2_main_read_word(address & ~1) >> ((~address & 1) * 8);
}

static void scanline_timer(INT32 state)
{
	scanline_int_state = state;

	update_interrupts();
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
	AtariVADReset();

	sound_cpu_halt = 0;
	scanline_int_state = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x0080000;
	DrvM6502ROM			= Next; Next += 0x0100000;

	DrvGfxROM0			= Next; Next += 0x1000000;
	DrvGfxROM1			= Next; Next += 0x1000000;

	DrvSndROM			= Next; Next += 0x0080000;

	DrvPalette			= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam				= Next;

	DrvM6502RAM			= Next; Next += 0x002000;

	DrvPalRAM			= Next; Next += 0x000200;
	Drv68KRAM			= Next; Next += 0x010000;
	DrvMOBRAM			= Next; Next += 0x002000;

	atarimo_0_slipram 	= (UINT16*)(DrvMOBRAM + 0x1f80);

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[8] = { STEP8(0,1) };
	INT32 XOffs0[8] = { 0, (0x80000*8)+0, 8, (0x80000*8)+8, 16, (0x80000*8)+16, 24, (0x80000*8)+24 };
	INT32 YOffs0[8] = { STEP8(0,32) };

	INT32 Plane1[4] = { STEP4(0,1) };
	INT32 XOffs1[8] = { 0x40000*8+0, 0x40000*8+4, 0, 4, 0x40000*8+8, 0x40000*8+12, 8, 12 };
	INT32 YOffs1[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x100000);

	GfxDecode(0x4000, 8, 8, 8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x080000);

	GfxDecode(0x4000, 4, 8, 8, Plane1, XOffs1, YOffs1, 0x080, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		1,					// index to which gfx system
		1,					// number of motion object banks
		1,					// are the entries linked?
		0,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		8,					// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x000,				// base palette entry
		0x100,				// maximum number of colors
		0,					// transparent pen index

		{{ 0x3ff,0,0,0 }},	// mask for the link
		{{ 0 }},			// mask for the graphics bank		- not used
		{{ 0,0x7fff,0,0 }},	// mask for the code index					- iq_132 - shouldn't this be 4000-1?
		{{ 0 }},			// mask for the upper code index	- not used
		{{ 0,0,0x000f,0 }},	// mask for the color
		{{ 0,0,0xff80,0 }},	// mask for the X position
		{{ 0,0,0,0xff80 }},	// mask for the Y position
		{{ 0,0,0,0x0070 }},	// mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	// mask for the height, in tiles
		{{ 0,0x8000,0,0 }},	// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0,0,0x0070,0 }},	// mask for the priority
		{{ 0 }},			// mask for the neighbor
		{{ 0 }},			// mask for absolute coordinates

		{{ 0 }},			// mask for the special value
		0,					// resulting value to indicate "special"
		0					// callback routine for special entries
	};

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(Drv68KROM  + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x040001, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, k++, 1)) return 1;

		if (BurnLoadRom(DrvSndROM  + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvSndROM  + 0x060000, k++, 1)) return 1;

		// fix white start screen
		*((UINT16*)(Drv68KROM + 0xa6c)) = 0x6000;	// beq a86 -> bra a86

		DrvGfxDecode();
	}

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 8, 8, 8, 0x100000, 0x000, 0x1);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 8, 8, 0x100000, 0x000, 0xf);

	AtariVADInit(0, 1, 1, scanline_timer, NULL);
	AtariVADSetXOffsets(4, 4, 0);
	AtariMoInit(0, &modesc);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,					0x000000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRAM + 0xc000,		0x601000, 0x6013ff, MAP_RAM);
//	SekMapMemory(DrvPalRAM,					0x7c0000, 0x7c03ff, MAP_ROM); // 0xff00 write!
	SekMapMemory(Drv68KRAM + 0x4000,		0x7d0000, 0x7d7fff, MAP_RAM);
	SekMapMemory(DrvMOBRAM,					0x7da000, 0x7dbfff, MAP_ROM); // 0-1eff mob, f00-fff slip, etc
	SekMapMemory(Drv68KRAM + 0x0000,		0x7f8000, 0x7fbfff, MAP_RAM);
	SekSetWriteWordHandler(0,				marblmd2_main_write_word);
	SekSetWriteByteHandler(0,				marblmd2_main_write_byte);
	SekSetReadWordHandler(0,				marblmd2_main_read_word);
	SekSetReadByteHandler(0,				marblmd2_main_read_byte);

	AtariVADMap(0x7c0000, 0x7c0fff, 2);	// starts at 7c0000, but doesn't actually map over

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1, 0x601000, 0x601fff);
	AtariEEPROMLoad(Drv68KRAM); // temp 0'd memory
	SekClose();

	AtariJSAInit(DrvM6502ROM, &update_interrupts, DrvSndROM, NULL);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	AtariVADExit();
	AtariMoExit();
	AtariJSAExit();
	AtariEEPROMExit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x200/2; i++)
	{
		UINT16 d = BURN_ENDIAN_SWAP_INT16((p[i] << 8) | (p[i] >> 8));
		INT32 n = d >> 15;
		UINT8 r = ((d >> 9) & 0x3e) | n;
		UINT8 g = ((d >> 4) & 0x3e) | n;
		UINT8 b = ((d << 1) & 0x3e) | n;

		r = (r << 2) | (r >> 4);
		g = (g << 2) | (g >> 4);
		b = (b << 2) | (b >> 4);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void copy_pf_sprites()
{
	UINT16 *mo = BurnBitmapGetPosition(31, 0, 0);
	UINT16 *pf = BurnBitmapGetPosition(0, 0, 0);

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++)
	{
		int p = pTransDraw[i];
		pTransDraw[i] &= 0x7f;

		if (~nSpriteEnable & 1) continue;

		if (mo[i] != 0xffff)
		{
			pf[i] = p & 0x7f;

			if (p >= 0x80)
			{
				if (mo[i] >= 0x80) pf[i] = mo[i];
			}
			else
			{
				pf[i] = mo[i] | 0x80;
			}

			mo[i] = 0xffff;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force!!
	}

	BurnTransferClear();

	AtariMoRender(0);

	AtariVADDraw(pTransDraw, 0); // nBurnLayer 1,2 check is internal

	copy_pf_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();
	M6502NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));
		DrvInputs[3] = 0x0040; // jsa, active high

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[3];
		atarijsa_test_mask = 0x40;
		atarijsa_test_port = DrvDips[1] & atarijsa_test_mask;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(14318181 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		atarivad_scanline = i;

		if (i == 0) AtariVADEOFUpdate((UINT16*)(DrvMOBRAM + 0x1f00));

		AtariVADTimerUpdate();

		CPU_RUN(0, Sek);

		if (sound_cpu_halt == 0) {
			CPU_RUN(1, M6502);
		} else {
			CPU_IDLE(1, M6502);
		}

		if (i == 239) {
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && (i & 1)) {
			INT32 nSegment = nBurnSoundLen / (nInterleave / 2);
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
		AtariVADScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		SCAN_VAR(sound_cpu_halt);
		SCAN_VAR(scanline_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Marble Madness II (prototype)

static struct BurnRomInfo marblmd2RomDesc[] = {
	{ "rom0l.18c",	0x20000, 0xa4db40d9, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "rom0h.20c",	0x20000, 0xd1a17d67, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "rom1l.18e",	0x20000, 0xb6fb08b5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "rom1h.20e",	0x20000, 0xb2a361a8, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "aud0.12c",	0x10000, 0x89a8d90a, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "pf0l.3p",	0x20000, 0xa4fe377a, 3 | BRF_GRA },           //  5 Background Tiles
	{ "pf1l.3m",	0x20000, 0x5dc7aaa8, 3 | BRF_GRA },           //  6
	{ "pf2l.3k",	0x20000, 0x0c7c5f74, 3 | BRF_GRA },           //  7
	{ "pf3l.3j",	0x20000, 0x0a780429, 3 | BRF_GRA },           //  8
	{ "pf0h.1p",	0x20000, 0xa6297a83, 3 | BRF_GRA },           //  9
	{ "pf1h.1m",	0x20000, 0x5b40f1bb, 3 | BRF_GRA },           // 10
	{ "pf2h.1k",	0x20000, 0x18323df9, 3 | BRF_GRA },           // 11
	{ "pf3h.1j",	0x20000, 0x05d86ef8, 3 | BRF_GRA },           // 12

	{ "mo0l.7p",	0x20000, 0x950d95a3, 4 | BRF_GRA },           // 13 Sprites
	{ "mo1l.10p",	0x20000, 0xb62b6ebf, 4 | BRF_GRA },           // 14
	{ "mo0h.12p",	0x20000, 0xe47d92b0, 4 | BRF_GRA },           // 15
	{ "mo1h.14p",	0x20000, 0x317a03fb, 4 | BRF_GRA },           // 16

	{ "sound.19e",	0x20000, 0xe916bef7, 5 | BRF_GRA },           // 17 Samples
	{ "sound.12e",	0x20000, 0xbab2f8e5, 5 | BRF_GRA },           // 18
};

STD_ROM_PICK(marblmd2)
STD_ROM_FN(marblmd2)

struct BurnDriver BurnDrvMarblmd2 = {
	"marblmd2", NULL, NULL, NULL, "1991",
	"Marble Madness II (prototype)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_MISC, 0,
	NULL, marblmd2RomInfo, marblmd2RomName, NULL, NULL, NULL, NULL, Marblmd2InputInfo, Marblmd2DIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	336, 240, 4, 3
};
