// FB Alpha Return of the Jedi driver module
// Based on MAME driver by Dan Boris and Aaron Giles

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "pokey.h"
#include "tms5220.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSmthPROM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvM6502RAM0;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvBgRAM;
static UINT8 *DrvFgRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 nvram_enable;
static INT32 a2d_select;
static INT32 bankselect;
static INT32 foreground_bank;
static INT32 video_off;
static INT32 scrollx;
static INT32 scrolly;
static INT32 soundlatch[2];
static INT32 smoothing_table;
static INT32 audio_in_reset;

static INT32 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo JediInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 coin"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 fire 3"	},

	A("P1 Stick X",     BIT_ANALOG_REL, &DrvAnalogPort0,"p1 x-axis"),
	A("P1 Stick Y",     BIT_ANALOG_REL, &DrvAnalogPort1,"p1 y-axis"),

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy1 + 5,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};
#undef A

STDINPUTINFO(Jedi)

static struct BurnDIPInfo JediDIPList[]=
{
	{0x09, 0xff, 0xff, 0x10, NULL					},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x09, 0x01, 0x10, 0x10, "Off"					},
	{0x09, 0x01, 0x10, 0x00, "On"					},
};

STDDIPINFO(Jedi)

static void bankswitch(INT32 data)
{
	bankselect = data;

	if (data & 1) M6502MapMemory(DrvM6502ROM0 + 0x10000, 0x4000, 0x7fff, MAP_ROM);
	if (data & 2) M6502MapMemory(DrvM6502ROM0 + 0x14000, 0x4000, 0x7fff, MAP_ROM);
	if (data & 4) M6502MapMemory(DrvM6502ROM0 + 0x18000, 0x4000, 0x7fff, MAP_ROM);
}

static inline UINT8 audio_pending()
{
	return ((soundlatch[0] & 0x100) >> 7) | ((soundlatch[1] & 0x100) >> 8);
}

static void jedi_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0x0800) {
		if (nvram_enable) {
			DrvNVRAM[address & 0xff] = data;
		}
		return;
	}

	if ((address & 0xfe00) == 0x3e00) {
		smoothing_table = data;
		return;
	}

	switch (address & ~0x78)
	{
		case 0x1c00:
		case 0x1c01:
			nvram_enable = ~address & 1;
		return;

		case 0x1c80:
		case 0x1c81:
		case 0x1c82:
			a2d_select = address & 3;
		return;

		case 0x1d00:
		return; // nvram store

		case 0x1d80:
			BurnWatchdogWrite();
		return;

		case 0x1e00:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x1e80:
		case 0x1e81:
			// coin counter left (0), right (0), (data & 0x80)
		return;

		case 0x1e82:
		case 0x1e83:
			// LED control (data & 0x80)
		return;

		case 0x1e84:
			foreground_bank = data >> 7;
		return;

		case 0x1e85:
		return; // not used

		case 0x1e86:
			audio_in_reset = ~data & 0x80;
			if (audio_in_reset) {
				M6502Close();
				M6502Open(1);
				M6502Reset();
				M6502Close();
				M6502Open(0);
			}
		return;

		case 0x1e87:
			video_off = data >> 7;
		return;

		case 0x1f00:
			soundlatch[0] = data | 0x100;
		return;

		case 0x1f80:
			bankswitch(data);
		return;

		case 0x3c00:
		case 0x3c01:
			scrolly = data + (address & 1) * 256;
		return;

		case 0x3d00:
		case 0x3d01:
			scrollx = data + (address & 1) * 256;
		return;
	}

	if ((address & 0xf800) == 0x6800) return; // NOP (service mode control test)

	bprintf (0, _T("MW: %4.4x, %2.2x\n"), address, data);
}

