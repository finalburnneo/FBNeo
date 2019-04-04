// FB Alpha Escape from the Planet of the Robot Monsters driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "watchdog.h"
#include "atariic.h"
#include "atarimo.h"
#include "atarijsa.h"
#include "burn_ym2151.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM0;
static UINT8 *Drv68KROM1;
static UINT8 *DrvM6502ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvPfRAM0;
static UINT8 *DrvPfRAM1;
static UINT8 *DrvMobRAM;
static UINT8 *DrvAlphaRAM;
static UINT8 *Drv68KRAM0;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 video_int_state;
static INT32 nExtraCycles[3] = { 0, 0, 0 };
static INT32 screen_intensity;
static INT32 video_disable;
static INT32 subcpu_halted;

static INT32 vblank;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT16 DrvInputs[3];
static UINT8 DrvDips[1];
static UINT8 DrvReset;
static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;
static UINT8 analog_port = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo EpromInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy3 + 0,	"p1 coin"	},
	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort1,"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort0,"p1 y-axis"),
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 9,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 11,	"p1 fire 3"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 coin"	},
	A("P2 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort3,"p2 x-axis"),
	A("P2 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort2,"p2 y-axis"),
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 9,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 8,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 11,	"p2 fire 3"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A

STDINPUTINFO(Eprom)

static struct BurnDIPInfo EpromDIPList[]=
{
	{0x0d, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"	},
	{0x0d, 0x01, 0x02, 0x02, "Off"			},
	{0x0d, 0x01, 0x02, 0x00, "On"			},
};

STDDIPINFO(Eprom)

static void update_interrupts()
{
	INT32 cpu = SekGetActive();

	INT32 state = 0;
	if (video_int_state) state = 4;
	if (atarijsa_int_state && cpu == 0) {
		state = 6;
	}
	if (state) {
		SekSetIRQLine(state, CPU_IRQSTATUS_ACK);
	} else {
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
	}

	if (atarijsa_int_state != 0 && (cpu^1) == 1) return;

    state = 0;
	if (video_int_state) state = 4;
	if (atarijsa_int_state && (cpu^1) == 0) {
		state = 6;
	}

	SekClose();
	SekOpen(cpu^1);
	if (state) {
		SekSetIRQLine(state, CPU_IRQSTATUS_ACK);
	} else {
		SekSetIRQLine(7, CPU_IRQSTATUS_NONE);
	}
	SekClose();
	SekOpen(cpu);
}


static void latch_write(UINT16 data)
{
	{
		subcpu_halted = ~data & 1;

		if (subcpu_halted) {
			INT32 cpu = SekGetActive();
			if (cpu == 0) {
				SekClose();
				SekOpen(1);
				SekReset();
				SekClose();
				SekOpen(0);
			} else {
				SekReset();
			}
		}

		screen_intensity = (data & 0x1e) >> 1;
		video_disable = (data & 0x20);
	}
}

static void __fastcall eprom_main_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffe000) == 0x3f2000) {
		*((UINT16*)(DrvMobRAM + (address & 0x1ffe))) = data;
		AtariMoWrite(0, (address & 0x1fff) >> 1, data);
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfffc00) == 0x16cc00) {
		UINT16 *ram = (UINT16*)DrvShareRAM;
		if ((ram[(address & 0xfffe)/2] & 0xff00) != (data & 0xff00) && address == 0x16cc00) {
			SekRunEnd();
		}
		ram[(address & 0xfffe)/2] = data;
		return;
	}

	switch (address)
	{
		case 0x2e0000:
			BurnWatchdogWrite();
		return;

		case 0x360000:
			video_int_state = 0;
			update_interrupts();
		return;

		case 0x360010:
			latch_write(data);
		return;

		case 0x360020:
			AtariJSAResetWrite(0);
		return;

		case 0x360030:
			AtariJSAWrite(data);
		return;
	}
}

