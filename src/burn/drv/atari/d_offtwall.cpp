// FB Alpha Atari Off the Wall driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarivad.h"
#include "atarijsa.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvMobRAM;
static UINT8 *Drv68KRAM;
static UINT16 *DrvEOFData;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 soundcpu_halted;
static INT32 bank_offset;
static INT32 scanline_int_state;

static UINT32 bankswitch_address_hi;
static UINT32 bankswitch_address_lo;
static UINT32 unknown_prot_address;
static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy4[16];
static UINT16 DrvInputs[4];
static UINT8 DrvDips[1];
static UINT8 DrvReset;

static struct BurnInputInfo OfftwallInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy4 + 1,	"p1 coin"	},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy4 + 0,	"p2 coin"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 15,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 14,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 13,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 12,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 10,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 4"	},
//	analog placeholder
	{"P1 Button 5",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 5"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 7,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 2,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 fire 3"	},
	{"P2 Button 4",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 fire 4"	},
//	analog placeholder
	{"P2 Button 5",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 fire 5"	},

	{"P3 Up",			BIT_DIGITAL,	DrvJoy2 + 15,	"p3 up"		},
	{"P3 Down",			BIT_DIGITAL,	DrvJoy2 + 14,	"p3 down"	},
	{"P3 Left",			BIT_DIGITAL,	DrvJoy2 + 13,	"p3 left"	},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy2 + 12,	"p3 right"	},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy2 + 9,	"p3 fire 1"	},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy2 + 10,	"p3 fire 2"	},
	{"P3 Button 3",		BIT_DIGITAL,	DrvJoy2 + 11,	"p3 fire 3"	},
	{"P3 Button 4",		BIT_DIGITAL,	DrvJoy2 + 8,	"p3 fire 4"	},
//	analog placeholder
	{"P3 Button 5",		BIT_DIGITAL,	DrvJoy2 + 0,	"p3 fire 5"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Tilt",			BIT_DIGITAL,	DrvJoy4 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Offtwall)

static struct BurnDIPInfo OfftwallDIPList[]=
{
	{0x1f, 0xff, 0xff, 0x42, NULL				},

	{0   , 0xfe, 0   ,    2, "Controls"			},
	{0x1f, 0x01, 0x02, 0x00, "Whirly-gigs"		},
	{0x1f, 0x01, 0x02, 0x02, "Joysticks"		},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x1f, 0x01, 0x40, 0x40, "Off"				},
	{0x1f, 0x01, 0x40, 0x00, "On"				},
};

STDDIPINFO(Offtwall)

static void update_interrupts()
{
	INT32  newstate = 0;
	if (scanline_int_state) newstate = 4;
	if (atarijsa_int_state)	newstate = 6;

	if (newstate)
		SekSetIRQLine(newstate, CPU_IRQSTATUS_ACK);
	else
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
}

static void __fastcall offtwall_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xf00000) == 0x300000) {
		SekWriteWord(address | 0x400000, data);
		return;
	}

	if ((address & 0xfff800) == 0x7fd000) {
		*((UINT16*)(DrvMobRAM + (address & 0x7fe))) = data;
		AtariMoWrite(0, (address & 0x7fe)/2, data);
		return;
	}

	switch (address)
	{
		case 0x260040:
		case 0x260041:
			AtariJSAWrite(data);
		return;

		case 0x260050:
		case 0x260051:
			soundcpu_halted = ~data & 0x10;
			if (soundcpu_halted) AtariJSAReset();
		return;

		case 0x260060:
		case 0x260061:
			AtariEEPROMUnlockWrite();
		return;

		case 0x2a0000:
		case 0x2a0001:
			BurnWatchdogWrite();
		return;
	}
}

