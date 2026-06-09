// FinalBurn Neo Little Robin driver module
// Based on MAME driver by Pierpaolo Prazzoli and David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "tms34_intf.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *Drv68KROM;
static UINT8 *DrvSndROM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvTMSRAM;
static UINT8 *DrvVidRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *palette[3];

static UINT8 pal_index[2];
static UINT8 int_index[2];

static INT32 shiftfull;

// dink sound.
#include "stream.h"
static Stream stream7500;

static INT32 sound_spf_counter;
static UINT32 sound_timer;
static UINT8 sound_index[2];
static UINT16 sound_pointer[2];
static INT32 sound_flop[2];

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[3];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo LittlerbInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 10,	"p1 coin"		},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 8,	"p1 start"		},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"			},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"		},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"		},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 11,	"p2 coin"		},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 start"		},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"			},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"		},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 fire 3"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
	{"Dip C",			BIT_DIPSWITCH,	DrvDips + 2,	"dip"			},
};

STDINPUTINFO(Littlerb)

static struct BurnDIPInfo LittlerbDIPList[]=
{
	DIP_OFFSET(0x13)
	{0x00, 0xff, 0xff, 0x25, NULL					},
	{0x01, 0xff, 0xff, 0xef, NULL					},
	{0x02, 0xff, 0xff, 0x1f, NULL					},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x00, 0x01, 0x03, 0x03, "2"					},
	{0x00, 0x01, 0x03, 0x01, "3"					},
	{0x00, 0x01, 0x03, 0x02, "4"					},
	{0x00, 0x01, 0x03, 0x00, "5"					},

	{0   , 0xfe, 0   ,    8, "Coin B"				},
	{0x00, 0x01, 0x1c, 0x00, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0x1c, 0x10, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0x1c, 0x08, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0x1c, 0x18, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0x1c, 0x04, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0x1c, 0x14, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0x1c, 0x0c, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0x1c, 0x1c, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    8, "Coin A"				},
	{0x00, 0x01, 0xe0, 0x00, "5 Coins/1 Credit"		},
	{0x00, 0x01, 0xe0, 0x80, "4 Coins/1 Credit"		},
	{0x00, 0x01, 0xe0, 0x40, "3 Coins/1 Credit"		},
	{0x00, 0x01, 0xe0, 0xc0, "2 Coins/1 Credit"		},
	{0x00, 0x01, 0xe0, 0x20, "1 Coin/1 Credit"		},
	{0x00, 0x01, 0xe0, 0xa0, "1 Coin/2 Credits"		},
	{0x00, 0x01, 0xe0, 0x60, "1 Coin/3 Credits"		},
	{0x00, 0x01, 0xe0, 0xe0, "1 Coin/4 Credits"		},

	{0   , 0xfe, 0   ,    2, "Unknown"				},
	{0x01, 0x01, 0x01, 0x01, "Off"					},
	{0x01, 0x01, 0x01, 0x00, "On"					},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x01, 0x01, 0x06, 0x06, "Every 150,000"		},
	{0x01, 0x01, 0x06, 0x02, "Every 200,000"		},
	{0x01, 0x01, 0x06, 0x04, "Every 300,000"		},
	{0x01, 0x01, 0x06, 0x00, "None"					},

	{0   , 0xfe, 0   ,    8, "Difficulty"			},
	{0x01, 0x01, 0x38, 0x38, "Easy"					},
	{0x01, 0x01, 0x38, 0x18, "Medium Easy"			},
	{0x01, 0x01, 0x38, 0x28, "Normal"				},
	{0x01, 0x01, 0x38, 0x08, "Medium Hard"			},
	{0x01, 0x01, 0x38, 0x30, "Hard"					},
	{0x01, 0x01, 0x38, 0x10, "Harder"				},
	{0x01, 0x01, 0x38, 0x20, "Very Hard"			},
	{0x01, 0x01, 0x38, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x01, 0x01, 0x40, 0x40, "Off"					},
	{0x01, 0x01, 0x40, 0x00, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x01, 0x01, 0x80, 0x00, "Off"					},
	{0x01, 0x01, 0x80, 0x80, "On"					},

	{0   , 0xfe, 0   ,    2, "???"					},
	{0x02, 0x01, 0x10, 0x10, "Off"					},
	{0x02, 0x01, 0x10, 0x00, "On"					},
};

