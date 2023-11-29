// FinalBurn Neo Midway X Unit driver module
// Based on MAME driver by Aaron Giles

#include "tiles_generic.h"
#include "tms34_intf.h"
#include "midwayic.h"
#include "dcs2k.h"
#include "burn_pal.h"
#include "burn_gun.h"

static UINT8* AllMem;
static UINT8* AllRam;
static UINT8* RamEnd;
static UINT8* MemEnd;
static UINT8* DrvTMSROM;
static UINT8* DrvGfxROM;
static UINT8* DrvSndROM;
static UINT8* DrvNVRAM;
static UINT8* DrvVidRAM;
static UINT16* DrvVRAM16;
static UINT8* DrvTMSRAM;

static INT32 security_bits;
static INT32 analog_port;
static UINT8 uart[8];
static UINT16 nDMA[0x20];

static INT32 bGfxRomLarge = 1;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvJoy3[16];
static UINT8 DrvDips[3];
static UINT16 DrvInputs[6];
static UINT8 DrvReset;

static INT16 Gun[6];

static INT32 nExtraCycles = 0;

static INT32 vb_start = 0;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo RevxInputList[] = {
	{"P1 Coin", BIT_DIGITAL, DrvJoy3 + 0, "p1 coin"},
	{"P1 Start", BIT_DIGITAL, DrvJoy3 + 2, "p1 start"},
	{"P1 Button 1", BIT_DIGITAL, DrvJoy1 + 4, "p1 fire 1"},
	{"P1 Button 2", BIT_DIGITAL, DrvJoy1 + 5, "p1 fire 2"},
	A("P1 Gun X", BIT_ANALOG_REL, &Gun[0], "mouse x-axis"),
	A("P1 Gun Y", BIT_ANALOG_REL, &Gun[1], "mouse y-axis"),

	{"P2 Coin", BIT_DIGITAL, DrvJoy3 + 1, "p2 coin"},
	{"P2 Start", BIT_DIGITAL, DrvJoy3 + 5, "p2 start"},
	{"P2 Button 1", BIT_DIGITAL, DrvJoy1 + 12, "p2 fire 1"},
	{"P2 Button 2", BIT_DIGITAL, DrvJoy1 + 13, "p2 fire 2"},
	A("P2 Gun X", BIT_ANALOG_REL, &Gun[2], "p2 x-axis"),
	A("P2 Gun Y", BIT_ANALOG_REL, &Gun[3], "p2 y-axis"),

	{"P3 Coin", BIT_DIGITAL, DrvJoy3 + 7, "p3 coin"},
	{"P3 Start", BIT_DIGITAL, DrvJoy3 + 9, "p3 start"},
	{"P3 Button 1", BIT_DIGITAL, DrvJoy2 + 4, "p3 fire 1"},
	{"P3 Button 2", BIT_DIGITAL, DrvJoy2 + 5, "p3 fire 2"},
	A("P3 Gun X", BIT_ANALOG_REL, &Gun[4], "p2 x-axis"),
	A("P3 Gun Y", BIT_ANALOG_REL, &Gun[5], "p2 y-axis"),

	{"Reset", BIT_DIGITAL, &DrvReset, "reset"},
	{"Service", BIT_DIGITAL, DrvJoy3 + 6, "service"},
	{"Service Mode", BIT_DIGITAL, DrvJoy3 + 4, "diag"},
	{"Tilt", BIT_DIGITAL, DrvJoy3 + 3, "tilt"},
	{"Volume Down", BIT_DIGITAL, DrvJoy3 + 11, "p1 fire 7"},
	{"Volume Up", BIT_DIGITAL, DrvJoy3 + 12, "p1 fire 8"},
	{"Dip A", BIT_DIPSWITCH, DrvDips + 0, "dip"},
	{"Dip B", BIT_DIPSWITCH, DrvDips + 1, "dip"},
	{"Dip C", BIT_DIPSWITCH, DrvDips + 2, "dip"},
};
#undef A
STDINPUTINFO(Revx)

static struct BurnDIPInfo RevxDIPList[] =
{
	DIP_OFFSET(0x18)
	{0x00, 0xff, 0xff, 0x7c, nullptr},
	{0x01, 0xff, 0xff, 0xff, nullptr},
	{0x02, 0xff, 0xff, 0x00, nullptr},

	{0, 0xfe, 0, 2, "Flip Screen"},
	{0x00, 0x01, 0x01, 0x00, "Off"},
	{0x00, 0x01, 0x01, 0x01, "On"},

	{0, 0xfe, 0, 2, "Dipswitch Coinage"},
	{0x00, 0x01, 0x02, 0x00, "Off"},
	{0x00, 0x01, 0x02, 0x02, "On"},

	{0, 0xfe, 0, 5, "Coinage"},
	{0x00, 0x01, 0x1c, 0x1c, "1"},
	{0x00, 0x01, 0x1c, 0x18, "2"},
	{0x00, 0x01, 0x1c, 0x14, "3"},
	{0x00, 0x01, 0x1c, 0x0c, "ECA"},
	{0x00, 0x01, 0x1c, 0x00, "Free Play"},