static UINT8 jedi_main_read(UINT16 address)
{
	if ((address & 0xfc00) == 0x1400) address &= 0xfc00; // masking 0x3ff
	if ((address & 0xfc00) == 0x1800) address &= 0xfc00; // same

	switch (address)
	{
		case 0x0c00:
			return (DrvInputs[0] & ~0x10) | (DrvDips[0] & 0x10);

		case 0x0c01:
		{
			UINT8 ret = DrvInputs[1] & 0x1f;
			ret |= audio_pending() << 5;
			if (vblank) ret |= 0x80;
			return ret;
		}

		case 0x1400:
			soundlatch[1] &= 0xff;
			return soundlatch[1];

		case 0x1800:
			switch (a2d_select) {
				case 0: return ProcessAnalog(DrvAnalogPort1, 0, 1, 0x00, 0xff);
				case 2: return ProcessAnalog(DrvAnalogPort0, 0, 1, 0x00, 0xff);
			    default: return 0;
			}
		case 0x3c00:
		case 0x3c01:
		case 0x3d00:
		case 0x3d01:
			return 0; // nop
	}

	if ((address & 0xfe00) == 0x3e00) {
		return 0; // nop
	}

	bprintf (0, _T("MR: %4.4x\n"), address);

	return 0;
}

static void jedi_sound_write(UINT16 address, UINT8 data)
{
	if ((address & 0xffc0) == 0x0800) {
		pokey_write((address >> 4) & 3, address & 0xf, data);
		return;
	}

	if ((address & 0xfe00) == 0x1200) {
        tms5220_wsq_w((address >> 8) & 1);
		return;
	}

	if ((address & 0xff00) == 0x1100) address = 0x1100; // speech_data mask

	switch (address)
	{
		case 0x1000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x1100:
            tms5220_write(data);
		return;

		case 0x1400:
			soundlatch[1] = data | 0x100;
		return;

        case 0x1500:
            tms5220_volume((data & 1) ? 1.00 : 0.00);
		return;
	}

	bprintf (0, _T("SW: %4.4x, %2.2x\n"), address, data);
}

static UINT8 jedi_sound_read(UINT16 address)
{
	if ((address & 0xffc0) == 0x0800) {
		return pokey_read((address >> 4) & 3, address & 0xf);
	}

	switch (address)
	{
		case 0x1800:
		case 0x1801:
			soundlatch[0] &= 0xff;
			return soundlatch[0];

		case 0x1c00:
			return tms5220_ready() ? 0 : 0x80;

		case 0x1c01:
			return (audio_pending() << 6);
	}

	bprintf (0, _T("SR: %4.4x\n"), address);

	return 0;
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	bankswitch(0);
	M6502Reset();
	tms5220_reset();
	M6502Close();

	BurnWatchdogReset();

	PokeyReset();

	if (clear_mem) {
		nvram_enable = 0;
		a2d_select = 0;
		bankselect = 0;
		foreground_bank = 0;
		video_off = 0;
		scrollx = 0;
		scrolly = 0;
		soundlatch[0] = 0;
		soundlatch[1] = 0;
		smoothing_table = 0;
	}

	audio_in_reset = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM0		= Next; Next += 0x01c000;
	DrvM6502ROM1		= Next; Next += 0x010000;

	DrvGfxROM0			= Next; Next += 0x002000;
	DrvGfxROM1			= Next; Next += 0x010000;
	DrvGfxROM2			= Next; Next += 0x020000;

	DrvSmthPROM			= Next; Next += 0x001000;

	DrvPalette			= (UINT32*)Next; Next += 0x400 * sizeof(UINT32);

	DrvNVRAM			= Next; Next += 0x000100;

	AllRam				= Next;

	DrvM6502RAM0		= Next; Next += 0x000800;
	DrvM6502RAM1		= Next; Next += 0x000800;

	DrvBgRAM			= Next; Next += 0x000800;
	DrvFgRAM			= Next; Next += 0x000c00;
	DrvPalRAM			= Next; Next += 0x000800;

	DrvSprRAM 			= DrvFgRAM + 0x7c0;

	RamEnd				= Next;

	MemEnd				= Next;

	return 0;
}

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		INT32 k = 0;
		if (BurnLoadRom(DrvM6502ROM0 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x0c000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x14000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x18000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x0c000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x08000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2   + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x08000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x10000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM2   + 0x18000,  k++, 1)) return 1;

		if (BurnLoadRom(DrvSmthPROM  + 0x00000,  k++, 1)) return 1;
		if (BurnLoadRom(DrvSmthPROM  + 0x00800,  k++, 1)) return 1;
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,			0x0000, 0x07ff, MAP_RAM);
	for (INT32 i = 0; i < 0x400; i+=0x100) {
		M6502MapMemory(DrvNVRAM,			0x0800 + i, 0x08ff + i, MAP_ROM); // handler
	}
	M6502MapMemory(DrvBgRAM,				0x2000, 0x27ff, MAP_RAM);
	M6502MapMemory(DrvPalRAM,				0x2800, 0x2fff, MAP_RAM);
	M6502MapMemory(DrvFgRAM,				0x3000, 0x3bff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM0 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(jedi_main_write);
	M6502SetReadHandler(jedi_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,			0x0000, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1 + 0x8000,	0x8000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(jedi_sound_write);
	M6502SetReadHandler(jedi_sound_read);
	M6502Close();

	BurnWatchdogInit(DrvDoReset, 180);

	PokeyInit(12096000/8, 4, 0.30, 0);
	PokeySetTotalCyclesCB(M6502TotalCycles);
	PokeySetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);
	PokeySetRoute(1, 0.30, BURN_SND_ROUTE_BOTH);
	PokeySetRoute(2, 0.30, BURN_SND_ROUTE_LEFT);
	PokeySetRoute(3, 0.30, BURN_SND_ROUTE_RIGHT);

	tms5220_init(M6502TotalCycles, 1512000);
	tms5220_set_frequency(672000);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();

	PokeyExit();
	tms5220_exit();

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		UINT16 color = DrvPalRAM[offs] | (DrvPalRAM[offs | 0x400] << 8);

		INT32 i =  (color >> 9) & 7;
		UINT8 r = ((color >> 6) & 7) * 5 * i;
		UINT8 g = ((color >> 3) & 7) * 5 * i;
		UINT8 b = ((color >> 0) & 7) * 5 * i;

		DrvPalette[offs] = BurnHighCol(r,g,b,0);
	}
}