static void __fastcall eprom_main_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffe000) == 0x3f2000) {
		DrvMobRAM[(address & 0x1fff)^1] = data;
		if (address&1)
			AtariMoWrite(0, (address & 0x1fff) >> 1, *((UINT16*)(DrvMobRAM + (address & 0x1ffe))));
		return;
	}

	if ((address & 0xff0000) == 0x1f0000) {
		AtariEEPROMUnlockWrite();
		return;
	}

	if ((address & 0xfffc00) == 0x16cc00) {
		if (DrvShareRAM[(address & 0xffff)^1] != data && (address & ~1) == 0x16cc00) {
			SekRunEnd();
		}
		DrvShareRAM[(address & 0xffff)^1] = data;
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
			video_int_state = 0;
			update_interrupts();
		return;

		case 0x360010:
		case 0x360011:
			latch_write(data);
		return;

		case 0x360021:
		case 0x360020:
			AtariJSAResetWrite(0);
		return;

		case 0x360031:
			AtariJSAWrite(data);
		return;
    }
}

static UINT16 special_port_read()
{
	UINT16 ret = DrvInputs[1] & ~0x11;
	ret ^= 0x10;
	if (atarigen_cpu_to_sound_ready) ret ^= 0x0008;
	if (atarigen_sound_to_cpu_ready) ret ^= 0x0004;
	if (vblank) ret ^= 1;

	return ret;
}

static UINT16 __fastcall eprom_main_read_word(UINT32 address)
{
	if ((address & 0xfffff0) == 0x260000) {
		return DrvInputs[0];
	}

	if ((address & 0xfffff0) == 0x260010) {
		return special_port_read();
	}

	if ((address & 0xfffff0) == 0x260020) {
		INT16 analog[4] = { DrvAnalogPort0, DrvAnalogPort1, DrvAnalogPort2, DrvAnalogPort3 };
		UINT8 result = ProcessAnalog(analog[analog_port], analog_port & 1, 1, 0x10, 0xf0);
		analog_port = ((address & 0xf) >> 1) & 3;
		return result;
	}

	if ((address & 0xfffffe) == 0x260030) {
		return AtariJSARead();
	}

	return 0;
}

static UINT8 __fastcall eprom_main_read_byte(UINT32 address)
{
	if ((address & 0xfffff0) == 0x260000) {
		return DrvInputs[0] >> ((~address & 1) * 8);
	}

	if ((address & 0xfffff0) == 0x260010) {
		return (special_port_read() >> ((~address & 1) * 8)) & 0xff;
	}

	if ((address & 0xfffff0) == 0x260020) {
		INT16 analog[4] = { DrvAnalogPort0, DrvAnalogPort1, DrvAnalogPort2, DrvAnalogPort3 };
		UINT8 result = ProcessAnalog(analog[analog_port], analog_port & 1, 1, 0x10, 0xf0);
		analog_port = ((address & 0xf) >> 1) & 3;
		return result;
	}

	if ((address & 0xfffffe) == 0x260030) {
		return AtariJSARead() >> (8 * (~address & 1));
    }

	return 0;
}

static tilemap_callback( alpha )
{
	UINT16 data = *((UINT16*)(DrvAlphaRAM + offs * 2));

	INT32 color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);

	TILE_SET_INFO(1, data, color, (data & 0x8000) ? TILE_OPAQUE : 0);
}