	{0, 0xfe, 0, 6, "Credits"},
	{0x00, 0x01, 0xe0, 0x20, "3 Start/1 Continue"},
	{0x00, 0x01, 0xe0, 0xe0, "2 Start/2 Continue"},
	{0x00, 0x01, 0xe0, 0xa0, "2 Start/1 Continue"},
	{0x00, 0x01, 0xe0, 0x00, "1 Start/4 Continue"},
	{0x00, 0x01, 0xe0, 0x40, "1 Start/3 Continue"},
	{0x00, 0x01, 0xe0, 0x60, "1 Start/1 Continue"},

	{0, 0xfe, 0, 3, "Country"},
	{0x01, 0x01, 0x03, 0x03, "USA"},
	{0x01, 0x01, 0x03, 0x01, "French"},
	{0x01, 0x01, 0x03, 0x02, "German"},

	{0, 0xfe, 0, 2, "Bill Validator"},
	{0x01, 0x01, 0x04, 0x04, "Off"},
	{0x01, 0x01, 0x04, 0x00, "On"},

	{0, 0xfe, 0, 2, "Two Counters"},
	{0x01, 0x01, 0x08, 0x08, "Off"},
	{0x01, 0x01, 0x08, 0x00, "On"},

	{0, 0xfe, 0, 2, "Players"},
	{0x01, 0x01, 0x10, 0x10, "3 Players"},
	{0x01, 0x01, 0x10, 0x00, "2 Players"},

	{0, 0xfe, 0, 2, "Cabinet"},
	{0x01, 0x01, 0x20, 0x20, "Rev X"},
	{0x01, 0x01, 0x20, 0x00, "Terminator 2"},

	{0, 0xfe, 0, 2, "Video Freeze"},
	{0x01, 0x01, 0x40, 0x40, "Off"},
	{0x01, 0x01, 0x40, 0x00, "On"},

	{0, 0xfe, 0, 2, "Test Switch"},
	{0x01, 0x01, 0x80, 0x80, "Off"},
	{0x01, 0x01, 0x80, 0x00, "On"},

	{0, 0xfe, 0, 2, "Emulated Crosshair"},
	{0x02, 0x01, 0x01, 0x00, "Off"},
	{0x02, 0x01, 0x01, 0x01, "On"},
};

STDDIPINFO(Revx)

#include "midtunit_dma.h"

static void sound_sync()
{
	INT32 cyc = TMS34010TotalCycles() - Dcs2kTotalCycles();
	if (cyc > 0)
	{
		Dcs2kRun(cyc);
	}
}

static void sound_sync_end()
{
	INT32 cyc = (static_cast<double>(10000000) * 100 / nBurnFPS) - Dcs2kTotalCycles();
	if (cyc > 0)
	{
		Dcs2kRun(cyc);
	}
}

static UINT16 analog_inputs(INT32 select)
{
	switch (select)
	{
	case 0:
	case 2:
	case 4:
		return (0xff - BurnGunReturnX(select >> 1)) | 0xff00;
	case 1:
	case 3:
	case 5:
		return BurnGunReturnY(select >> 1) | 0xff00;
	}

	return 0xffff;
}

static UINT16 midxunit_uart_read(UINT32 offset)
{
	if (~offset & 1)
	{
		switch (offset >> 1)
		{
		case 0:
			return 0x13;

		case 1:
			{
				if (uart[1] == 0x66)
				{
					return 5;
				}
				sound_sync();
				UINT16 dcs = Dcs2kControlRead();
				return ((dcs & 0x800) >> 9) | ((~dcs & 0x400) >> 10);
			}
			break;

		case 3:
			{
				if (uart[1] == 0x66)
				{
					return uart[3];
				}
				return Dcs2kDataRead();
			}

		case 5:
			{
				if (uart[1] == 0x66)
				{
					return 5;
				}
				sound_sync();
				UINT16 dcs = Dcs2kControlRead();
				return ((dcs & 0x800) >> 11) | ((~dcs & 0x400) >> 8);
			}
			break;

		default:
			return uart[offset >> 1];
		}
	}

	return 0;
}

static void midxunit_uart_write(UINT32 offset, UINT16 data)
{
	if (~offset & 1)
	{
		switch (offset >> 1)
		{
		case 3:
			if (uart[1] == 0x66)
			{
				uart[3] = data & 0xff;
			}
			else
			{
				sound_sync();
				Dcs2kDataWrite(data & 0xff);
				Dcs2kRun(20);
			}
			break;

		case 5:
			sound_sync();
			Dcs2kDataRead();
			Dcs2kRun(20);
			break;

		default:
			uart[offset >> 1] = data;
			break;
		}
	}
}

