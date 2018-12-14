// FB Alpha Atari Blasteroids driver module
// Based on MAME driver by Aaron Giles

// this game is very picky regarding timing!

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "burn_pal.h"
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
static UINT8 *DrvPfRAM;
static UINT8 *DrvMobRAM;
static UINT8 *DrvPriRAM;
static UINT8 *Drv68KRAM;

static UINT8 DrvRecalc;

static INT32 scanline_int_state;
static INT32 video_int_state;
static INT32 cpu_halted;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvJoy4f[8];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static UINT8 TrackA;
static UINT8 TrackB;

static UINT32 linecycles;

static struct BurnInputInfo BlstroidInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},

	{"P1 Left",			BIT_DIGITAL,	DrvJoy4f + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy4f + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 fire 4"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy4f + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4f + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 4"	},

	{"P3 Coin",			BIT_DIGITAL,	DrvJoy3 + 2,	"p3 coin"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Blstroid)

static struct BurnDIPInfo BlstroidDIPList[]=
{
	{0x10, 0xff, 0xff, 0x80, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x10, 0x01, 0x80, 0x80, "Off"			},
	{0x10, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Blstroid)

static void update_interrupts()
{
	INT32 state = 0;
	if (scanline_int_state) state = 1;
	if (video_int_state) state = 2;
	if (atarijsa_int_state) state = 4;

	if (state) {
		SekSetIRQLine(state, CPU_IRQSTATUS_ACK);
	} else {
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
	}
}

static void __fastcall blstroid_main_write_word(UINT32 address, UINT16 data)
{
	if (address & 0x7f8000) {
		SekWriteWord(address & 0x807fff, data);
		return;
	}

	if ((address & 0xfff000) == 0x805000) {
		*((UINT16*)(DrvMobRAM + (address & 0xffe))) = data;
		AtariMoWrite(0, (address / 2) & 0x7ff, data);
		return;
	}

	if ((address & 0xfffe00) == 0x800800) {
		*((UINT16*)(DrvPriRAM + (address & 0x1fe))) = data;
		return;
	}
	
	switch (address)
	{
		case 0x800000:
			BurnWatchdogWrite();
		return;

		case 0x800200:
			scanline_int_state = 0;
			update_interrupts();
		return;

		case 0x800400:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0x800600:
		case 0x800601:
			AtariEEPROMUnlockWrite();
		return;

		case 0x800a00:
			AtariJSAWrite(data);
		return;

		case 0x800c00:
			AtariJSAResetWrite(0);
		return;

		case 0x800e00:
			cpu_halted = 1;
		return;
	}

	bprintf (0, _T("MW: %5.5x, %4.4x\n"), address, data);
}

static void __fastcall blstroid_main_write_byte(UINT32 address, UINT8 data)
{
	if (address & 0x7f8000) {
		SekWriteByte(address & 0x807fff, data);
		return;
	}

	if ((address & 0xfff000) == 0x805000) {
		DrvMobRAM[(address & 0xfff)^1] = data;
		AtariMoWrite(0, (address / 2) & 0x7ff, *((UINT16*)(DrvMobRAM + (address & 0xffe))));
		return;
	}

	if ((address & 0xfffe00) == 0x800800) {
		DrvPriRAM[(address & 0x1ff)^1] = data;
		return;
	}
	
	switch (address)
	{
		case 0x800000:
		case 0x800001:
			BurnWatchdogWrite();
		return;

		case 0x800200:
		case 0x800201:
			scanline_int_state = 0;
			update_interrupts();
		return;

		case 0x800400:
		case 0x800401:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0x800600:
		case 0x800601:
			AtariEEPROMUnlockWrite();
		return;

		case 0x800a00:
		case 0x800a01:
			AtariJSAWrite(data);
		return;

		case 0x800c00:
		case 0x800c01:
			AtariJSAResetWrite(0);
		return;

		case 0x800e00:
		case 0x800e01:
			cpu_halted = 1;
		return;
	}

	bprintf (0, _T("MB: %5.5x, %2.2x\n"), address, data);
}

static inline UINT16 input_read(INT32 select)
{
	UINT16 ret = DrvInputs[select] & ~0x30;

	if (SekTotalCycles() - linecycles > 410) ret ^= 0x10;
	if (vblank) ret ^= 0x20;
	if (atarigen_cpu_to_sound_ready) ret ^= 0x40;

	return ret;
}

static UINT16 __fastcall blstroid_main_read_word(UINT32 address)
{
	if (address & 0x7f8000) {
		return SekReadWord(address & 0x807fff);
	}

	if ((address & ~0x0383ff) == 0x801c00) address &= ~0x0383fc; // unmask masked input derp

	switch (address & 0x807fff)
	{
		case 0x801400:
			return AtariJSARead();

		case 0x801800:
			return 0xff00 | TrackA;

		case 0x801804:
			return 0xff00 | TrackB;

		case 0x801c00:
		case 0x801c02:
			return input_read((address / 2) & 1);
	}

	bprintf (0, _T("RW: %5.5x\n"), address);

	return 0;
}

static UINT8 __fastcall blstroid_main_read_byte(UINT32 address)
{
	if (address & 0x7f8000) {
		return SekReadByte(address & 0x807fff);
	}

	if ((address & ~0x0383ff) == 0x801c00) address &= ~0x0383fc; // unmask masked input derp

	switch (address & 0x807fff)
	{
		case 0x801400:
		case 0x801401:
			return AtariJSARead() >> ((~address & 1) * 8);

		case 0x801800:
		case 0x801801:
			return (0xff00 | TrackA) >> ((~address & 1) * 8);

		case 0x801804:
		case 0x801805:
			return (0xff00 | TrackB) >> ((~address & 1) * 8);

		case 0x801c00:
		case 0x801c01:
		case 0x801c02:
		case 0x801c03:
			return (input_read((address / 2) & 1) >> ((~address & 1) * 8)) & 0xff;
	}

	bprintf (0, _T("RB: %5.5x\n"), address);

	return 0;
}

static tilemap_callback( bg )
{
	UINT16 data = *((UINT16*)(DrvPfRAM + offs * 2));

	TILE_SET_INFO(0, data, data >> 13, 0);
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

	AtariJSAReset();
	AtariEEPROMReset();

	scanline_int_state = 0;
	video_int_state = 0;
	cpu_halted = 0;
	TrackA = 0;
	TrackB = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x040000;
	DrvM6502ROM			= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x100000;
	DrvGfxROM1			= Next; Next += 0x200000;

	BurnPalette			= (UINT32*)Next; Next += 0x200 * sizeof(UINT32);

	AllRam				= Next;

	BurnPalRAM			= Next; Next += 0x000400;
	DrvPfRAM			= Next; Next += 0x001000;
	DrvMobRAM			= Next; Next += 0x001000;
	Drv68KRAM			= Next; Next += 0x002000;

	DrvPriRAM			= Next; Next += 0x000200;

	atarimo_0_slipram 	= NULL;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0,1) };
	INT32 XOffs0[16] = { 0,0, 4,4, 8,8, 12,12, 16,16, 20,20, 24,24, 28,28 };
	INT32 YOffs0[8] = { STEP8(0,32) };

	INT32 half = (0x100000/2) * 8;
	INT32 Plane1[4] = { STEP4(0,1) };
	INT32 XOffs1[16] = { half+0, half+4, 0, 4, half+8, half+12, 8, 12,
			half+16, half+20, 16, 20, half+24, half+28, 24, 28 };
	INT32 YOffs1[8] = { STEP8(0,32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x100000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x040000);

	GfxDecode(0x2000, 4, 16, 8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x4000, 4, 16, 8, Plane1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		1,					/* index to which gfx system */
		1,					/* number of motion object banks */
		1,					/* are the entries linked? */
		0,					/* are the entries split? */
		0,					/* render in reverse order? */
		0,					/* render in swapped X/Y order? */
		0,					/* does the neighbor bit affect the next object? */
		0,					/* pixels per SLIP entry (0 for no-slip) */
		0,					/* pixel offset for SLIPs */
		0,					/* maximum number of links to visit/scanline (0=all) */

		0x000,				/* base palette entry */
		0x100,				/* maximum number of colors */
		0,					/* transparent pen index */

		{{ 0,0,0x0ff8,0 }},	/* mask for the link */
		{{ 0 }},			/* mask for the graphics bank */
		{{ 0,0x3fff,0,0 }},	/* mask for the code index */
		{{ 0 }},			/* mask for the upper code index */
		{{ 0,0,0,0x000f }},	/* mask for the color */
		{{ 0,0,0,0xffc0 }},	/* mask for the X position */
		{{ 0xff80,0,0,0 }},	/* mask for the Y position */
		{{ 0 }},			/* mask for the width, in tiles*/
		{{ 0x000f,0,0,0 }},	/* mask for the height, in tiles */
		{{ 0,0x8000,0,0 }},	/* mask for the horizontal flip */
		{{ 0,0x4000,0,0 }},	/* mask for the vertical flip */
		{{ 0 }},			/* mask for the priority */
		{{ 0 }},			/* mask for the neighbor */
		{{ 0 }},			/* mask for absolute coordinates */

		{{ 0 }},			/* mask for the special value */
		0,					/* resulting value to indicate "special" */
		0					/* callback routine for special entries */
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
		if (BurnLoadRom(DrvGfxROM0 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x030000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x070000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x090000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0b0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0d0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0e0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x0f0000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	AtariEEPROMInit(0x400);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x03ffff, MAP_ROM);
	for (INT32 i = 0; i < 0x1000; i+= 0x400) {
		SekMapMemory(BurnPalRAM,	0x802000 + i, 0x8023ff + i, MAP_RAM);
		AtariEEPROMInstallMap(1, 	0x803000 + i, 0x8033ff + i);
	}
	SekMapMemory(DrvPfRAM,			0x804000, 0x804fff, MAP_RAM);
	SekMapMemory(DrvMobRAM,			0x805000, 0x805fff, MAP_ROM);
	SekMapMemory(Drv68KRAM,			0x806000, 0x807fff, MAP_RAM);
	SekSetWriteWordHandler(0,		blstroid_main_write_word);
	SekSetWriteByteHandler(0,		blstroid_main_write_byte);
	SekSetReadWordHandler(0,		blstroid_main_read_word);
	SekSetReadByteHandler(0,		blstroid_main_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, NULL, NULL);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 16, 8, 64, 64);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 16, 8, 0x100000, 0x100, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 8, 0x200000, 0x000, 0x0f);

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

static void copy_sprites()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	UINT16 *pri = (UINT16*)DrvPriRAM;

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				INT32 priaddr = ((pf[x] & 8) << 4) | (pf[x] & 0x70) | ((mo[x] & 0xf0) >> 4);
				if (pri[priaddr] & 1)
					pf[x] = mo[x];
				mo[x] = 0xffff; // clear
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		BurnPaletteUpdate_xRRRRRGGGGGBBBBB();;
		DrvRecalc = 1; // force!!
	}

	AtariMoRender(0);

	BurnTransferClear();

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) copy_sprites();

	BurnTransferCopy(BurnPalette);

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
		DrvInputs[0] = 0xff7f | (DrvDips[0] & 0x80);
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2] & 0xff;
		atarijsa_test_mask = 0x80;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;

		{
			if (DrvJoy4f[0]) TrackA -= 4;
			if (DrvJoy4f[1]) TrackA += 4;

			if (DrvJoy4f[2]) TrackB -= 4;
			if (DrvJoy4f[3]) TrackB += 4;
		}

	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { (INT32)(7159090 / 59.92), (INT32)(1789773 / 59.92) };
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;
	for (INT32 i = 0; i < nInterleave; i++)
	{
		linecycles = SekTotalCycles();

		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);

		if (i == 247) {
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

		SCAN_VAR(video_int_state);
		SCAN_VAR(scanline_int_state);
		SCAN_VAR(cpu_halted);

		SCAN_VAR(TrackA);
		SCAN_VAR(TrackB);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Blasteroids (rev 4)

static struct BurnRomInfo blstroidRomDesc[] = {
	{ "136057-4123.6c",		0x10000, 0xd14badc4, 1 | BRF_PRG | BRF_ESS }, //  0 M68K Code
	{ "136057-4121.6b",		0x10000, 0xae3e93e8, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136057-4124.4c",		0x10000, 0xfd2365df, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136057-4122.4b",		0x10000, 0xc364706e, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136057-1135.2k",		0x10000, 0xbaa8b5fe, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136057-1101.1l",		0x10000, 0x3c2daa5b, 3 | BRF_GRA },           //  5 Background Layer
	{ "136057-1102.1m",		0x10000, 0xf84f0b97, 3 | BRF_GRA },           //  6
	{ "136057-1103.3l",		0x10000, 0xae5274f0, 3 | BRF_GRA },           //  7
	{ "136057-1104.3m",		0x10000, 0x4bb72060, 3 | BRF_GRA },           //  8

	{ "136057-1105.5m",		0x10000, 0x50e0823f, 4 | BRF_GRA },           //  9 Sprites
	{ "136057-1107.67m",	0x10000, 0x729de7a9, 4 | BRF_GRA },           // 10
	{ "136057-1109.8m",		0x10000, 0x090e42ab, 4 | BRF_GRA },           // 11
	{ "136057-1111.10m",	0x10000, 0x1ff79e67, 4 | BRF_GRA },           // 12
	{ "136057-1113.11m",	0x10000, 0x4be1d504, 4 | BRF_GRA },           // 13
	{ "136057-1115.13m",	0x10000, 0xe4409310, 4 | BRF_GRA },           // 14
	{ "136057-1117.14m",	0x10000, 0x7aaca15e, 4 | BRF_GRA },           // 15
	{ "136057-1119.16m",	0x10000, 0x33690379, 4 | BRF_GRA },           // 16
	{ "136057-1106.5n",		0x10000, 0x2720ee71, 4 | BRF_GRA },           // 17
	{ "136057-1108.67n",	0x10000, 0x2faecd15, 4 | BRF_GRA },           // 18
	{ "136057-1110.8n",		0x10000, 0xa15e79e1, 4 | BRF_GRA },           // 19
	{ "136057-1112.10n",	0x10000, 0x4d5fc284, 4 | BRF_GRA },           // 20
	{ "136057-1114.11n",	0x10000, 0xa70fc6e6, 4 | BRF_GRA },           // 21
	{ "136057-1116.13n",	0x10000, 0xf423b4f8, 4 | BRF_GRA },           // 22
	{ "136057-1118.14n",	0x10000, 0x56fa3d16, 4 | BRF_GRA },           // 23
	{ "136057-1120.16n",	0x10000, 0xf257f738, 4 | BRF_GRA },           // 24
};

STD_ROM_PICK(blstroid)
STD_ROM_FN(blstroid)

struct BurnDriver BurnDrvBlstroid = {
	"blstroid", NULL, NULL, NULL, "1987",
	"Blasteroids (rev 4)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, blstroidRomInfo, blstroidRomName, NULL, NULL, NULL, NULL, BlstroidInputInfo, BlstroidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	640, 240, 4, 3
};


// Blasteroids (rev 3)

static struct BurnRomInfo blstroid3RomDesc[] = {
	{ "136057-3123.6c",		0x10000, 0x8fb050f5, 1 | BRF_PRG | BRF_ESS }, //  0 M68K Code
	{ "136057-3121.6b",		0x10000, 0x21fae262, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136057-3124.4c",		0x10000, 0xa9140c31, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136057-3122.4b",		0x10000, 0x137fbb17, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136057-1135.2k",		0x10000, 0xbaa8b5fe, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136057-1101.1l",		0x10000, 0x3c2daa5b, 3 | BRF_GRA },           //  5 Background Layer
	{ "136057-1102.1m",		0x10000, 0xf84f0b97, 3 | BRF_GRA },           //  6
	{ "136057-1103.3l",		0x10000, 0xae5274f0, 3 | BRF_GRA },           //  7
	{ "136057-1104.3m",		0x10000, 0x4bb72060, 3 | BRF_GRA },           //  8

	{ "136057-1105.5m",		0x10000, 0x50e0823f, 4 | BRF_GRA },           //  9 Sprites
	{ "136057-1107.67m",	0x10000, 0x729de7a9, 4 | BRF_GRA },           // 10
	{ "136057-1109.8m",		0x10000, 0x090e42ab, 4 | BRF_GRA },           // 11
	{ "136057-1111.10m",	0x10000, 0x1ff79e67, 4 | BRF_GRA },           // 12
	{ "136057-1113.11m",	0x10000, 0x4be1d504, 4 | BRF_GRA },           // 13
	{ "136057-1115.13m",	0x10000, 0xe4409310, 4 | BRF_GRA },           // 14
	{ "136057-1117.14m",	0x10000, 0x7aaca15e, 4 | BRF_GRA },           // 15
	{ "136057-1119.16m",	0x10000, 0x33690379, 4 | BRF_GRA },           // 16
	{ "136057-1106.5n",		0x10000, 0x2720ee71, 4 | BRF_GRA },           // 17
	{ "136057-1108.67n",	0x10000, 0x2faecd15, 4 | BRF_GRA },           // 18
	{ "136057-1110.8n",		0x10000, 0xa15e79e1, 4 | BRF_GRA },           // 19
	{ "136057-1112.10n",	0x10000, 0x4d5fc284, 4 | BRF_GRA },           // 20
	{ "136057-1114.11n",	0x10000, 0xa70fc6e6, 4 | BRF_GRA },           // 21
	{ "136057-1116.13n",	0x10000, 0xf423b4f8, 4 | BRF_GRA },           // 22
	{ "136057-1118.14n",	0x10000, 0x56fa3d16, 4 | BRF_GRA },           // 23
	{ "136057-1120.16n",	0x10000, 0xf257f738, 4 | BRF_GRA },           // 24
};

STD_ROM_PICK(blstroid3)
STD_ROM_FN(blstroid3)

struct BurnDriver BurnDrvBlstroid3 = {
	"blstroid3", "blstroid", NULL, NULL, "1987",
	"Blasteroids (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, blstroid3RomInfo, blstroid3RomName, NULL, NULL, NULL, NULL, BlstroidInputInfo, BlstroidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	640, 240, 4, 3
};


// Blasteroids (rev 2)

static struct BurnRomInfo blstroid2RomDesc[] = {
	{ "136057-2123.6c",		0x10000, 0x5a092513, 1 | BRF_PRG | BRF_ESS }, //  0 M68K Code
	{ "136057-2121.6b",		0x10000, 0x486aac51, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136057-2124.4c",		0x10000, 0xd0fa38fe, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136057-2122.4b",		0x10000, 0x744bf921, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136057-1135.2k",		0x10000, 0xbaa8b5fe, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136057-1101.1l",		0x10000, 0x3c2daa5b, 3 | BRF_GRA },           //  5 Background Layer
	{ "136057-1102.1m",		0x10000, 0xf84f0b97, 3 | BRF_GRA },           //  6
	{ "136057-1103.3l",		0x10000, 0xae5274f0, 3 | BRF_GRA },           //  7
	{ "136057-1104.3m",		0x10000, 0x4bb72060, 3 | BRF_GRA },           //  8

	{ "136057-1105.5m",		0x10000, 0x50e0823f, 4 | BRF_GRA },           //  9 Sprites
	{ "136057-1107.67m",	0x10000, 0x729de7a9, 4 | BRF_GRA },           // 10
	{ "136057-1109.8m",		0x10000, 0x090e42ab, 4 | BRF_GRA },           // 11
	{ "136057-1111.10m",	0x10000, 0x1ff79e67, 4 | BRF_GRA },           // 12
	{ "136057-1113.11m",	0x10000, 0x4be1d504, 4 | BRF_GRA },           // 13
	{ "136057-1115.13m",	0x10000, 0xe4409310, 4 | BRF_GRA },           // 14
	{ "136057-1117.14m",	0x10000, 0x7aaca15e, 4 | BRF_GRA },           // 15
	{ "136057-1119.16m",	0x10000, 0x33690379, 4 | BRF_GRA },           // 16
	{ "136057-1106.5n",		0x10000, 0x2720ee71, 4 | BRF_GRA },           // 17
	{ "136057-1108.67n",	0x10000, 0x2faecd15, 4 | BRF_GRA },           // 18
	{ "136057-1110.8n",		0x10000, 0xa15e79e1, 4 | BRF_GRA },           // 19
	{ "136057-1112.10n",	0x10000, 0x4d5fc284, 4 | BRF_GRA },           // 20
	{ "136057-1114.11n",	0x10000, 0xa70fc6e6, 4 | BRF_GRA },           // 21
	{ "136057-1116.13n",	0x10000, 0xf423b4f8, 4 | BRF_GRA },           // 22
	{ "136057-1118.14n",	0x10000, 0x56fa3d16, 4 | BRF_GRA },           // 23
	{ "136057-1120.16n",	0x10000, 0xf257f738, 4 | BRF_GRA },           // 24
};

STD_ROM_PICK(blstroid2)
STD_ROM_FN(blstroid2)

struct BurnDriver BurnDrvBlstroid2 = {
	"blstroid2", "blstroid", NULL, NULL, "1987",
	"Blasteroids (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, blstroid2RomInfo, blstroid2RomName, NULL, NULL, NULL, NULL, BlstroidInputInfo, BlstroidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	640, 240, 4, 3
};


// Blasteroids (German, rev 2)

static struct BurnRomInfo blstroidgRomDesc[] = {
	{ "136057-2223.6c",		0x10000, 0xcc82108b, 1 | BRF_PRG | BRF_ESS }, //  0 M68K Code
	{ "136057-2221.6b",		0x10000, 0x84822e68, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136057-2224.4c",		0x10000, 0x849249d4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136057-2222.4b",		0x10000, 0xbdeaba0d, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136057-1135.2k",		0x10000, 0xbaa8b5fe, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136057-1101.1l",		0x10000, 0x3c2daa5b, 3 | BRF_GRA },           //  5 Background Layer
	{ "136057-1102.1m",		0x10000, 0xf84f0b97, 3 | BRF_GRA },           //  6
	{ "136057-1103.3l",		0x10000, 0xae5274f0, 3 | BRF_GRA },           //  7
	{ "136057-1104.3m",		0x10000, 0x4bb72060, 3 | BRF_GRA },           //  8

	{ "136057-1105.5m",		0x10000, 0x50e0823f, 4 | BRF_GRA },           //  9 Sprites
	{ "136057-1107.67m",	0x10000, 0x729de7a9, 4 | BRF_GRA },           // 10
	{ "136057-1109.8m",		0x10000, 0x090e42ab, 4 | BRF_GRA },           // 11
	{ "136057-1111.10m",	0x10000, 0x1ff79e67, 4 | BRF_GRA },           // 12
	{ "136057-1113.11m",	0x10000, 0x4be1d504, 4 | BRF_GRA },           // 13
	{ "136057-1115.13m",	0x10000, 0xe4409310, 4 | BRF_GRA },           // 14
	{ "136057-1117.14m",	0x10000, 0x7aaca15e, 4 | BRF_GRA },           // 15
	{ "136057-1119.16m",	0x10000, 0x33690379, 4 | BRF_GRA },           // 16
	{ "136057-1106.5n",		0x10000, 0x2720ee71, 4 | BRF_GRA },           // 17
	{ "136057-1108.67n",	0x10000, 0x2faecd15, 4 | BRF_GRA },           // 18
	{ "136057-1110.8n",		0x10000, 0xa15e79e1, 4 | BRF_GRA },           // 19
	{ "136057-1112.10n",	0x10000, 0x4d5fc284, 4 | BRF_GRA },           // 20
	{ "136057-1114.11n",	0x10000, 0xa70fc6e6, 4 | BRF_GRA },           // 21
	{ "136057-1116.13n",	0x10000, 0xf423b4f8, 4 | BRF_GRA },           // 22
	{ "136057-1118.14n",	0x10000, 0x56fa3d16, 4 | BRF_GRA },           // 23
	{ "136057-1120.16n",	0x10000, 0xf257f738, 4 | BRF_GRA },           // 24
};

STD_ROM_PICK(blstroidg)
STD_ROM_FN(blstroidg)

struct BurnDriver BurnDrvBlstroidg = {
	"blstroidg", "blstroid", NULL, NULL, "1987",
	"Blasteroids (German, rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, blstroidgRomInfo, blstroidgRomName, NULL, NULL, NULL, NULL, BlstroidInputInfo, BlstroidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	640, 240, 4, 3
};


// Blasteroids (with heads)

static struct BurnRomInfo blstroidhRomDesc[] = {
	{ "eheadh0.c6",			0x10000, 0x061f0898, 1 | BRF_PRG | BRF_ESS }, //  0 M68K Code
	{ "eheadl0.b6",			0x10000, 0xae8df7cb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "eheadh1.c5",			0x10000, 0x0b7a3cb6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "eheadl1.b5",			0x10000, 0x43971694, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136057-1135.2k",		0x10000, 0xbaa8b5fe, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136057-1101.1l",		0x10000, 0x3c2daa5b, 3 | BRF_GRA },           //  5 Background Layer
	{ "136057-1102.1m",		0x10000, 0xf84f0b97, 3 | BRF_GRA },           //  6
	{ "136057-1103.3l",		0x10000, 0xae5274f0, 3 | BRF_GRA },           //  7
	{ "136057-1104.3m",		0x10000, 0x4bb72060, 3 | BRF_GRA },           //  8

	{ "136057-1105.5m",		0x10000, 0x50e0823f, 4 | BRF_GRA },           //  9 Sprites
	{ "136057-1107.67m",	0x10000, 0x729de7a9, 4 | BRF_GRA },           // 10
	{ "136057-1109.8m",		0x10000, 0x090e42ab, 4 | BRF_GRA },           // 11
	{ "136057-1111.10m",	0x10000, 0x1ff79e67, 4 | BRF_GRA },           // 12
	{ "mol4.m12",			0x10000, 0x571139ea, 4 | BRF_GRA },           // 13
	{ "136057-1115.13m",	0x10000, 0xe4409310, 4 | BRF_GRA },           // 14
	{ "136057-1117.14m",	0x10000, 0x7aaca15e, 4 | BRF_GRA },           // 15
	{ "mol7.m16",			0x10000, 0xd27b2d91, 4 | BRF_GRA },           // 16
	{ "136057-1106.5n",		0x10000, 0x2720ee71, 4 | BRF_GRA },           // 17
	{ "136057-1108.67n",	0x10000, 0x2faecd15, 4 | BRF_GRA },           // 18
	{ "moh2.n8",			0x10000, 0xa15e79e1, 4 | BRF_GRA },           // 19
	{ "136057-1112.10n",	0x10000, 0x4d5fc284, 4 | BRF_GRA },           // 20
	{ "moh4.n12",			0x10000, 0x1a74e960, 4 | BRF_GRA },           // 21
	{ "136057-1116.13n",	0x10000, 0xf423b4f8, 4 | BRF_GRA },           // 22
	{ "136057-1118.14n",	0x10000, 0x56fa3d16, 4 | BRF_GRA },           // 23
	{ "moh7.n16",			0x10000, 0xa93cbbe7, 4 | BRF_GRA },           // 24
};

STD_ROM_PICK(blstroidh)
STD_ROM_FN(blstroidh)

struct BurnDriver BurnDrvBlstroidh = {
	"blstroidh", "blstroid", NULL, NULL, "1987",
	"Blasteroids (with heads)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, blstroidhRomInfo, blstroidhRomName, NULL, NULL, NULL, NULL, BlstroidInputInfo, BlstroidDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	640, 240, 4, 3
};