STDDIPINFO(Littlerb)


static void sound_render(INT16 **streams, INT32 len)
{
	INT16 *dest = streams[0];

	const int spf = stream7500.samples_to_source(nBurnSoundLen);

	for (INT32 i = 0; i < len; i++)
	{
		dest[i]  = 0;
		dest[i] += (0x80 - DrvSndROM[(0 * 0x40000) + (sound_index[0] * 0x400) + sound_pointer[0]]) * 0x7f;
		dest[i] += (0x80 - DrvSndROM[(1 * 0x40000) + (sound_index[1] * 0x400) + sound_pointer[1]]) * 0x7f;

		sound_pointer[0] = (sound_pointer[0] + 1) & 0x3ff;
		sound_pointer[1] = (sound_pointer[1] + 1) & 0x3ff;

		if (++sound_spf_counter >= (spf/2)) {
			sound_spf_counter = 0;
			sound_timer++;
		}
	}
}

static void sync_cpu()
{
	INT32 cyc = (((INT64)SekTotalCycles() * (48000000 / 8)) / 8000000) - TMS34010TotalCycles();
	if (cyc > 0) {
		TMS34010Run(cyc);
	}
}

static void __fastcall littlerb_main_write_word(UINT32 address, UINT16 data)
{
	if (address < 0x100000) return;

	switch (address)
	{
		case 0x700000:
		case 0x700002:
		case 0x700004:
		case 0x700006:
			sync_cpu();
			TMS34010HostWrite((address / 2) & 3, data);
		return;

		case 0x740000:
		case 0x760000: {
			const INT32 channel = (address >> 17) & 1;
			stream7500.update();
			sound_index[channel] = (data >> (sound_flop[channel] << 3)) & 0xff;
			sound_pointer[channel] = 0;
			sound_flop[channel] ^= 1;
		}
		return;

		case 0x780000: // outputs
			return;
	}
	bprintf(0, _T("mww %x  %x\n"), address, data);

}

static void __fastcall littlerb_main_write_byte(UINT32 address, UINT8 data)
{
	if (address < 0x100000) return;

	switch (address)
	{
		case 0x700000:
		case 0x700001:
		case 0x700002:
		case 0x700003:
		case 0x700004:
		case 0x700005:
		case 0x700006:
		case 0x700007:
			sync_cpu();
			TMS34010HostWriteMask((address / 2) & 3, data << ((~address&1) << 3), 0xff << ((~address&1) << 3));
		return;

		case 0x780000: // outputs
		case 0x780001:
		return;
	}
	bprintf(0, _T("mwb %x  %x\n"), address, data);

}

static UINT16 __fastcall littlerb_main_read_word(UINT32 address)
{
	switch (address)
	{
		case 0x700000:
		case 0x700002:
		case 0x700004:
		case 0x700006:
			sync_cpu();
			return TMS34010HostRead((address / 2) & 3);

		case 0x7c0000:
			return DrvDips[0] | (DrvDips[1] << 8);

		case 0x7e0000:
			stream7500.update();
			return (DrvInputs[0] & 0x1fff) | ((sound_timer & 7) << 13);

		case 0x7e0002:
			return DrvInputs[1];
	}
	bprintf(0, _T("mrw %x\n"), address);

	return 0;
}

static UINT8 __fastcall littlerb_main_read_byte(UINT32 address)
{
	switch (address)
	{
		case 0x700000:
		case 0x700001:
		case 0x700002:
		case 0x700003:
		case 0x700004:
		case 0x700005:
		case 0x700006:
		case 0x700007:
			sync_cpu();
			return TMS34010HostRead((address / 2) & 3) >> ((~address&1) << 3);

		case 0x7c0000:
		case 0x7c0001:
			return DrvDips[~address & 1];

		case 0x7e0000:
			stream7500.update();
			return ((DrvInputs[0] & 0x1fff) | ((sound_timer & 7) << 13)) >> 8;
		case 0x7e0001:
			return DrvInputs[0];

		case 0x7e0002:
		case 0x7e0003:
			return DrvInputs[1];
	}

	bprintf(0, _T("mrb %x\n"), address);
	return 0;
}