static tilemap_callback( eprbg )
{
	UINT16 data0 = *((UINT16*)(DrvPfRAM0 + offs * 2));
	UINT16 data1 = *((UINT16*)(DrvPfRAM1 + offs * 2));

	TILE_SET_INFO(0, data0, data1 >> 8, TILE_FLIPYX(data0 >> 15)); // color += 0x10, 
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	SekOpen(0);
	SekReset();
	SekClose();

	SekOpen(1);
	SekReset();
	SekClose();

	BurnWatchdogReset();
	AtariEEPROMReset();
	AtariJSAReset();

	subcpu_halted = 0;
	screen_intensity = 0;
	video_disable = 0;
	video_int_state = 0;

	memset (nExtraCycles, 0, sizeof(nExtraCycles));

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM0		= Next; Next += 0x0a0000;
	Drv68KROM1		= Next; Next += 0x020000;
	DrvM6502ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x200000;
	DrvGfxROM1		= Next; Next += 0x010000;

	DrvPalette		= (UINT32*)Next; Next += 0x800 * sizeof(UINT32);

	AllRam			= Next;

	DrvShareRAM		= Next; Next += 0x010000;
	DrvPalRAM		= Next; Next += 0x001000;
	DrvPfRAM0		= Next; Next += 0x002000;
	DrvPfRAM1		= Next; Next += 0x002000;
	DrvMobRAM		= Next; Next += 0x002000;
	DrvAlphaRAM		= Next; Next += 0x001000;
	Drv68KRAM0		= Next; Next += 0x003000;

	atarimo_0_slipram	 = (UINT16*)(DrvAlphaRAM + 0xf80);

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane0[4] = { RGN_FRAC(0x100000,0,4), RGN_FRAC(0x100000,1,4), RGN_FRAC(0x100000,2,4), RGN_FRAC(0x100000,3,4) };
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
		0,					/* index to which gfx system */
		1,					/* number of motion object banks */
		1,					/* are the entries linked? */
		0,					/* are the entries split? */
		1,					/* render in reverse order? */
		0,					/* render in swapped X/Y order? */
		0,					/* does the neighbor bit affect the next object? */
		8,					/* pixels per SLIP entry (0 for no-slip) */
		0,					/* pixel offset for SLIPs */
		0,					/* maximum number of links to visit/scanline (0=all) */

		0x100,				/* base palette entry */
		0x100,				/* maximum number of colors */
		0,					/* transparent pen index */

		{{ 0x03ff,0,0,0 }},	/* mask for the link */
		{{ 0 }},			/* mask for the graphics bank */
		{{ 0,0x7fff,0,0 }},	/* mask for the code index */
		{{ 0 }},			/* mask for the upper code index */
		{{ 0,0,0x000f,0 }},	/* mask for the color */
		{{ 0,0,0xff80,0 }},	/* mask for the X position */
		{{ 0,0,0,0xff80 }},	/* mask for the Y position */
		{{ 0,0,0,0x0070 }},	/* mask for the width, in tiles*/
		{{ 0,0,0,0x0007 }},	/* mask for the height, in tiles */
		{{ 0,0,0,0x0008 }},	/* mask for the horizontal flip */
		{{ 0 }},			/* mask for the vertical flip */
		{{ 0,0,0x0070,0 }},	/* mask for the priority */
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
		if (BurnLoadRom(Drv68KROM0 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x020001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x020000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x040001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x040000, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x060001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM0 + 0x060000, k++, 2)) return 1;

		if (BurnLoadRom(Drv68KROM1 + 0x000001, k++, 2)) return 1;
		if (BurnLoadRom(Drv68KROM1 + 0x000000, k++, 2)) return 1;

		if (BurnLoadRom(DrvM6502ROM+ 0x000000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x010000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x020000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x030000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x040000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x050000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x060000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x070000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x090000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0a0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0b0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0c0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0d0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0e0000, k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x0f0000, k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, k++, 1)) return 1;

		DrvGfxDecode();
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM0,			0x000000, 0x09ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM,			0x160000, 0x16ffff, MAP_RAM);
	SekMapMemory(NULL,					0x16cc00, 0x16cfff, MAP_WRITE); // unmap write!
	SekMapMemory(DrvPalRAM,				0x3e0000, 0x3e0fff, MAP_RAM);
	SekMapMemory(DrvPfRAM0,				0x3f0000, 0x3f1fff, MAP_RAM);
	SekMapMemory(DrvMobRAM,				0x3f2000, 0x3f3fff, MAP_ROM); // handler
	SekMapMemory(DrvAlphaRAM,			0x3f4000, 0x3f4fff, MAP_RAM);
	SekMapMemory(Drv68KRAM0,			0x3f5000, 0x3f7fff, MAP_RAM);
	SekMapMemory(DrvPfRAM1,				0x3f8000, 0x3f9fff, MAP_RAM);
	SekSetWriteWordHandler(0,			eprom_main_write_word);
	SekSetWriteByteHandler(0,			eprom_main_write_byte);
	SekSetReadWordHandler(0,			eprom_main_read_word);
	SekSetReadByteHandler(0,			eprom_main_read_byte);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1, 0x0e0000, 0x0e0fff);
	SekClose();

	SekInit(1, 0x68000);
	SekOpen(1);
	SekMapMemory(Drv68KROM1,			0x000000, 0x01ffff, MAP_ROM);
	SekMapMemory(Drv68KROM0 + 0x60000,	0x060000, 0x07ffff, MAP_ROM);
	SekMapMemory(DrvShareRAM,			0x160000, 0x16ffff, MAP_RAM);
	SekMapMemory(NULL,					0x16cc00, 0x16cfff, MAP_WRITE); // unmap write!
	SekSetWriteWordHandler(0,			eprom_main_write_word);
	SekSetWriteByteHandler(0,			eprom_main_write_byte);
	SekSetReadWordHandler(0,			eprom_main_read_word);
	SekSetReadByteHandler(0,			eprom_main_read_byte);
	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);

	AtariJSAInit(DrvM6502ROM, &update_interrupts, NULL, NULL);
    BurnYM2151SetInterleave((262/2)+1);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, eprbg_map_callback, 8, 8, 64, 64);
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

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 c = 0; c < 0x1000/2; c++)
	{
		INT32 data = p[c];

		INT32 i = (((data >> 12) & 15) + 1) * (4 - screen_intensity);
		if (i < 0) i = 0;

		INT32 r = ((data >> 8) & 15) * i / 4;
		INT32 g = ((data >> 4) & 15) * i / 4;
		INT32 b = ((data >> 0) & 15) * i / 4;

		DrvPalette[c] = BurnHighCol(r,g,b,0);
	}
}