static void __fastcall offtwall_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xf00000) == 0x300000) {
		SekWriteByte(address | 0x400000, data);
		return;
	}

	if ((address & 0xfff800) == 0x7fd000) {
		DrvMobRAM[(address & 0x7ff)^1] = data;
		AtariMoWrite(0, (address & 0x7fe)/2, *((UINT16*)(DrvMobRAM + (address & 0x7fe))));
		return;
	}

	switch (address)
	{
		case 0x260040:
		case 0x260041:
			AtariJSAWrite(data);
		return;

		case 0x260050:
		case 0x260051:
			soundcpu_halted = ~data & 0x10;
			if (soundcpu_halted) AtariJSAReset();
		return;

		case 0x260060:
		case 0x260061:
			AtariEEPROMUnlockWrite();
		return;

		case 0x2a0000:
		case 0x2a0001:
			BurnWatchdogWrite();
		return;
	}
}

static inline UINT16 special_read()
{
	UINT16 ret = DrvInputs[2];

	if (atarigen_cpu_to_sound_ready) ret ^= 0x20;
	if (vblank) ret ^= 0x80;

	return ret;
}

static inline UINT16 checksum_read(INT32 offset)
{
	UINT32 checksum = 0xaaaa5555 - ((SekReadWord(0x7fd210) << 16) | SekReadWord(0x7fd212));
	if (offset) return checksum;
	return checksum >> 16;
}

static inline UINT16 spritecache_count_read()
{
	INT32 prevpc = SekGetPC(-1);
	INT32 oldword = *((UINT16*)(DrvMobRAM + 0xe42));

	if (prevpc == 0x99f8 || prevpc == 0x9992)
	{
		UINT16 *data = (UINT16*)(DrvMobRAM + 0xc42);
		INT32 count = oldword >> 8;
		INT32 width = 0;

		for (INT32 i = 0; i < count; i++)
			width += 1 + ((data[i * 4 + 1] >> 4) & 7);

		if (width <= 38)
		{
			while (width <= 38)
			{
				data[count * 4 + 0] = (42 * 8) << 7;
				data[count * 4 + 1] = ((30 * 8) << 7) | (7 << 4);
				data[count * 4 + 2] = 0;
				width += 8;
				count++;
			}

			*((UINT16*)(DrvMobRAM + 0xe42)) = (count << 8) | (oldword & 0xff);
		}
	}

	return oldword;
}

static UINT16 __fastcall offtwall_read_word(UINT32 address)
{
	if ((address & 0xff8000) == 0x030000) {
		if (address >= bankswitch_address_lo && address <= bankswitch_address_hi) {
			bank_offset = ((address - bankswitch_address_lo) / 2) & 3;
		}

		return *((UINT16*)(Drv68KROM + (address & 0x3fffe)));
	}

	if ((address & 0xff8000) == 0x038000) {
		if ((address & 0x7ffc) == 0x6000) {
			return checksum_read(address & 2);
		}
		INT32 offset = (bank_offset * 0x2000) + (address & 0x1ffe);
		return *((UINT16*)(Drv68KROM + offset));
	}

	if ((address & ~1) == unknown_prot_address) {
		UINT16 ret = SekReadWord(0x400000 | unknown_prot_address);
		UINT32 pc = SekGetPC(-1);
		if (pc < 0x5c5e || pc > 0xc432)
			return ret;
		else
			return ret | 0x100;
	}

	if ((address & ~1) == 0x3fde42) {
		return spritecache_count_read();
	}

	if ((address & 0xf00000) == 0x300000) {
		return SekReadWord(address | 0x400000);
	}

	switch (address)
	{
		case 0x260000:
		case 0x260001:
			return DrvInputs[0];

		case 0x260002:
		case 0x260003:
			return DrvInputs[1];

		case 0x260010:
		case 0x260011:
			return special_read();

		case 0x260012:
		case 0x260013:
			return 0xffff;

		case 0x260020:
		case 0x260021:
			return 0xff00; // analog

		case 0x260022:
		case 0x260023:
			return 0xff00; // analog

		case 0x260024:
		case 0x260025:
			return 0xff00; // analog

		case 0x260030:
		case 0x260031:
			return AtariJSARead() & 0xff;
	}

	return 0;
}

static UINT8 __fastcall offtwall_read_byte(UINT32 address)
{
	return offtwall_read_word(address) >> ((~address & 1) * 8);
}

static void scanline_timer(INT32 state)
{
	scanline_int_state = state;

	update_interrupts();
}