static void midxunit_write(UINT32 address, UINT16 data)
{
	if ((address & 0xffc00000) == 0x00000000)
	{
		UINT32 offset = TOBYTE(address & 0x3fffff);
		auto vram = (UINT16*)DrvVidRAM;
		vram[offset + 0] = (nDMA[DMA_PALETTE] << 8) | (data & 0x00ff);
		vram[offset + 1] = (nDMA[DMA_PALETTE] & 0xff00) | ((data >> 8) & 0xff);
		return;
	}

	if ((address & 0xffc00000) == 0x00800000)
	{
		UINT32 offset = TOBYTE(address & 0x3fffff);
		auto vram = (UINT16*)DrvVidRAM;
		vram[offset + 0] = (vram[offset + 0] & 0x00ff) | ((data & 0xff) << 8);
		vram[offset + 1] = (vram[offset + 1] & 0x00ff) | (data & 0xff00);
		return;
	}

	if (address >= 0x40800000 && address <= 0x4fffffff)
	{
		INT32 offset = TOWORD(address - 0x40800000) / 0x40000;
		if (offset == 1)
		{
			Dcs2kResetWrite(data & 0x02);
		}
		return;
	}

	if ((address & 0xffffff80) == 0x60c00080)
	{
		if ((address & 0xff) > 0xdf)
		{
			security_bits = data & 0x0f;
		}
		else
		{
		}
		return;
	}

	if ((address & 0xfffffff0) == 0x60400000)
	{
		MidwaySerialPicWrite(((~data & 2) << 3) | security_bits);
		return;
	}

	if ((address & 0xfffffff0) == 0x80800000)
	{
		analog_port = data & ~8;
		return;
	}

	if ((address & 0xffffff00) == 0x80c00000)
	{
		midxunit_uart_write((address >> 4) & 0xf, data);
		return;
	}

	if ((address & 0xfff00000) == 0xa0800000)
	{
		if (~address & 0x10)
		{
			auto pal = (UINT16*)BurnPalRAM;
			INT32 offset = (address >> 5) & 0x7fff;
			pal[offset] = data;
			BurnPalette[offset] = BurnHighCol(pal5bit(data >> 10), pal5bit(data >> 5), pal5bit(data), 0);
		}
		return;
	}

	if ((address & 0xff8fff00) == 0xc0800000)
	{
		TUnitDmaWrite(address, data);
	}
}

static UINT16 midxunit_read(UINT32 address)
{
	if ((address & 0xffc00000) == 0x00000000)
	{
		UINT32 offset = TOBYTE(address & 0x3fffff);
		auto vram = (UINT16*)DrvVidRAM;
		return (vram[offset + 0] & 0xff) | (vram[offset + 1] << 8);
	}

	if ((address & 0xffc00000) == 0x00800000)
	{
		UINT32 offset = TOBYTE(address & 0x3fffff);
		auto vram = (UINT16*)DrvVidRAM;
		return (vram[offset + 0] >> 8) | (vram[offset + 1] & 0xff00);
	}

	if ((address & 0xffffffe0) == 0x60400000)
	{
		return (MidwaySerialPicStatus() << 1) | 1;
	}

	if (address >= 0x60c00000 && address <= 0x60c0007f)
	{
		switch ((address >> 5) & 7)
		{
		case 0:
		case 1:
		case 2:
			return DrvInputs[(address >> 5) & 3];

		case 3:
			return (DrvDips[1] << 8) | DrvDips[0];
		}

		return ~0;
	}

	if ((address & 0xffffffe0) == 0x60c000e0)
	{
		return MidwaySerialPicRead();
	}

	if ((address & 0xffffffe0) == 0x80800000)
	{
		return analog_inputs(analog_port);
	}

	if ((address & 0xffffff00) == 0x80c00000)
	{
		return midxunit_uart_read((address >> 4) & 0xf);
	}

	if ((address & 0xfff00000) == 0xa0800000)
	{
		auto pal = (UINT16*)BurnPalRAM;
		return pal[(address >> 5) & 0x7fff];
	}

	if ((address & 0xff8fff00) == 0xc0800000)
	{
		return TUnitDmaRead(address);
	}

	if (address >= 0xf8000000 && address <= 0xfeffffff)
	{
		INT32 offset = TOBYTE(address & 0x7ffffff);
		return DrvGfxROM[offset + 0] | (DrvGfxROM[offset + 1] << 8);
	}

	return 0xffff;
}

static INT32 scanline_callback(INT32 scanline, TMS34010Display* params)
{
	scanline -= params->veblnk; // top clipping
	if (scanline < 0 || scanline >= nScreenHeight || scanline >= 254) return 0;
	// last 2 lines on the screen are garbage.

	vb_start = params->vsblnk;

#if 0
	if (scanline == params->veblnk+1) {
		bprintf(0, _T("ENAB %d\n"), params->enabled);
		bprintf(0, _T("he %d\n"), params->heblnk*1);
		bprintf(0, _T("hs %d\n"), params->hsblnk*1);
		bprintf(0, _T("ve %d\n"), params->veblnk);
		bprintf(0, _T("vs %d\n"), params->vsblnk);
		bprintf(0, _T("vt %d\n"), params->vtotal);
		bprintf(0, _T("ht %d\n"), params->htotal);
	}
#endif

	UINT32 fulladdr = ((params->rowaddr << 16) | params->coladdr) >> 3;
	auto vram = (UINT16*)DrvVidRAM;
	UINT16* src = &vram[fulladdr & 0x3fe00];
	UINT16* dest = pTransDraw + scanline * nScreenWidth;

	INT32 heblnk = params->heblnk;
	INT32 hsblnk = params->hsblnk;

	if (!params->enabled) heblnk = hsblnk; // blank line!

	if ((hsblnk - heblnk) < nScreenWidth)
	{
		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			dest[x] = 0;
		}
	}

	for (INT32 x = params->heblnk; x < params->hsblnk; x++)
	{
		INT32 ex = x - params->heblnk;
		if (ex >= 0 && ex < nScreenWidth)
		{
			dest[ex] = src[fulladdr++ & 0x1ff];
		}
	}

	return 0;
}