static struct atarimo_rect_list rectlist, rectlistsave;

static void copy_sprites_layer1()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	rectlistsave = rectlist;

	//for (INT32 r = 0; r < rectlist.numrects; r++, rectlist.rect++) {
	//for (INT32 y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

		//for (INT32 x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				int mopriority = (mo[x] >> ATARIMO_PRIORITY_SHIFT) & 7;
				int pfpriority = (pf[x] >> 4) & 3;
				int forcemc0 = 0, shade = 1, pfm = 1, m7 = 0;

				if (mopriority & 4)
					continue;

				if (!(pf[x] & 8))
				{
					if (((pfpriority == 3) && !(mopriority & 1)) ||
						((pfpriority & 2) && !(mopriority & 2)) ||
						((pfpriority & 1) && (mopriority == 0)))
						forcemc0 = 1;
				}

				if (((mo[x] & 0x0f) != 1) ||
					((mo[x] & 0xf0) == 0) ||
					forcemc0)
					shade = 0;

				if ((mopriority == 3) ||
					(pf[x] & 8) ||
					(!(pfpriority & 1) && (mopriority & 2)) ||
					(!(pfpriority & 2) && (mopriority & 2)) ||
					(!(pfpriority & 2) && (mopriority & 1)) ||
					((pfpriority == 0) && (mopriority == 0)))
					pfm = 0;

				if ((mo[x] & 0x0f) == 1)
					m7 = 1;

				if (!pfm && !m7)
				{
					if (!forcemc0)
						pf[x] = mo[x] & ATARIMO_DATA_MASK;
					else
						pf[x] = mo[x] & ATARIMO_DATA_MASK & ~0x70;
				}
				else
				{
					if (shade)
						pf[x] |= 0x100;
					if (m7)
						pf[x] |= 0x080;
				}
			}
		}
	}
	//}
}

static void copy_sprites_layer2()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	//rectlist.rect -= rectlist.numrects;
	rectlist = rectlistsave;

	//for (INT32 r = 0; r < rectlist.numrects; r++, rectlist.rect++) {
   // for (INT32 y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

	  //  for (INT32 x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				INT32 mopriority = mo[x] >> ATARIMO_PRIORITY_SHIFT;
				/* upper bit of MO priority might mean palette kludges */
				if (mopriority & 4)
				{
					/* if bit 2 is set, start setting high palette bits */
					if (mo[x] & 2)
						atarimo_apply_stain(pf, mo, x, y, nScreenWidth);
				}

				mo[x] = 0xffff; // clean!
			}
		}
	}
 //   }
}

