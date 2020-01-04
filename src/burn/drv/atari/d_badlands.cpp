// FB Alpha Bad Lands driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
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
static UINT8 *DrvPalRAM;
static UINT8 *DrvPfRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvM6502RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 playfield_bank;
static INT32 video_int_state;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static INT16 DrvAnalogPortA = 0;
static INT16 DrvAnalogPortB = 0;

static UINT8 pedal[2];

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo BadlandsInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy3 + 1,	"p1 coin"	},
	{"P1 Pedal",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Start / Fire",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},
	A("P1 Steering",        BIT_ANALOG_REL, &DrvAnalogPortA,"p1 x-axis"),

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy3 + 0,	"p2 coin"	},
	{"P2 Pedal",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 fire 1"	},
	{"P2 Start / Fire",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 fire 2"	},
	A("P2 Steering",        BIT_ANALOG_REL, &DrvAnalogPortB,"p2 x-axis"),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Freeze (Service)",	BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A

STDINPUTINFO(Badlands)

static struct BurnDIPInfo BadlandsDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x80, NULL				},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x0a, 0x01, 0x80, 0x80, "Off"				},
	{0x0a, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Badlands)

static void update_interrupts()
{
	INT32 interrupt = 0;
	if (video_int_state) interrupt = 1;
	if (atarijsa_int_state) interrupt = 2;

	if (interrupt) 
		SekSetIRQLine(interrupt, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void __fastcall badlands_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xfffc00) == 0xfff000) {
		*((UINT16*)(Drv68KRAM + (address & 0xffe))) = data;
		if ((address & 0x200) == 0) {
			AtariMoExpandedWrite(0, (address / 2) & 0xff, data);
		}
		return;
	}

	if ((address & 0xffffc00) == 0xffc000) {
		DrvPalRAM[(address / 2) & 0x1ff] = data >> 8;
		return;
	}

	switch (address & ~0x1fff)
	{
		case 0xfc0000:
			AtariJSAResetWrite(0);
		return;

		case 0xfe0000:
			BurnWatchdogWrite();
		return;

		case 0xfe2000:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0xfe8000:
			AtariJSAWrite(data>>8);
		return;

		case 0xfee000:
			AtariEEPROMUnlockWrite();
		return;

		case 0xfec000:
			playfield_bank = data & 1;
		return;
	}
}

static void __fastcall badlands_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xfffc00) == 0xfff000) {
		Drv68KRAM[(address & 0xfff)^1] = data;
		if ((address & 0x200) == 0) {
			AtariMoExpandedWrite(0, (address / 2) & 0xff, *((UINT16*)(Drv68KRAM + (address & 0xffe))));
		}
		return;
	}

	if ((address & 0xffffc00) == 0xffc000) {
		if ((address & 1) == 0) DrvPalRAM[(address / 2) & 0x1ff] = data;
		return;
	}

	switch (address & ~0x1fff)
	{
		case 0xfc0000:
			AtariJSAResetWrite(0);
		return;

		case 0xfe0000:
			BurnWatchdogWrite();
		return;

		case 0xfe2000:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0xfe8000:
			AtariJSAWrite(data);
		return;

		case 0xfee000:
			AtariEEPROMUnlockWrite();
		return;

		case 0xfec000:
			playfield_bank = data & 1;
		return;
	}
}

static UINT16 __fastcall badlands_main_read_word(UINT32 address)
{
	if ((address & 0xffffc00) == 0xffc000) {
		return DrvPalRAM[(address / 2) & 0x1ff] << 8;
	}

	switch (address & ~0x1fff)
	{
		case 0xfc0000:
		{
			UINT16 temp = 0xfeff;
			if (atarigen_cpu_to_sound_ready) temp ^= 0x0100;
			return temp;
		}

		case 0xfe4000:
			return (DrvInputs[0] ^ (vblank ? 0x40 : 0));

		case 0xfe6000:
		{
			switch (address & 6)
			{
				default:
				case 0x0:
					return 0xff00 | (BurnTrackballRead(0, 0) & 0xff); // fe6000

				case 0x2:
					return 0xff00 | (BurnTrackballRead(0, 1) & 0xff); // fe6002

				case 0x4:
					return pedal[0];

				case 0x6:
					return pedal[1];
			}
		}

		case 0xfea000:
			return AtariJSARead() << 8;
	}

	return 0;
}