static void draw_background_and_text()
{
	INT32 background_line_buffer[0x200];

	UINT8 *tx_gfx = DrvGfxROM0;
	UINT8 *bg_gfx = DrvGfxROM1;
	UINT8 *prom1 = &DrvSmthPROM[0x0000 | ((smoothing_table & 0x03) << 8)];
	UINT8 *prom2 = &DrvSmthPROM[0x0800 | ((smoothing_table & 0x03) << 8)];
	INT32 vscroll = scrolly;
	INT32 hscroll = scrollx;
	INT32 tx_bank = foreground_bank;
	UINT8 *tx_ram = DrvFgRAM;
	UINT8 *bg_ram = DrvBgRAM;

	memset(background_line_buffer, 0, 0x200 * sizeof(INT32));

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		INT32 bg_last_col = 0;

		for (INT32 x = 0; x < nScreenWidth; x += 2)
		{
			INT32 tx_col1, tx_col2, bg_col;
			INT32 bg_tempcol;
			INT32 tx_gfx_offs, bg_gfx_offs;
			INT32 tx_data, bg_data1, bg_data2;

			INT32 sy = y + vscroll;
			INT32 sx = x + hscroll;

			INT32 tx_offs = ((y & 0xf8) << 3) | (x >> 3);
			INT32 bg_offs = ((sy & 0x1f0) << 1) | ((sx & 0x1f0) >> 4);

			INT32 tx_code = (tx_bank << 8) | tx_ram[tx_offs];
			INT32 bg_bank = bg_ram[0x0400 | bg_offs];
			INT32 bg_code = bg_ram[0x0000 | bg_offs] |
							((bg_bank & 0x01) << 8) |
							((bg_bank & 0x08) << 6) |
							((bg_bank & 0x02) << 9);

			if (bg_bank & 0x04)
				sx = sx ^ 0x0f;

			tx_gfx_offs = (tx_code << 4) | ((y & 0x07) << 1) | ((( x & 0x04) >> 2));
			bg_gfx_offs = (bg_code << 4) | (sy & 0x0e)       | (((sx & 0x08) >> 3));

			tx_data  = tx_gfx[         tx_gfx_offs];
			bg_data1 = bg_gfx[0x0000 | bg_gfx_offs];
			bg_data2 = bg_gfx[0x8000 | bg_gfx_offs];

			if (x & 0x02)
			{
				tx_col1 = ((tx_data  & 0x0c) << 6);
				tx_col2 = ((tx_data  & 0x03) << 8);
			}
			else
			{
				tx_col1 = ((tx_data  & 0xc0) << 2);
				tx_col2 = ((tx_data  & 0x30) << 4);
			}

			switch (sx & 0x06)
			{
				case 0x00: bg_col = ((bg_data1 & 0x80) >> 4) | ((bg_data1 & 0x08) >> 1) | ((bg_data2 & 0x80) >> 6) | ((bg_data2 & 0x08) >> 3); break;
				case 0x02: bg_col = ((bg_data1 & 0x40) >> 3) | ((bg_data1 & 0x04) >> 0) | ((bg_data2 & 0x40) >> 5) | ((bg_data2 & 0x04) >> 2); break;
				case 0x04: bg_col = ((bg_data1 & 0x20) >> 2) | ((bg_data1 & 0x02) << 1) | ((bg_data2 & 0x20) >> 4) | ((bg_data2 & 0x02) >> 1); break;
				default:   bg_col = ((bg_data1 & 0x10) >> 1) | ((bg_data1 & 0x01) << 2) | ((bg_data2 & 0x10) >> 3) | ((bg_data2 & 0x01) >> 0); break;
			}

			bg_tempcol = prom1[(bg_last_col << 4) | bg_col];
			pTransDraw[(y * nScreenWidth) + x + 0] = tx_col1 | prom2[(background_line_buffer[x + 0] << 4) | bg_tempcol];
			pTransDraw[(y * nScreenWidth) + x + 1] = tx_col2 | prom2[(background_line_buffer[x + 1] << 4) | bg_col];
			background_line_buffer[x + 0] = bg_tempcol;
			background_line_buffer[x + 1] = bg_col;

			bg_last_col = bg_col;
		}
	}
}