static INT32 lastline = 0;

static void DrvDrawBegin()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force!!
	}

	if (pBurnDraw) BurnTransferClear();

	lastline = 0;
}

static void partial_update(INT32 scanline)
{
	if (!pBurnDraw) return;
	if (scanline==240) scanline=239; // because +1
	if (scanline < 0 || scanline > nScreenHeight || scanline == lastline || lastline > scanline) return;

	//bprintf(0, _T("%07d: partial %d - %d. \n"), nCurrentFrame, lastline, scanline);

	GenericTilesSetClip(0, nScreenWidth, lastline, scanline+1);

	AtariMoRender(0, &rectlist);

	if (nBurnLayer & 1) GenericTilemapDraw(0, pTransDraw, 0);

	if (nSpriteEnable & 1) copy_sprites_layer1();

	if (nBurnLayer & 2) GenericTilemapDraw(1, pTransDraw, 0);

	if (nSpriteEnable & 1) copy_sprites_layer2();
	GenericTilesClearClip();

	lastline = scanline+1;
}

static INT32 DrvDraw()
{
	BurnTransferCopy(DrvPalette);

	return 0;
}

static inline void video_update()
{
	UINT16 *scr = (UINT16*)(DrvAlphaRAM + 0xf00);

	GenericTilemapSetScrollX(0, (scr[0] >> 7) & 0x1ff);
	GenericTilemapSetScrollY(0, (scr[1] >> 7) & 0x1ff);
	atarimo_set_xscroll(0, (scr[0] >> 7) & 0x1ff);
	atarimo_set_yscroll(0, (scr[1] >> 7) & 0x1ff);
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	SekNewFrame();
	M6502NewFrame();

	if (DrvReset) {
		DrvDoReset(1);
	}

	{
		DrvInputs[0] = 0xffff;
		DrvInputs[1] = 0xfffd | (DrvDips[0] & 2);
		DrvInputs[2] = 0x0040;

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		atarijsa_input_port = DrvInputs[2];
		atarijsa_test_mask = 0x02;
		atarijsa_test_port = (DrvDips[0] & atarijsa_test_mask);
	}

	INT32 nSoundBufferPos = 0;
	INT32 nInterleave = 262;
	INT32 nCyclesTotal[3] = { (INT32)((double)7159090 / 59.92), (INT32)((double)7159090 / 59.92), (INT32)((double)1789773 / 59.92) };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	M6502Open(0);

	INT32 partial = 0;
	vblank = 0;
	DrvDrawBegin();

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 0) video_update();
		if (i == partial) {
			if (i != 0) {
				partial_update(i-1);
			}
			partial += 64;
		}

		SekOpen(0);
		nCyclesDone[0] += SekRun(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		nCyclesDone[2] += M6502Run(((i + 1) * nCyclesTotal[2] / nInterleave) - nCyclesDone[2]);
		SekClose();

		SekOpen(1);
		if (subcpu_halted == 0) {
			nCyclesDone[1] += SekRun(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		} else {
			nCyclesDone[1] += SekIdle(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		}
		SekClose();

		if (i == 239) {
			partial_update(240);

			partial = 0;

			vblank = 1;
			video_int_state = 1;
			SekOpen(0);
			update_interrupts();
			SekClose();

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		AtariJSAInterruptUpdate(nInterleave);;

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

		SCAN_VAR(subcpu_halted);
		SCAN_VAR(nExtraCycles);
		SCAN_VAR(screen_intensity);
		SCAN_VAR(video_disable);
		SCAN_VAR(video_int_state);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Escape from the Planet of the Robot Monsters (set 1)

static struct BurnRomInfo epromRomDesc[] = {
	{ "136069-3025.50a",		0x10000, 0x08888dec, 1 | BRF_PRG | BRF_ESS }, //  0 maincpu
	{ "136069-3024.40a",		0x10000, 0x29cb1e97, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136069-4027.50b",		0x10000, 0x702241c9, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136069-4026.40b",		0x10000, 0xfecbf9e2, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136069-4029.50d",		0x10000, 0x0f2f1502, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136069-4028.40d",		0x10000, 0xbc6f6ae8, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136069-2033.40k",		0x10000, 0x130650f6, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136069-2032.50k",		0x10000, 0x1da21ed8, 1 | BRF_PRG | BRF_ESS }, //  7

	{ "136069-2035.10s",		0x10000, 0xdeff6469, 2 | BRF_GRA },           //  8 extra
	{ "136069-2034.10u",		0x10000, 0x5d7afca2, 2 | BRF_GRA },           //  9

	{ "136069-1040.7b",			0x10000, 0x86e93695, 3 | BRF_PRG | BRF_ESS }, // 10 jsa:cpu

	{ "136069-1020.47s",		0x10000, 0x0de9d98d, 4 | BRF_GRA },           // 11 gfx1
	{ "136069-1013.43s",		0x10000, 0x8eb106ad, 4 | BRF_GRA },           // 12
	{ "136069-1018.38s",		0x10000, 0xbf3d0e18, 4 | BRF_GRA },           // 13
	{ "136069-1023.32s",		0x10000, 0x48fb2e42, 4 | BRF_GRA },           // 14
	{ "136069-1016.76s",		0x10000, 0x602d939d, 4 | BRF_GRA },           // 15
	{ "136069-1011.70s",		0x10000, 0xf6c973af, 4 | BRF_GRA },           // 16
	{ "136069-1017.64s",		0x10000, 0x9cd52e30, 4 | BRF_GRA },           // 17
	{ "136069-1022.57s",		0x10000, 0x4e2c2e7e, 4 | BRF_GRA },           // 18
	{ "136069-1012.47u",		0x10000, 0xe7edcced, 4 | BRF_GRA },           // 19
	{ "136069-1010.43u",		0x10000, 0x9d3e144d, 4 | BRF_GRA },           // 20
	{ "136069-1015.38u",		0x10000, 0x23f40437, 4 | BRF_GRA },           // 21
	{ "136069-1021.32u",		0x10000, 0x2a47ff7b, 4 | BRF_GRA },           // 22
	{ "136069-1008.76u",		0x10000, 0xb0cead58, 4 | BRF_GRA },           // 23
	{ "136069-1009.70u",		0x10000, 0xfbc3934b, 4 | BRF_GRA },           // 24
	{ "136069-1014.64u",		0x10000, 0x0e07493b, 4 | BRF_GRA },           // 25
	{ "136069-1019.57u",		0x10000, 0x34f8f0ed, 4 | BRF_GRA },           // 26

	{ "136069-1007.125d",		0x04000, 0x409d818e, 5 | BRF_GRA },           // 27 gfx2

	{ "gal16v8-136069.100t",	0x00117, 0xfd9d472e, 6 | BRF_OPT },           // 28 plds
	{ "gal16v8-136069.100v",	0x00117, 0xcd472121, 6 | BRF_OPT },           // 29
	{ "gal16v8-136069.50f",		0x00117, 0xdb013b25, 6 | BRF_OPT },           // 30
	{ "gal16v8-136069.50p",		0x00117, 0x4a765b00, 6 | BRF_OPT },           // 31
	{ "gal16v8-136069.55p",		0x00117, 0x48abc939, 6 | BRF_OPT },           // 32
	{ "gal16v8-136069.70j",		0x00117, 0x3b4ebe41, 6 | BRF_OPT },           // 33
};

STD_ROM_PICK(eprom)
STD_ROM_FN(eprom)

struct BurnDriver BurnDrvEprom = {
	"eprom", NULL, NULL, NULL, "1989",
	"Escape from the Planet of the Robot Monsters (set 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, epromRomInfo, epromRomName, NULL, NULL, NULL, NULL, EpromInputInfo, EpromDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x800,
	336, 240, 4, 3
};