static void palette_write(INT32 offset, UINT16 data)
{
	UINT8 i = data >> 15;
	UINT8 r = ((data >> 9) & 0x3e) | i;
	UINT8 g = ((data >> 4) & 0x3e) | i;
	UINT8 b = ((data << 1) & 0x3e) | i;

	r = (r << 2) | (r >> 4);
	g = (g << 2) | (g >> 4);
	b = (b << 2) | (b >> 4);

	DrvPalette[offset] = BurnHighCol(r,g,b,0);
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
	AtariVADReset();

	soundcpu_halted = 0;
	scanline_int_state = 0;
	bank_offset = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM			= Next; Next += 0x040000;
	DrvM6502ROM			= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x200000;

	DrvPalette			= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam				= Next;

	DrvMobRAM			= Next; Next += 0x000800;
	Drv68KRAM			= Next; Next += 0x008000;

	// keep in this order!! we overflow these in the frame
	atarimo_0_slipram 	= (UINT16*)Next; Next += 0x00080;
	DrvEOFData			= (UINT16*)Next; Next += 0x00080;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { 0x060000*8+0, 0x060000*8+4, 0, 4 };
	INT32 XOffs0[8] = { STEP4(0,1), STEP4(8,1) };
	INT32 YOffs0[8] = { STEP8(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0xc0000);
	if (tmp == NULL) {
		return 1;
	}

	for (INT32 i = 0; i < 0x0c0000; i++) tmp[i] = DrvGfxROM0[i];

	GfxDecode(0x6000, 4, 8, 8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	BurnFree(tmp);

	return 0;
}

static INT32 DrvInit()
{
	static const struct atarimo_desc modesc =
	{
		0,					//  index to which gfx system
		1,					//  number of motion object banks
		1,					//  are the entries linked?
		0,					//  are the entries split?
		0,					//  render in reverse order?
		0,					//  render in swapped X/Y order?
		0,					//  does the neighbor bit affect the next object?
		8,					//  pixels per SLIP entry (0 for no-slip)
		0,					//  pixel offset for SLIPs
		0,					//  maximum number of links to visit/scanline (0=all)

		0x100,				//  base palette entry
		0x100,				//  maximum number of colors
		0,					//  transparent pen index

		{{ 0x00ff,0,0,0 }},	//  mask for the link
		{{ 0 }},			//  mask for the graphics bank
		{{ 0,0x7fff,0,0 }},	//  mask for the code index
		{{ 0 }},			//  mask for the upper code index
		{{ 0,0,0x000f,0 }},	//  mask for the color
		{{ 0,0,0xff80,0 }},	//  mask for the X position
		{{ 0,0,0,0xff80 }},	//  mask for the Y position
		{{ 0,0,0,0x0070 }},	//  mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	//  mask for the height, in tiles
		{{ 0,0x8000,0,0 }},	//  mask for the horizontal flip
		{{ 0 }},			//  mask for the vertical flip
		{{ 0 }},			//  mask for the priority
		{{ 0 }},			//  mask for the neighbor
		{{ 0 }},			//  mask for absolute coordinates

		{{ 0 }},			//  mask for the special value
		0,					//  resulting value to indicate "special"
		0					//  callback routine for special entries
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

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a0000, k++, 1)) return 1;

	//	if (BurnLoadRom(Drv68KRAM  + 0x000000, k++, 1)) return 1; // temp memory

		DrvGfxDecode();
	}

	GenericTilesInit();
	GenericTilemapSetGfx(0, DrvGfxROM0, 4, 8, 8, 0x180000, 0x200, 0x0f);
	GenericTilemapSetGfx(1, DrvGfxROM0, 4, 8, 8, 0x180000, 0x100, 0x0f);

	AtariVADInit(0, 1, 0, scanline_timer, palette_write);
	AtariMoInit(0, &modesc);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x02ffff, MAP_ROM);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1,		0x120000, 0x120fff);

	// map these at 7xxxxx rather than 3xxxxx so we can catch reads
	// and then pass them back through SekReadWord/Byte
	AtariVADMap(0x7e0000, 0x7f7fff, 1);
	SekMapMemory(Drv68KRAM,			0x7f8000, 0x7fffff, MAP_RAM);
	SekMapMemory(DrvMobRAM,			0x7fd000, 0x7fd7ff, MAP_ROM); // handler
	SekMapMemory(NULL,				0x7fd000, 0x7fd3ff, MAP_ROM); // unmap these!
	SekMapMemory(NULL,				0x7fd400, 0x7fd7ff, MAP_ROM);

	// slip = 0x7f5f80 
	// eof    = 7f5f00

	SekSetWriteWordHandler(0,		offtwall_write_word);
	SekSetWriteByteHandler(0,		offtwall_write_byte);
	SekSetReadWordHandler(0,		offtwall_read_word);
	SekSetReadByteHandler(0,		offtwall_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, NULL, NULL); // JSA3 !!!!

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	SekExit();

	AtariVADExit();
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

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				pf[x] = mo[x] & 0x7ff;
				mo[x] = 0xffff;
			}
		}
	}
}