static void draw_sprites()
{
	UINT8 *spriteram = DrvSprRAM;
	UINT8 *gfx3 = DrvGfxROM2;

	for (INT32 offs = 0; offs < 0x30; offs++)
	{
		INT32 y_size;

		UINT8 y = 240 - spriteram[offs + 0x80] + 1;
		INT32 flip_x = spriteram[offs + 0x40] & 0x10;
		INT32 flip_y = spriteram[offs + 0x40] & 0x20;
		INT32 tall = spriteram[offs + 0x40] & 0x08;

		UINT16 code = spriteram[offs] |
						((spriteram[offs + 0x40] & 0x04) << 8) |
						((spriteram[offs + 0x40] & 0x40) << 3) |
						((spriteram[offs + 0x40] & 0x02) << 7);

		if (tall)
		{
			code &= ~1;
			y_size = 0x20;
			y = y - 0x10;
		}
		else
			y_size = 0x10;

		UINT8 *gfx = &gfx3[code << 5];

		if (flip_y)
			y = y + y_size - 1;

		for (INT32 sy = 0; sy < y_size; sy++)
		{
			UINT16 x = spriteram[offs + 0x100] + ((spriteram[offs + 0x40] & 0x01) << 8) - 2;

			if (/*(y < 0) || */(y >= 240))
				continue;

			if (flip_x)
				x = x + 7;

			for (INT32 i = 0; i < 2; i++)
			{
				UINT8 data1 = *(0x00000 + gfx);
				UINT8 data2 = *(0x10000 + gfx);

				for (INT32 sx = 0; sx < 4; sx++)
				{
					UINT32 col = (data1 & 0x80) | ((data1 & 0x08) << 3) | ((data2 & 0x80) >> 2) | ((data2 & 0x08) << 1);

					x = x & 0x1ff;

					if (col) {
						if (y >= 0 && y < nScreenHeight && x >= 0 && x < nScreenWidth) {
							pTransDraw[(y * nScreenWidth) + x] = (pTransDraw[(y * nScreenWidth) + x] & 0x30f) | col;
						}
					}

					if (flip_x)
						x = x - 1;
					else
						x = x + 1;

					data1 = data1 << 1;
					data2 = data2 << 1;
				}

				gfx = gfx + 1;
			}

			if (flip_y)
				y = y - 1;
			else
				y = y + 1;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1; // force update (palram)
	}

	BurnTransferClear();

	if (!video_off)
	{
		if (nBurnLayer & 1) draw_background_and_text();
		if (nSpriteEnable & 1) draw_sprites();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}
	
static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	BurnWatchdogUpdate();

	{
		DrvInputs[0] = 0xf7;
		DrvInputs[1] = 0x1b;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

    M6502NewFrame();

	INT32 nInterleave = 262; // for scanlines
	INT32 nCyclesTotal[2] = { 2500000 / 60, 1512000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		nCyclesDone[0] += M6502Run(((i + 1) * nCyclesTotal[0] / nInterleave) - nCyclesDone[0]);
		if ((i%64) == 63) M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6502Close();

		M6502Open(1);
		if (audio_in_reset == 0) {
            nCyclesDone[1] += M6502Run(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		} else {
            nCyclesDone[1] += M6502Idle(((i + 1) * nCyclesTotal[1] / nInterleave) - nCyclesDone[1]);
		}
		if ((i%64) == 63) M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		M6502Close();

		if (i == 240) {
			vblank = 1;

			if (pBurnDraw) {
				BurnDrvRedraw();
			}
		}

		if (pBurnSoundOut && (i%4)==3) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/4);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			pokey_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			pokey_update(pSoundBuf, nSegmentLength);
		}
		tms5220_update(pBurnSoundOut, nBurnSoundLen);
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

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		BurnWatchdogScan(nAction);

		pokey_scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);

		SCAN_VAR(nvram_enable);
		SCAN_VAR(a2d_select);
		SCAN_VAR(bankselect);
		SCAN_VAR(foreground_bank);
		SCAN_VAR(video_off);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
		SCAN_VAR(soundlatch);
		SCAN_VAR(smoothing_table);
		SCAN_VAR(audio_in_reset);
	}

	if (nAction & ACB_NVRAM) {
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x00100;
		ba.nAddress	= 0;
		ba.szName	= "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		M6502Open(0);
		bankswitch(bankselect);
		M6502Close();
	}

	return 0;
}