static void reg_increment(INT32 inc_type)
{
	int_index[inc_type]++;
	if (int_index[inc_type] == 3)
	{
		int_index[inc_type] = 0;
		pal_index[inc_type]++;
	}
}

static void littlerb_sub_write(UINT32 address, UINT16 data)
{
	switch (address & ~0xf)
	{
		case 0x4000000:
			pal_index[0] = data;
			int_index[0] = 0;
		return;

		case 0x4000010:
			palette[int_index[0] & 3][pal_index[0]] = data;
			reg_increment(0);
		return;

		case 0x4000030:
			pal_index[1] = data;
			int_index[1] = 0;
		return;
	}
}

static UINT16 littlerb_sub_read(UINT32 address)
{
	switch (address & ~0xf)
	{
		case 0x4000010:
		{
			UINT8 ret;
			ret = palette[int_index[1] & 3][pal_index[1]];
			reg_increment(1);
			return ret;
		}
	}

	return 0xffff;
}

static void tms_to_shift(UINT32 address, UINT16 *dst)
{
	if (shiftfull == 0)
	{
		UINT16 *ram = (UINT16*)DrvVidRAM;
		memcpy (dst, &ram[(address & ~0x1fff) >> 4], 0x400);
		shiftfull = 1;
	}
}

static void tms_from_shift(UINT32 address, UINT16 *src)
{
	UINT16 *ram = (UINT16*)DrvVidRAM;
	memcpy (&ram[(address & ~0x1fff) >> 4], src, 0x400);

	shiftfull = 0;
}

static INT32 scanline_cb(INT32 s_scanline, TMS34010Display *params)
{
	UINT16 *ram = (UINT16*)DrvVidRAM;
	UINT16 *vram = &ram[(params->rowaddr << 8) & 0x3ff00];
	s_scanline -= params->veblnk; // top clip
	UINT16 *dst = pTransDraw + s_scanline * nScreenWidth;

#if 0
	if (s_scanline == params->veblnk) {
		bprintf(0, _T("video ENAB %d\n"), params->enabled);
		bprintf(0, _T("htotal %d  (not always correct)\n"), params->htotal);
		bprintf(0, _T("he %d\n"), params->heblnk);
		bprintf(0, _T("hs %d\n"), params->hsblnk);
		bprintf(0, _T("calculated htotal (hs-he) %d\n"), params->hsblnk - params->heblnk);

		bprintf(0, _T("vtotal %d  (not always correct)\n"), params->vtotal);
		bprintf(0, _T("ve %d\n"), params->veblnk);
		bprintf(0, _T("vs %d\n"), params->vsblnk);
		bprintf(0, _T("calculated vtotal (vs-ve) %d\n"), params->vsblnk - params->veblnk);
	}
#endif

	if (s_scanline < 0 || s_scanline >= nScreenHeight) return 0;

	INT32 coladdr = params->coladdr;

	for (INT32 x = params->heblnk; x < params->hsblnk; x += 2)
	{
		UINT16 pixels = vram[coladdr++ & 0xff];
		INT32 dx = x - params->heblnk; // side clip

		if (dx < 0 || (dx+1) >= nScreenWidth) continue;

		dst[dx + 0] = pixels & 0xff;
		dst[dx + 1] = pixels >> 8;
	}

	return 0;
}