static void to_shiftreg(UINT32 address, UINT16* shiftreg)
{
	auto vram = (UINT16*)DrvVidRAM;
	memcpy(shiftreg, &vram[address >> 3], 2 * 512 * sizeof(UINT16));
}

static void from_shiftreg(UINT32 address, UINT16* shiftreg)
{
	auto vram = (UINT16*)DrvVidRAM;
	memcpy(&vram[address >> 3], shiftreg, 2 * 512 * sizeof(UINT16));
}

static UINT16 midxunit_romredirect(UINT32 address)
{
	if ((address & 0xfffff000) == 0x20d31000)
	{
		UINT32 offset = TOWORD(address & 0xffffff);
		auto ram = (UINT16*)DrvTMSRAM;
		if (address == 0x20d31550 && TMS34010GetPC() == 0x20d31560 && ram[offset] == 0x58e)
		{
			return 0x078e;
		}
		return ram[offset];
	}

	return 0;
}

static UINT16 midxunit_romredirectp5(UINT32 address)
{
	if ((address & 0xfffff000) == 0x20d22000)
	{
		UINT32 offset = TOWORD(address & 0xffffff);
		auto ram = (UINT16*)DrvTMSRAM;
		if (address == 0x20d22870 && TMS34010GetPC() == 0x20d22880 && ram[offset] == 0x58e)
		{
			return 0x078e;
		}
		return ram[offset];
	}

	return 0;
}