static UINT8 __fastcall badlands_main_read_byte(UINT32 address)
{
	if ((address & 0xffffc00) == 0xffc000) {
		return DrvPalRAM[(address / 2) & 0x1ff];
	}

	switch (address & ~0x1fff)
	{
		case 0xfc0000:
		{
			UINT16 temp = 0xfeff;
			if (atarigen_cpu_to_sound_ready) temp ^= 0x0100;
			return temp >> ((~address & 1) * 8);
		}

		case 0xfe4000:
			return (DrvInputs[0] ^ (vblank ? 0x40 : 0)) >> ((~address & 1) * 8);

		case 0xfe6000:
		{
			switch (address & 6)
			{
				default:
				case 0x0:
					return (0xff00 | (BurnTrackballRead(0, 0) & 0xff)) >> ((~address & 1) * 8); // fe6000

				case 0x2:
					return (0xff00 | (BurnTrackballRead(0, 1) & 0xff)) >> ((~address & 1) * 8); // fe6002

				case 0x4:
					return pedal[0] >> ((~address & 1) * 8);

				case 0x6:
					return pedal[1] >> ((~address & 1) * 8);
			}
		}

		case 0xfea000:
			return AtariJSARead();
	}

	return 0;
}
		
static tilemap_callback( bg )
{
	UINT16 data = *((UINT16*)(DrvPfRAM + offs * 2));
	INT32 code = (data & 0x1fff);
	if (code & 0x1000) code += (playfield_bank * 0x1000);

	TILE_SET_INFO(0, code, data >> 13, 0);
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	AtariJSAReset();
	AtariEEPROMReset();

	BurnWatchdogReset();

	playfield_bank = 0;

	pedal[0] = pedal[1] = 0x80;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x040000;
	DrvM6502ROM			= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x100000;
	DrvGfxROM1			= Next; Next += 0x080000;

	DrvPalette			= (UINT32*)Next; Next += 0x100 * sizeof(UINT32);

	AllRam				= Next;

	DrvM6502RAM			= Next; Next += 0x002000;

	DrvPalRAM			= Next; Next += 0x000400;
	DrvPfRAM			= Next; Next += 0x001000;
	Drv68KRAM			= Next; Next += 0x001000;

	atarimo_0_slipram 	= NULL;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { STEP4(0,1) };
	INT32 XOffs0[8] = { STEP8(0,4) };
	INT32 YOffs0[8] = { STEP8(0,32) };

	INT32 Plane1[4] = { STEP4(0,1) };
	INT32 XOffs1[16]= { STEP16(0,4) };
	INT32 YOffs1[8] = { STEP8(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x60000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x60000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x3000, 4,  8, 8, Plane0, XOffs0, YOffs0, 0x100, tmp, DrvGfxROM0);

	for (INT32 i = 0; i < 0x30000; i++) tmp[i] = DrvGfxROM1[i] ^ 0xff;

	GfxDecode(0x0c00, 4, 16, 8, Plane1, XOffs1, YOffs1, 0x200, tmp, DrvGfxROM1);

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
		1,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		0,					// pixels per SLIP entry (0 for no-slip)
		0,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x80,				// base palette entry
		0x80,				// maximum number of colors
		0,					// transparent pen index

		{{ 0x1f }},			// mask for the link
		{{ 0 }},			// mask for the graphics bank
		{{ 0x0fff,0,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0,0,0x0007 }},	// mask for the color
		{{ 0,0,0,0xff80 }},	// mask for the X position
		{{ 0,0xff80,0,0 }},	// mask for the Y position
		{{ 0 }},			// mask for the width, in tiles*/
		{{ 0,0x000f,0,0 }},	// mask for the height, in tiles
		{{ 0 }},			// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0,0,0,0x0008 }},	// mask for the priority
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
		if (BurnLoadRom(Drv68KROM  + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x020000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x050000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x020000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,					0x000000, 0x03ffff, MAP_ROM);
//	SekMapMemory(DrvPalRAM,					0xffc000, 0xffc3ff, MAP_ROM); // 0xff00 write!
	SekMapMemory(DrvPfRAM,					0xffe000, 0xffefff, MAP_RAM);
	SekMapMemory(Drv68KRAM,					0xfff000, 0xfff3ff, MAP_ROM); // 0-1ff mob, 200-3ff 68kram
	SekMapMemory(Drv68KRAM + 0x400,			0xfff400, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,				badlands_main_write_word);
	SekSetWriteByteHandler(0,				badlands_main_write_byte);
	SekSetReadWordHandler(0,				badlands_main_read_word);
	SekSetReadByteHandler(0,				badlands_main_read_byte);

	AtariEEPROMInit(0x2000);
	AtariEEPROMInstallMap(1,				0xfd0000, 0xfd1fff);
	SekClose();

	AtariJSAInit(DrvM6502ROM, &update_interrupts, NULL, NULL);

	BurnWatchdogInit(DrvDoReset, 180);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 64, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 4,  8, 8, 0x100000, 0x000, 0x07);
	GenericTilemapSetGfx(1, DrvGfxROM1, 4, 16, 8, 0x080000, 0x080, 0x07);

	AtariMoInit(0, &modesc);

	BurnTrackballInit(2);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	AtariMoExit();
	AtariJSAExit();
	AtariEEPROMExit();

	BurnFree(AllMem);

	BurnTrackballExit();

	return 0;
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x200/2; i++)
	{
		UINT16 d = (p[i] << 8) | (p[i] >> 8);
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
				if ((mo[x] & 0xf000) || !(pf[x] & 8)) {
					pf[x] = mo[x] & 0x0ff;
				}
				mo[x] = 0xffff;
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force!!
	}

	AtariMoRender(0);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) copy_sprites();

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
		DrvInputs[0] = 0xff3f | (DrvDips[0] & 0x80);
		DrvInputs[1] = 0x0000;
		DrvInputs[2] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2];
		atarijsa_test_mask = 0x80;
		atarijsa_test_port = (DrvDips[0] & atarijsa_test_mask);

		{
			// Using trackball device as 2 steering wheel paddles
			BurnTrackballConfig(0, AXIS_NORMAL, AXIS_NORMAL);
			BurnTrackballFrame(0, DrvAnalogPortA, DrvAnalogPortB, 0x01, 0x06);
			BurnTrackballUpdate(0);
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
		CPU_RUN(0, Sek);
		CPU_RUN(1, M6502);

		if (i == 239) {
			vblank = 1;

			video_int_state = 1;
			update_interrupts();

			for (INT32 p = 0; p < 2; p++) {
				pedal[p]--;
				if (DrvInputs[1] & (1 << p))
					pedal[p]++;
			}

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

		AtariMoScan(nAction, pnMin);
		AtariJSAScan(nAction, pnMin);

		BurnWatchdogScan(nAction);
		BurnTrackballScan();

		SCAN_VAR(playfield_bank);
		SCAN_VAR(video_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Bad Lands

static struct BurnRomInfo badlandsRomDesc[] = {
	{ "136074-1008.20f",	0x10000, 0xa3da5774, 1 | BRF_PRG | BRF_ESS }, //  0 M68000 Code
	{ "136074-1006.27f",	0x10000, 0xaa03b4f3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136074-1009.17f",	0x10000, 0x0e2e807f, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136074-1007.24f",	0x10000, 0x99a20c2c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136074-1018.9c",		0x10000, 0xa05fd146, 2 | BRF_PRG | BRF_ESS }, //  4 M6502 Code

	{ "136074-1012.4n",		0x10000, 0x5d124c6c, 3 | BRF_GRA },           //  5 Background Tiles
	{ "136074-1013.2n",		0x10000, 0xb1ec90d6, 3 | BRF_GRA },           //  6
	{ "136074-1014.4s",		0x10000, 0x248a6845, 3 | BRF_GRA },           //  7
	{ "136074-1015.2s",		0x10000, 0x792296d8, 3 | BRF_GRA },           //  8
	{ "136074-1016.4u",		0x10000, 0x878f7c66, 3 | BRF_GRA },           //  9
	{ "136074-1017.2u",		0x10000, 0xad0071a3, 3 | BRF_GRA },           // 10

	{ "136074-1010.14r",	0x10000, 0xc15f629e, 4 | BRF_GRA },           // 11 Sprites
	{ "136074-1011.10r",	0x10000, 0xfb0b6717, 4 | BRF_GRA },           // 12
	{ "136074-1019.14t",	0x10000, 0x0e26bff6, 4 | BRF_GRA },           // 13

	{ "136074-1001.26c",	0x00117, 0x04c3be6a, 5 | BRF_OPT },           // 14 PLDs
	{ "136074-1002.21r",	0x00117, 0xf68bf41d, 5 | BRF_OPT },           // 15
	{ "136074-1003.16s",	0x00117, 0xa288bbd0, 5 | BRF_OPT },           // 16
	{ "136074-1004.9n",		0x00117, 0x5ffbdaad, 5 | BRF_OPT },           // 17
	{ "136074-1005.12e",	0x00117, 0x9df77c79, 5 | BRF_OPT },           // 18
	{ "136074-2000.26r",	0x00117, 0xfb8fb3d0, 5 | BRF_OPT },           // 19
};

STD_ROM_PICK(badlands)
STD_ROM_FN(badlands)

struct BurnDriver BurnDrvBadlands = {
	"badlands", NULL, NULL, NULL, "1989",
	"Bad Lands\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, badlandsRomInfo, badlandsRomName, NULL, NULL, NULL, NULL, BadlandsInputInfo, BadlandsDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	336, 240, 4, 3
};