static void draw_scanline(INT32 line)
{
	GenericTilesSetClip(-1, -1, line-1, line);

	for (INT32 i = 0; i < 0x80; i+=2) { // update slip, but not eof
		atarimo_0_slipram[i] = SekReadWord(0x7f5f00);
	}

	if (nSpriteEnable & 4) AtariMoRender(0);

	AtariVADDraw(pTransDraw, 0);

	if (nSpriteEnable & 1) copy_sprites();

	GenericTilesClearClip();
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		AtariVADRecalcPalette();
		DrvRecalc = 0;
	}

//	BurnTransferClear();

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
		DrvInputs[1] = 0xffff;
		DrvInputs[2] = 0xffbd | (DrvDips[0] & 0x42);
		DrvInputs[3] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy2[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[3];
		atarijsa_test_mask = 0x40;
		atarijsa_test_port = DrvDips[0] & atarijsa_test_mask;
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
//	INT32 nCyclesTotal[3] = { (INT32)(14318180 / 59.92), (INT32)(1789773 / 59.92) };

	SekOpen(0);
	M6502Open(0);

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		atarivad_scanline = i;

		if (i == 0) {
			for (INT32 j = 0; j < 0x100; j+=2) {
				atarimo_0_slipram[j/2] = SekReadWord(0x7f5f00);
			}
			AtariVADEOFUpdate(DrvEOFData);
		}

		if (atarivad_scanline_timer == atarivad_scanline) {
			scanline_timer(CPU_IRQSTATUS_ACK);
		}

		// beam active (2 instances for higher interleave)
		SekRun(336);
		if (soundcpu_halted == 0) {
			M6502Run(((SekTotalCycles() / 8) - M6502TotalCycles()));
		} else {
			M6502Idle(((SekTotalCycles() / 8) - M6502TotalCycles()));
		}
		SekRun(336);
		if (soundcpu_halted == 0) {
			M6502Run(((SekTotalCycles() / 8) - M6502TotalCycles()));
		} else {
			M6502Idle(((SekTotalCycles() / 8) - M6502TotalCycles()));
		}

		// hblank
		SekRun(240);
		if (soundcpu_halted == 0) {
			M6502Run(((SekTotalCycles() / 8) - M6502TotalCycles()));
		} else {
			M6502Idle(((SekTotalCycles() / 8) - M6502TotalCycles()));
		}

		if (i <= 240 && i > 0) {
			draw_scanline(i);
		}

		if (i == 239) {
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);

		if (pBurnSoundOut && (i & 1) == 1) {
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

		BurnWatchdogScan(nAction);

		SCAN_VAR(soundcpu_halted);
		SCAN_VAR(bank_offset);
		SCAN_VAR(scanline_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Off the Wall (2/3-player upright)

static struct BurnRomInfo offtwallRomDesc[] = {
	{ "136090-2012.17e",		0x20000, 0xd08d81eb, 1 | BRF_PRG | BRF_ESS }, //  0 68K Code
	{ "136090-2013.17j",		0x20000, 0x61c2553d, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "136090-1020.12c",		0x10000, 0x488112a5, 2 | BRF_PRG | BRF_ESS }, //  2 M6502 Code

	{ "136090-1014.14s",		0x20000, 0x4d64507e, 3 | BRF_GRA },           //  3 Graphics
	{ "136090-1016.14p",		0x20000, 0xf5454f3a, 3 | BRF_GRA },           //  4
	{ "136090-1018.14m",		0x20000, 0x17864231, 3 | BRF_GRA },           //  5
	{ "136090-1015.18s",		0x20000, 0x271f7856, 3 | BRF_GRA },           //  6
	{ "136090-1017.18p",		0x20000, 0x7f7f8012, 3 | BRF_GRA },           //  7
	{ "136090-1019.18m",		0x20000, 0x9efe511b, 3 | BRF_GRA },           //  8

	{ "offtwall-eeprom.17l",	0x00800, 0x5eaf2d5b, 4 | BRF_PRG | BRF_ESS }, //  9 Default EEPROM Data

	{ "136085-1038.17c.bin",	0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 10 JSA:PLDs
	{ "136085-1039.20c.bin",	0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 11
	{ "136090-1001.14l.bin",	0x0201d, 0x00000000, 6 | BRF_NODUMP | BRF_GRA },           // 12 Main:PLDs
	{ "136090-1002.11r.bin",	0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 13
	{ "136090-1003.15f.bin",	0x00117, 0x5e723b46, 5 | BRF_GRA },           // 14
	{ "136090-1005.5n.bin",		0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 15
	{ "136090-1006.5f.bin",		0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 16
	{ "136090-1007.3f.bin",		0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_GRA },           // 17
};

STD_ROM_PICK(offtwall)
STD_ROM_FN(offtwall)

static INT32 OfftwallInit()
{
	bankswitch_address_lo = 0x037ec2;
	bankswitch_address_hi = 0x037f39;
	unknown_prot_address = 0x3fdf1e;

	return DrvInit();
}

struct BurnDriverD BurnDrvOfftwall = {
	"offtwall", NULL, NULL, NULL, "1991",
	"Off the Wall (2/3-player upright)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, offtwallRomInfo, offtwallRomName, NULL, NULL, NULL, NULL, OfftwallInputInfo, OfftwallDIPInfo,
	OfftwallInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};


// Off the Wall (2-player cocktail)

static struct BurnRomInfo offtwallcRomDesc[] = {
	{ "090-2612.rom",			0x20000, 0xfc891a3f, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "090-2613.rom",			0x20000, 0x805d79d4, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "136090-1020.12c",		0x10000, 0x488112a5, 2 | BRF_PRG | BRF_ESS }, //  2 M6502 Code

	{ "090-1614.rom",			0x20000, 0x307ed447, 3 | BRF_GRA },           //  3 Graphics
	{ "090-1616.rom",			0x20000, 0xa5bd3d9b, 3 | BRF_GRA },           //  4
	{ "090-1618.rom",			0x20000, 0xc7d9df5d, 3 | BRF_GRA },           //  5
	{ "090-1615.rom",			0x20000, 0xac3642c7, 3 | BRF_GRA },           //  6
	{ "090-1617.rom",			0x20000, 0x15208a89, 3 | BRF_GRA },           //  7
	{ "090-1619.rom",			0x20000, 0x8a5d79b3, 3 | BRF_GRA },           //  8

	{ "offtwall-eeprom.17l",	0x00800, 0x5eaf2d5b, 4 | BRF_PRG | BRF_ESS }, //  9 Default EEPROM Data
};

STD_ROM_PICK(offtwallc)
STD_ROM_FN(offtwallc)

static INT32 OfftwallcInit()
{
	bankswitch_address_lo = 0x037eca;
	bankswitch_address_hi = 0x037f43;
	unknown_prot_address = 0x3fdf24;

	return DrvInit();
}

struct BurnDriverD BurnDrvOfftwallc = {
	"offtwallc", "offtwall", NULL, NULL, "1991",
	"Off the Wall (2-player cocktail)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_NOT_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PUZZLE, 0,
	NULL, offtwallcRomInfo, offtwallcRomName, NULL, NULL, NULL, NULL, OfftwallInputInfo, OfftwallDIPInfo,
	OfftwallcInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};