static UINT8 revx_nvram_chunk[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xce, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xce, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00,
	0xfe, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00,
	0xfe, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00,
	0xfe, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static INT32 DrvDoReset()
{
	memset(AllRam, 0, RamEnd - AllRam);

	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();

	Dcs2kReset();

	security_bits = 0;
	analog_port = 0;
	memset(uart, 0, sizeof(uart));
	memset(nDMA, 0, sizeof(nDMA));

	nExtraCycles = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8* Next;
	Next = AllMem;

	DrvTMSROM = Next;
	Next += 0x0200000;
	DrvGfxROM = Next;
	Next += 0x1000000;

	DrvSndROM = Next;
	Next += 0x1000000;

	BurnPalette = (UINT32*)Next;
	Next += 0x008000 * sizeof(UINT32);

	DrvNVRAM = Next;
	Next += 0x0008000;

	AllRam = Next;

	DrvVidRAM = Next;
	Next += 0x0100000;
	DrvVRAM16 = (UINT16*)DrvVidRAM;
	dma_state = (dma_state_s*)Next;
	Next += sizeof(dma_state_s);

	BurnPalRAM = Next;
	Next += 0x0010000;
	DrvTMSRAM = Next;
	Next += 0x0200000;

	RamEnd = Next;

	MemEnd = Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(54.70);

	BurnAllocMemIndex();

	{
		INT32 k = 0;
		memset(DrvSndROM, 0xff, 0x1000000);
		if (BurnLoadRom(DrvSndROM + 0x000000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x200000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x400000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x600000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x800000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM + 0xa00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM + 0xc00000, k++, 2)) return 1;
		if (BurnLoadRom(DrvSndROM + 0xe00000, k++, 2)) return 1;

		if (BurnLoadRom(DrvTMSROM + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(DrvTMSROM + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(DrvTMSROM + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(DrvTMSROM + 0x000003, k++, 4)) return 1;

		k++; // skip pic load

		if (BurnLoadRom(DrvGfxROM + 0x000000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x000003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x200000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x200001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x200002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x200003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x400000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x400001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x400002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x400003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x600000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x600001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x600002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x600003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0x800000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x800001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x800002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0x800003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0xa00000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xa00001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xa00002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xa00003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0xc00000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xc00001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xc00002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xc00003, k++, 4)) return 1;

		if (BurnLoadRom(DrvGfxROM + 0xe00000, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xe00001, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xe00002, k++, 4)) return 1;
		if (BurnLoadRom(DrvGfxROM + 0xe00003, k++, 4)) return 1;
	}

	TMS34020Init(0);
	TMS34010Open(0);
	TMS34010MapMemory(DrvTMSRAM, 0x20000000, 0x20ffffff, MAP_RAM);
	TMS34010MapMemory(DrvNVRAM, 0xa0440000, 0xa047ffff, MAP_RAM);
	TMS34010MapMemory(DrvTMSROM, 0xff000000, 0xffffffff, MAP_ROM);
	TMS34010SetHandlers(0, midxunit_read, midxunit_write);

	// hot-fix for u76 failure @ boot -dink
	if (strstr(BurnDrvGetTextA(DRV_NAME), "revxp5"))
	{
		TMS34010UnmapMemory(0x20d22000, 0x20d22fff, MAP_ROM); // unmap page w/bad opcode
		TMS34010SetReadHandler(1, midxunit_romredirectp5);
		// replace with a handler to dish out proper opcode when needed
		TMS34010MapHandler(1, 0x20d22000, 0x20d22fff, MAP_ROM);
	}
	else
	{
		TMS34010UnmapMemory(0x20d31000, 0x20d31fff, MAP_ROM); // unmap page w/bad opcode
		TMS34010SetReadHandler(1, midxunit_romredirect); // replace with a handler to dish out proper opcode when needed
		TMS34010MapHandler(1, 0x20d31000, 0x20d31fff, MAP_ROM);
	}

	TMS34010SetToShift(to_shiftreg);
	TMS34010SetFromShift(from_shiftreg);
	TMS34010SetHaltOnReset(0);
	TMS34010SetPixClock(8000000, 1);
	TMS34010SetCpuCyclesPerFrame(40000000 / 4 * 100 / nBurnFPS);
	TMS34010SetScanlineRender(scanline_callback);
	TMS34010TimerSetCB(TUnitDmaCallback);
	TMS34010Close();

	MidwaySerialPicInit(419);
	MidwaySerialPicReset();

	Dcs2kInit(DCS_2KU, MHz(10));
	Dcs2kMapSoundROM(DrvSndROM, 0x1000000);
	Dcs2kSetVolume(5.50);

	GenericTilesInit();

	BurnGunInit(3, true);

	midtunit_cpurate = 40000000 / 4; // midtunit_dma.h

	DrvDoReset();

	// pre-calibrated gun data
	memcpy(DrvNVRAM + 0x2000, revx_nvram_chunk, sizeof(revx_nvram_chunk));

	return 0;
}

static INT32 DrvExit()
{
	BurnGunExit();
	GenericTilesExit();
	BurnFreeMemIndex();
	TMS34010Exit();

	Dcs2kExit();

	return 0;
}

static void DrvRecalculatePalette()
{
	auto ram = (UINT16*)BurnPalRAM;

	for (INT32 i = 0; i < BurnDrvGetPaletteEntries(); i++)
	{
		BurnPalette[i] = BurnHighCol(pal5bit(ram[i] >> 10), pal5bit(ram[i] >> 5), pal5bit(ram[i]), 0);
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc)
	{
		DrvRecalculatePalette();
		BurnRecalc = 0;
	}

	BurnTransferCopy(BurnPalette);

	if (DrvDips[2] & 1)
	{
		// The emulated crosshair is only needed for calibration.
		// The game has it's own set of crosshairs.
		BurnGunDrawTargets();
	}

	return 0;
}

static void HandleDCSIRQ(INT32 line)
{
	if (line == 0 || line == 96 || line == 192) DcsCheckIRQ();
}

static INT32 DrvFrame()
{
	if (DrvReset)
	{
		DrvDoReset();
	}

	{
		memset(DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++)
		{
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		BurnGunMakeInputs(0, Gun[0], Gun[1]);
		BurnGunMakeInputs(1, Gun[2], Gun[3]);
		BurnGunMakeInputs(2, Gun[4], Gun[5]);
	}

	TMS34010NewFrame();
	Dcs2kNewFrame();

	INT32 nInterleave = 289;
	INT32 nCyclesTotal[2] = {40000000 / 4 * 100 / nBurnFPS, (10000000 * 100) / nBurnFPS};
	INT32 nCyclesDone[2] = {nExtraCycles, 0};
	INT32 bDrawn = 0;
	TMS34010Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, TMS34010);
		TMS34010GenerateScanline(i);

		if (i == vb_start && pBurnDraw)
		{
			BurnDrvRedraw();
			bDrawn = 1;
		}

		HandleDCSIRQ(i);

		sound_sync(); // sync to main cpu
		if (i == nInterleave - 1)
			sound_sync_end();
	}

	nExtraCycles = TMS34010TotalCycles() - nCyclesTotal[0];

	TMS34010Close();

	if (pBurnDraw && bDrawn == 0)
	{
		BurnDrvRedraw();
	}

	if (pBurnSoundOut)
	{
		Dcs2kRender(pBurnSoundOut, nBurnSoundLen);
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32* pnMin)
{
	struct BurnArea ba;

	if (pnMin)
	{
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE)
	{
		memset(&ba, 0, sizeof(ba));
		ba.Data = AllRam;
		ba.nLen = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA)
	{
		TMS34010Scan(nAction);

		Dcs2kScan(nAction, pnMin);
		MidwaySerialPicScan(nAction, pnMin);

		BurnGunScan();

		SCAN_VAR(security_bits);
		SCAN_VAR(analog_port);
		SCAN_VAR(uart);
		SCAN_VAR(nDMA);

		SCAN_VAR(nExtraCycles);
	}

	if (nAction & ACB_NVRAM)
	{
		ba.Data = DrvNVRAM;
		ba.nLen = 0x8000;
		ba.nAddress = 0;
		ba.szName = "NV RAM";
		BurnAcb(&ba);
	}

	return 0;
}

// Revolution X (rev 1.0 6/16/94)

static struct BurnRomInfo revxRomDesc[] = {
	{"l1_revolution_x_sound_rom_u2.u2", 0x80000, 0xd2ed9f5e, 1 | BRF_SND}, //  0 DCS Sound Data
	{"l1_revolution_x_sound_rom_u3.u3", 0x80000, 0xaf8f253b, 1 | BRF_SND}, //  1
	{"l1_revolution_x_sound_rom_u4.u4", 0x80000, 0x3ccce59c, 1 | BRF_SND}, //  2
	{"l1_revolution_x_sound_rom_u5.u5", 0x80000, 0xa0438006, 1 | BRF_SND}, //  3
	{"l1_revolution_x_sound_rom_u6.u6", 0x80000, 0xb7b34f60, 1 | BRF_SND}, //  4
	{"l1_revolution_x_sound_rom_u7.u7", 0x80000, 0x6795fd88, 1 | BRF_SND}, //  5
	{"l1_revolution_x_sound_rom_u8.u8", 0x80000, 0x793a7eb5, 1 | BRF_SND}, //  6
	{"l1_revolution_x_sound_rom_u9.u9", 0x80000, 0x14ddbea1, 1 | BRF_SND}, //  7

	{"l1_revolution_x_game_rom_u51.u51", 0x80000, 0x9960ac7c, 2 | BRF_PRG | BRF_ESS}, //  8 TMS34020 Code
	{"l1_revolution_x_game_rom_u52.u52", 0x80000, 0xfbf55510, 2 | BRF_PRG | BRF_ESS}, //  9
	{"l1_revolution_x_game_rom_u53.u53", 0x80000, 0xa045b265, 2 | BRF_PRG | BRF_ESS}, // 10
	{"l1_revolution_x_game_rom_u54.u54", 0x80000, 0x24471269, 2 | BRF_PRG | BRF_ESS}, // 11

	{"419_revolution-x_u444.u444", 0x02000, 0x517e0110, 3 | BRF_PRG | BRF_OPT}, // 12 PIC Code

	{"p5_revolution_x_game_rom_u120.u120", 0x80000, 0x523af1f0, 4 | BRF_GRA}, // 13 Graphics Data
	{"p5_revolution_x_game_rom_u121.u121", 0x80000, 0x78201d93, 4 | BRF_GRA}, // 14
	{"p5_revolution_x_game_rom_u122.u122", 0x80000, 0x2cf36144, 4 | BRF_GRA}, // 15
	{"p5_revolution_x_game_rom_u123.u123", 0x80000, 0x6912e1fb, 4 | BRF_GRA}, // 16
	{"p5_revolution_x_game_rom_u110.u110", 0x80000, 0xe3f7f0af, 4 | BRF_GRA}, // 17
	{"p5_revolution_x_game_rom_u111.u111", 0x80000, 0x49fe1a69, 4 | BRF_GRA}, // 18
	{"p5_revolution_x_game_rom_u112.u112", 0x80000, 0x7e3ba175, 4 | BRF_GRA}, // 19
	{"p5_revolution_x_game_rom_u113.u113", 0x80000, 0xc0817583, 4 | BRF_GRA}, // 20
	{"p5_revolution_x_game_rom_u101.u101", 0x80000, 0x5a08272a, 4 | BRF_GRA}, // 21
	{"p5_revolution_x_game_rom_u102.u102", 0x80000, 0x11d567d2, 4 | BRF_GRA}, // 22
	{"p5_revolution_x_game_rom_u103.u103", 0x80000, 0xd338e63b, 4 | BRF_GRA}, // 23
	{"p5_revolution_x_game_rom_u104.u104", 0x80000, 0xf7b701ee, 4 | BRF_GRA}, // 24
	{"p5_revolution_x_game_rom_u91.u91", 0x80000, 0x52a63713, 4 | BRF_GRA}, // 25
	{"p5_revolution_x_game_rom_u92.u92", 0x80000, 0xfae3621b, 4 | BRF_GRA}, // 26
	{"p5_revolution_x_game_rom_u93.u93", 0x80000, 0x7065cf95, 4 | BRF_GRA}, // 27
	{"p5_revolution_x_game_rom_u94.u94", 0x80000, 0x600d5b98, 4 | BRF_GRA}, // 28
	{"p5_revolution_x_game_rom_u81.u81", 0x80000, 0x729eacb1, 4 | BRF_GRA}, // 29
	{"p5_revolution_x_game_rom_u82.u82", 0x80000, 0x19acb904, 4 | BRF_GRA}, // 30
	{"p5_revolution_x_game_rom_u83.u83", 0x80000, 0x0e223456, 4 | BRF_GRA}, // 31
	{"p5_revolution_x_game_rom_u84.u84", 0x80000, 0xd3de0192, 4 | BRF_GRA}, // 32
	{"p5_revolution_x_game_rom_u71.u71", 0x80000, 0x2b29fddb, 4 | BRF_GRA}, // 33
	{"p5_revolution_x_game_rom_u72.u72", 0x80000, 0x2680281b, 4 | BRF_GRA}, // 34
	{"p5_revolution_x_game_rom_u73.u73", 0x80000, 0x420bde4d, 4 | BRF_GRA}, // 35
	{"p5_revolution_x_game_rom_u74.u74", 0x80000, 0x26627410, 4 | BRF_GRA}, // 36
	{"p5_revolution_x_game_rom_u63.u63", 0x80000, 0x3066e3f3, 4 | BRF_GRA}, // 37
	{"p5_revolution_x_game_rom_u64.u64", 0x80000, 0xc33f5309, 4 | BRF_GRA}, // 38
	{"p5_revolution_x_game_rom_u65.u65", 0x80000, 0x6eee3e71, 4 | BRF_GRA}, // 39
	{"p5_revolution_x_game_rom_u66.u66", 0x80000, 0xb43d6fff, 4 | BRF_GRA}, // 40
	{"revx.u51", 0x80000, 0x9960ac7c, 4 | BRF_GRA}, // 41
	{"revx.u52", 0x80000, 0xfbf55510, 4 | BRF_GRA}, // 42
	{"revx.u53", 0x80000, 0xa045b265, 4 | BRF_GRA}, // 43
	{"revx.u54", 0x80000, 0x24471269, 4 | BRF_GRA}, // 44

	{"a-17722.u1", 0x00117, 0x054de7a3, 5 | BRF_OPT}, // 45 PLDs
	{"a-17721.u955", 0x00117, 0x033fe902, 5 | BRF_OPT}, // 46
	{"snd-gal16v8a.u17", 0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT}, // 47
};

STD_ROM_PICK(revx)
STD_ROM_FN(revx)

struct BurnDriver BurnDrvRevx = {
	"revx", nullptr, nullptr, nullptr, "1994",
	"Revolution X (rev 1.0 6/16/94)\0", nullptr, "Midway", "X Unit",
	nullptr, nullptr, nullptr, nullptr,
	BDF_GAME_WORKING, 3, HARDWARE_MIDWAY_XUNIT, GBF_SHOOT, 0,
	nullptr, revxRomInfo, revxRomName, nullptr, nullptr, nullptr, nullptr, RevxInputInfo, RevxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8000,
	400, 256, 4, 3
};


// Revolution X (prototype, rev 5.0 5/23/94)

static struct BurnRomInfo revxp5RomDesc[] = {
	{"p5_revolution_x_sound_rom_u2.u2", 0x80000, 0x4ed9e803, 1 | BRF_SND}, //  0 DCS Sound Data
	{"l1_revolution_x_sound_rom_u3.u3", 0x80000, 0xaf8f253b, 1 | BRF_SND}, //  1
	{"l1_revolution_x_sound_rom_u4.u4", 0x80000, 0x3ccce59c, 1 | BRF_SND}, //  2
	{"l1_revolution_x_sound_rom_u5.u5", 0x80000, 0xa0438006, 1 | BRF_SND}, //  3
	{"l1_revolution_x_sound_rom_u6.u6", 0x80000, 0xb7b34f60, 1 | BRF_SND}, //  4
	{"l1_revolution_x_sound_rom_u7.u7", 0x80000, 0x6795fd88, 1 | BRF_SND}, //  5
	{"l1_revolution_x_sound_rom_u8.u8", 0x80000, 0x793a7eb5, 1 | BRF_SND}, //  6
	{"l1_revolution_x_sound_rom_u9.u9", 0x80000, 0x14ddbea1, 1 | BRF_SND}, //  7

	{"p5_revolution_x_game_rom_u51.u51", 0x80000, 0xf3877eee, 2 | BRF_PRG | BRF_ESS}, //  8 TMS34020 Code
	{"p5_revolution_x_game_rom_u52.u52", 0x80000, 0x199a54d8, 2 | BRF_PRG | BRF_ESS}, //  9
	{"p5_revolution_x_game_rom_u53.u53", 0x80000, 0xfcfcf72a, 2 | BRF_PRG | BRF_ESS}, // 10
	{"p5_revolution_x_game_rom_u54.u54", 0x80000, 0xfd684c31, 2 | BRF_PRG | BRF_ESS}, // 11

	{"419_revolution-x_u444.u444", 0x02000, 0x517e0110, 3 | BRF_PRG | BRF_OPT}, // 12 PIC Code

	{"p5_revolution_x_game_rom_u120.u120", 0x80000, 0x523af1f0, 4 | BRF_GRA}, // 13 Graphics Data
	{"p5_revolution_x_game_rom_u121.u121", 0x80000, 0x78201d93, 4 | BRF_GRA}, // 14
	{"p5_revolution_x_game_rom_u122.u122", 0x80000, 0x2cf36144, 4 | BRF_GRA}, // 15
	{"p5_revolution_x_game_rom_u123.u123", 0x80000, 0x6912e1fb, 4 | BRF_GRA}, // 16
	{"p5_revolution_x_game_rom_u110.u110", 0x80000, 0xe3f7f0af, 4 | BRF_GRA}, // 17
	{"p5_revolution_x_game_rom_u111.u111", 0x80000, 0x49fe1a69, 4 | BRF_GRA}, // 18
	{"p5_revolution_x_game_rom_u112.u112", 0x80000, 0x7e3ba175, 4 | BRF_GRA}, // 19
	{"p5_revolution_x_game_rom_u113.u113", 0x80000, 0xc0817583, 4 | BRF_GRA}, // 20
	{"p5_revolution_x_game_rom_u101.u101", 0x80000, 0x5a08272a, 4 | BRF_GRA}, // 21
	{"p5_revolution_x_game_rom_u102.u102", 0x80000, 0x11d567d2, 4 | BRF_GRA}, // 22
	{"p5_revolution_x_game_rom_u103.u103", 0x80000, 0xd338e63b, 4 | BRF_GRA}, // 23
	{"p5_revolution_x_game_rom_u104.u104", 0x80000, 0xf7b701ee, 4 | BRF_GRA}, // 24
	{"p5_revolution_x_game_rom_u91.u91", 0x80000, 0x52a63713, 4 | BRF_GRA}, // 25
	{"p5_revolution_x_game_rom_u92.u92", 0x80000, 0xfae3621b, 4 | BRF_GRA}, // 26
	{"p5_revolution_x_game_rom_u93.u93", 0x80000, 0x7065cf95, 4 | BRF_GRA}, // 27
	{"p5_revolution_x_game_rom_u94.u94", 0x80000, 0x600d5b98, 4 | BRF_GRA}, // 28
	{"p5_revolution_x_game_rom_u81.u81", 0x80000, 0x729eacb1, 4 | BRF_GRA}, // 29
	{"p5_revolution_x_game_rom_u82.u82", 0x80000, 0x19acb904, 4 | BRF_GRA}, // 30
	{"p5_revolution_x_game_rom_u83.u83", 0x80000, 0x0e223456, 4 | BRF_GRA}, // 31
	{"p5_revolution_x_game_rom_u84.u84", 0x80000, 0xd3de0192, 4 | BRF_GRA}, // 32
	{"p5_revolution_x_game_rom_u71.u71", 0x80000, 0x2b29fddb, 4 | BRF_GRA}, // 33
	{"p5_revolution_x_game_rom_u72.u72", 0x80000, 0x2680281b, 4 | BRF_GRA}, // 34
	{"p5_revolution_x_game_rom_u73.u73", 0x80000, 0x420bde4d, 4 | BRF_GRA}, // 35
	{"p5_revolution_x_game_rom_u74.u74", 0x80000, 0x26627410, 4 | BRF_GRA}, // 36
	{"p5_revolution_x_game_rom_u63.u63", 0x80000, 0x3066e3f3, 4 | BRF_GRA}, // 37
	{"p5_revolution_x_game_rom_u64.u64", 0x80000, 0xc33f5309, 4 | BRF_GRA}, // 38
	{"p5_revolution_x_game_rom_u65.u65", 0x80000, 0x6eee3e71, 4 | BRF_GRA}, // 39
	{"p5_revolution_x_game_rom_u66.u66", 0x80000, 0xb43d6fff, 4 | BRF_GRA}, // 40
	{"p5_revolution_x_game_rom_u51.u51", 0x80000, 0xf3877eee, 4 | BRF_GRA}, // 41
	{"p5_revolution_x_game_rom_u52.u52", 0x80000, 0x199a54d8, 4 | BRF_GRA}, // 42
	{"p5_revolution_x_game_rom_u53.u53", 0x80000, 0xfcfcf72a, 4 | BRF_GRA}, // 43
	{"p5_revolution_x_game_rom_u54.u54", 0x80000, 0xfd684c31, 4 | BRF_GRA}, // 44

	{"a-17722.u1", 0x00117, 0x054de7a3, 5 | BRF_OPT}, // 45 PLDs
	{"a-17721.u955", 0x00117, 0x033fe902, 5 | BRF_OPT}, // 46
	{"snd-gal16v8a.u17", 0x00117, 0x00000000, 5 | BRF_NODUMP | BRF_OPT}, // 47
};

STD_ROM_PICK(revxp5)
STD_ROM_FN(revxp5)

struct BurnDriver BurnDrvRevxp5 = {
	"revxp5", "revx", nullptr, nullptr, "1994",
	"Revolution X (prototype, rev 5.0 5/23/94)\0", nullptr, "Midway", "X Unit",
	nullptr, nullptr, nullptr, nullptr,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 3, HARDWARE_MIDWAY_XUNIT, GBF_SHOOT, 0,
	nullptr, revxp5RomInfo, revxp5RomName, nullptr, nullptr, nullptr, nullptr, RevxInputInfo, RevxDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 0x8000,
	400, 256, 4, 3
};