static void m68k_gen_int(INT32 state)
{
	SekSetIRQLine(4, state ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	TMS34010Open(0);
	TMS34010Reset();
	TMS34010Close();

	memset (pal_index, 0, sizeof(pal_index));
	memset (int_index, 0, sizeof(int_index));

	shiftfull = 0;

	sound_spf_counter = 0;
	sound_timer = 0;
	memset (sound_index, 0, sizeof(sound_index));
	memset (sound_pointer, 0, sizeof(sound_pointer));
	memset (sound_flop, 0, sizeof(sound_flop));

	HiscoreReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x100000;

	DrvSndROM	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0100 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x010000;
	DrvTMSRAM	= Next; Next += 0x080000;
	DrvVidRAM	= Next; Next += 0x080000;

	palette[0]	= Next; Next += 0x000100;
	palette[1]	= Next; Next += 0x000100;
	palette[2]	= Next; Next += 0x000100;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(30.00);

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(Drv68KROM + 0x000000, 0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM + 0x000001, 1, 2)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x000000, 2, 1)) return 1;
		if (BurnLoadRom(DrvSndROM + 0x040000, 3, 1)) return 1;
	}

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,		0x000000, 0x0fffff, MAP_ROM);
	SekMapMemory(Drv68KRAM,		0x200000, 0x203fff, MAP_RAM);
	SekSetWriteWordHandler(0,	littlerb_main_write_word);
	SekSetWriteByteHandler(0,	littlerb_main_write_byte);
	SekSetReadWordHandler(0,	littlerb_main_read_word);
	SekSetReadByteHandler(0,	littlerb_main_read_byte);
	SekClose();

	TMS34010Init(0);
	TMS34010Open(0);
	TMS34010MapMemory(DrvVidRAM,	0x00000000, 0x003fffff, MAP_RAM);
	TMS34010MapMemory(DrvTMSRAM,	0x7fc00000, 0x7fffffff, MAP_RAM);
	TMS34010MapMemory(DrvTMSRAM,	0xffc00000, 0xffffffff, MAP_RAM); // mirror
	TMS34010SetHandlers(0,			littlerb_sub_read, littlerb_sub_write);
	TMS34010SetScanlineRender(scanline_cb);
	TMS34010SetToShift(tms_to_shift);
	TMS34010SetFromShift(tms_from_shift);
	TMS34010SetOutputINT(m68k_gen_int);
	TMS34010SetPixClock(48000000 / 12, 2); // probably wrong
	TMS34010SetCpuCyclesPerFrame((INT32)(48000000 / 8 / 30));
	TMS34010SetHaltOnReset(1);
	TMS34010Close();

	stream7500.init(7680, nBurnSoundRate, 1, 0, sound_render);
    stream7500.set_volume(0.65);
	stream7500.set_buffered(SekTotalCycles, 8000000);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	stream7500.exit();

	SekExit();
	TMS34010Exit();

	BurnFreeMemIndex();

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x100; i++)
	{
		DrvPalette[i] = BurnHighCol(palette[0][i], palette[1][i], palette[2][i], 0);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	SekNewFrame();
	TMS34010NewFrame();

	{
		memset (DrvInputs, 0xff, sizeof(DrvInputs));

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 314;
	INT32 nCyclesTotal[2] = { 8000000 / 30, 48000000 / 8 / 30 }; // tms is actually 40,000,000 overclock a bit
	INT32 nCyclesDone[2] = { 0, 0 };

	SekOpen(0);
	TMS34010Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, Sek);
		CPU_RUN_SYNCINT(1, TMS34010);
		TMS34010GenerateScanline(i);
	}

	TMS34010Close();
	SekClose();

	if (pBurnSoundOut) {
		stream7500.render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin != NULL) {
		*pnMin = 0x029698;
	}

	if (nAction & ACB_MEMORY_RAM) {
		ScanVar(AllRam, RamEnd-AllRam, "All Ram");
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		TMS34010Scan(nAction);

		SCAN_VAR(pal_index);
		SCAN_VAR(int_index);

		SCAN_VAR(shiftfull);

		SCAN_VAR(sound_spf_counter);
		SCAN_VAR(sound_timer);
		SCAN_VAR(sound_index);
		SCAN_VAR(sound_pointer);
		SCAN_VAR(sound_flop);
	}

	return 0;
}


// Little Robin

static struct BurnRomInfo littlerbRomDesc[] = {
	{ "tch_1.u53",		0x080000, 0x172fbc13,  1 | BRF_PRG | BRF_ESS },	//  0 68K Code
	{ "tch_2.u29",		0x080000, 0xb2fb1d61,  1 | BRF_PRG | BRF_ESS },	//  1 

	{ "tch_3.u26",		0x040000, 0xf193c5b6,  2 | BRF_SND },			//  2 Samples
	{ "tch_4.u32",		0x040000, 0xd6b81583,  2 | BRF_SND },			//  3 
};

STD_ROM_PICK(littlerb)
STD_ROM_FN(littlerb)

struct BurnDriver BurnDrvLittlerb = {
	"littlerb", NULL, NULL, NULL, "1994",
	"Little Robin\0", NULL, "TCH", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, littlerbRomInfo, littlerbRomName, NULL, NULL, NULL, NULL, LittlerbInputInfo, LittlerbDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x100,
	338, 288, 4, 3
};