// Return of the Jedi

static struct BurnRomInfo jediRomDesc[] = {
	{ "136030-221.14f",	0x4000, 0x414d05e3, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 Code
	{ "136030-222.13f",	0x4000, 0x7b3f21be, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136030-123.13d",	0x4000, 0x877f554a, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136030-124.13b",	0x4000, 0xe72d41db, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136030-122.13a",	0x4000, 0xcce7ced5, 1 | BRF_PRG | BRF_ESS }, //  4

	{ "136030-133.01c",	0x4000, 0x6c601c69, 2 | BRF_PRG | BRF_ESS }, //  5 M6502 #1 Code
	{ "136030-134.01a",	0x4000, 0x5e36c564, 2 | BRF_PRG | BRF_ESS }, //  6

	{ "136030-215.11t",	0x2000, 0x3e49491f, 3 | BRF_GRA },           //  7 Foreground Graphics

	{ "136030-126.06r",	0x8000, 0x9c55ece8, 4 | BRF_GRA },           //  8 Background Graphics
	{ "136030-127.06n",	0x8000, 0x4b09dcc5, 4 | BRF_GRA },           //  9

	{ "136030-130.01h",	0x8000, 0x2646a793, 5 | BRF_GRA },           // 10 Sprite Graphics
	{ "136030-131.01f",	0x8000, 0x60107350, 5 | BRF_GRA },           // 11
	{ "136030-128.01m",	0x8000, 0x24663184, 5 | BRF_GRA },           // 12
	{ "136030-129.01k",	0x8000, 0xac86b98c, 5 | BRF_GRA },           // 13

	{ "136030-117.bin",	0x0400, 0x9831bd55, 6 | BRF_GRA },           // 14 Smoothing PROMs
	{ "136030-118.bin",	0x0400, 0x261fbfe7, 6 | BRF_GRA },           // 15
};

STD_ROM_PICK(jedi)
STD_ROM_FN(jedi)

struct BurnDriver BurnDrvJedi = {
	"jedi", NULL, NULL, NULL, "1984",
	"Return of the Jedi\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT | GBF_RACING | GBF_ACTION, 0,
	NULL, jediRomInfo, jediRomName, NULL, NULL, NULL, NULL, JediInputInfo, JediDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	296, 240, 4, 3
};
