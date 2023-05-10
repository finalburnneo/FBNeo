// FinalBurn Neo Fairchild Channel F driver module
// Based on MAME driver by Juergen Buchmueller, Frank Palazzolo, Sean Riddle

#include "tiles_generic.h"
#include "f8.h"
#include "math.h" // for sound
#include "burn_pal.h"

static INT32 SM = 3; // screen size multiplier

/*
	bugs -
		Alien Invasion - p1/p2 controlls are swapped
		baseball - not working?
		multicarts - maze not working in the final version?
*/

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvMainROM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvCartRAM;
static UINT8 *DrvVideoRAM;

static INT32 latch[6];
static UINT8 row_reg;
static UINT8 col_reg;
static UINT8 val_reg;

static INT32 sound_mode;
static INT32 incr;
static float decay_mult;
static INT32 envelope;
static UINT32 sample_counter;
static INT32 forced_ontime;
static INT32 min_ontime;

static INT32 read_write;
static INT32 address_latch;

static INT32 base_bank;
static INT32 half_bank;
static INT32 has_halfbank = 0; // config

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo ChannelfInputList[] = {
	{"Time (1)",		BIT_DIGITAL,    DrvJoy1 + 0,    "keyb_1"	},
	{"Hold (2)",		BIT_DIGITAL,	DrvJoy1 + 1,	"keyb_2"	},
	{"Mode (3)",		BIT_DIGITAL,	DrvJoy1 + 2,	"keyb_3"	},
	{"Start (4)",		BIT_DIGITAL,	DrvJoy1 + 3,	"keyb_4"	},

	{"P1 Up",			BIT_DIGITAL,	DrvJoy2 + 3,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 right"	},
	{"P1 Push Down",	BIT_DIGITAL,	DrvJoy2 + 7,	"p1 fire 1"	},
	{"P1 Pull Up",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"	},
	{"P1 Twist Right",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 3"	},
	{"P1 Twist Left",	BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 4"	},

	{"P2 Up",			BIT_DIGITAL,	DrvJoy3 + 3,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 right"	},
	{"P2 Push Down",	BIT_DIGITAL,	DrvJoy3 + 7,	"p2 fire 1"	},
	{"P2 Pull Up",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"	},
	{"P2 Twist Right",	BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 3"	},
	{"P2 Twist Left",	BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 4"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		}, // 0x15
};

STDINPUTINFO(Channelf)

static struct BurnDIPInfo ChannelfDIPList[] =
{
	DIP_OFFSET(0x15)
	{0x00, 0xff, 0xff, 0x00, NULL							},

	// bits 0,1 reserved for future expansion of bios selection
	{0,	   0xfe, 0,	2, "Bios Select"						},
	{0x00, 0x01, 0x03, 0x00, "Fairchild"					},
	{0x00, 0x01, 0x03, 0x01, "Luxor Video Entertainment"	},

	{0,	   0xfe, 0, 2, "Region (Requires re-load)"			},
	{0x00, 0x01, 0x04, 0x00, "NTSC"							},
	{0x00, 0x01, 0x04, 0x04, "PAL"							},
};

STDDIPINFO(Channelf)

static void sound_stream_update(INT16 *output, INT32 samples)
{
	UINT32 target = 0;

	switch (sound_mode)
	{
		case 0: // sound off
			memset (output, 0, samples * 2 * sizeof(INT16));
		return;

		case 1: // high tone (2V) - 1000Hz
			target = 0x00010000;
		break;

		case 2: // medium tone (4V) - 500Hz
			target = 0x00020000;
		break;

		case 3: // low (weird) tone (32V & 8V)
			target = 0x00140000;
		break;
	}

	for (INT32 sampindex = 0; sampindex < samples; sampindex++)
	{
		output[sampindex*2+0] = output[sampindex*2+1] = ((forced_ontime > 0) || ((sample_counter & target) == target)) ? BURN_SND_CLIP(envelope) : 0;

		sample_counter += incr;
		envelope *= decay_mult;
		if (forced_ontime > 0)
			forced_ontime -= 1;
	}
}

static inline void sound_write(INT32 mode)
{
	if (mode != sound_mode)
	{
		sound_mode		= mode;
		envelope 		= sound_mode ? 0x3fff : 0;
		forced_ontime	= sound_mode ? min_ontime : 0;
	}
}

static void channelf_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf800) == 0x2800) {
		DrvMainRAM[address & 0x7ff] = data;
		return;
	}

	if ((address & 0xf000) == 0x3000) { // multi-cart
		base_bank =  data & 0x1f;
		if (has_halfbank) half_bank = (data & 0x20) >> 5;
		bprintf(0, _T("data %x  basebank %x  half_bank %x\n"), data, base_bank, half_bank);
		return;
	}
}

static UINT8 channelf_main_read(UINT16 address)
{
	if (address >= 0x800 && address < 0x2800) {
		return DrvMainROM[0x800 + (base_bank * 0x2000) + (half_bank * 0x1000) + (address - 0x800)];
	}

	if ((address & 0xf800) == 0x2800) {
		return DrvMainRAM[address & 0x7ff];
	}

	// 0000-07ff bios
	// 0800-27ff cart data
	// 2800-2fff scratch ram
	// 3000-ffff more cart data

	return DrvMainROM[address];
}

static void channelf_io_write(UINT8 port, UINT8 data)
{
	switch (port)
	{
		case 0x00:
			latch[0] = data;
			if (data & 0x20) DrvVideoRAM[(row_reg * 0x80) + col_reg] = val_reg & 0x03;
		return;

		case 0x01:
			latch[1] = data;
			val_reg = (data ^ 0xff) >> 6;
		return;

		case 0x04:
			latch[2] = data;
			col_reg = (data ^ 0xff) & 0x7f;
		return;

		case 0x05:
			latch[3] = data;
			sound_write((data & 0xc0) >> 6);
			row_reg = (data ^ 0xff) & 0x3f;
		return;

		case 0x20:
		case 0x24:
			latch[4] = data;
			read_write = data & 0x01;
			//address_latch = (address_latch & 0x3f3) | ((data & 0x04) << 2) | ((data & 0x02) << 3);
			address_latch = (address_latch & 0x3f3) | ((data & 0x04) << 0) | ((data & 0x02) << 2);
			if (read_write) DrvCartRAM[address_latch] = (data & 0x08) >> 3;
		return;

		case 0x21:
		case 0x25:
			latch[5] = data;
//			address_latch = (address_latch & 0x0c) | (data & 0x01) |
//				((data & 0x10) >> 3) | ((data & 0x7e) << 3);
			address_latch = (address_latch & 0x0c) | (data & 0x01) |
				((data & 0xe) << 3) | ((data & 0xe0) << 2) | ((data & 0x10) >> 3);
		return;//        0x70             0x380
	}
}

static inline UINT8 port_read_with_latch(UINT8 ext, UINT8 latch_state)
{
	return (~ext | latch_state);
}

static UINT8 channelf_io_read(UINT8 port)
{
	switch (port)
	{
		case 0x00: 
			return port_read_with_latch(DrvInputs[0] | 0xf0, latch[0]);

		case 0x01:
			return port_read_with_latch(DrvInputs[1] | (((latch[0] & 0x40) == 0) ? 0 : 0xc0), latch[1]);

		case 0x04:
			return port_read_with_latch(((latch[0] & 0x40) == 0) ? DrvInputs[2] : 0xff, latch[2]);

		case 0x05:
			return port_read_with_latch(0xff, latch[3]);

		case 0x20:
		case 0x24:
		{
			if (read_write == 0) {
				return (latch[4] & 0x7f) |
					((DrvCartRAM[address_latch] & 0x01) << 7);
			}
	
			return port_read_with_latch(0xff, latch[4]);
		}

		case 0x21:
		case 0x25:
			return port_read_with_latch(0xff, latch[5]);
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	BurnLoadRom(DrvMainROM, 0x81 + (DrvDips[0] & 3), 1); // load top half of bios

	F8Open(0);
	F8Reset();
	F8Close();

	read_write = 0;
	address_latch = 0;

	memset (latch, 0, sizeof(latch));
	row_reg = 0;
	col_reg = 0;
	val_reg = 0;

	base_bank = 0;
	half_bank = 0;

	// initialize sound
	incr = 65536.0 / (nBurnSoundRate / 1000.0 / 2.0);
	min_ontime = nBurnSoundRate / 1000 * 2;
	decay_mult = exp((-0.693 / 9e-3) / nBurnSoundRate);
	envelope = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM		= Next; Next += 0x040800;

	BurnPalette		= (UINT32*)Next; Next += BurnDrvGetPaletteEntries() * sizeof(UINT32);

	AllRam			= Next;

	DrvVideoRAM		= Next; Next += 0x002000;
	DrvCartRAM		= Next; Next += 0x000400;
	DrvMainRAM      = Next; Next += 0x000800;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate((DrvDips[0] & 0x04) ? 50.00 : 60.00);

	BurnAllocMemIndex();

	{
		if (BurnLoadRom(DrvMainROM + 0x0400, 0x80, 1)) return 1; // bios bottom
		if (BurnLoadRom(DrvMainROM + 0x0000, 0x81, 1)) return 1; // bios top

		if (BurnLoadRom(DrvMainROM + 0x0800, 0x00, 1)) return 1; // cart
	}

	F8Init();
	F8SetProgramWriteHandler(channelf_main_write);
	F8SetProgramReadHandler(channelf_main_read);
	F8SetIOWriteHandler(channelf_io_write);
	F8SetIOReadHandler(channelf_io_read);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	F8Exit();
	GenericTilesExit();

	BurnFreeMemIndex();

	has_halfbank = 0;

	return 0;
}

static void DrvPaletteInit()
{
	static UINT8 color_table[8][3] = {
		{0x10, 0x10, 0x10}, // black
		{0xfd, 0xfd, 0xfd}, // white
		{0xff, 0x31, 0x53}, // red
		{0x02, 0xcc, 0x5d}, // green
		{0x4b, 0x3f, 0xf3}, // blue
		{0xe0, 0xe0, 0xe0}, // lt gray
		{0x91, 0xff, 0xa6}, // lt green
		{0xce, 0xd0, 0xff}  // lt blue
	};

	static INT32 color_map[16] = {
		0, 1, 1, 1, 7, 4, 2, 3, 5, 4, 2, 3, 6, 4, 2, 3
	};

	for (INT32 i = 0; i < 16; i++) {
		BurnPalette[i] = BurnHighCol(color_table[color_map[i]][0], color_table[color_map[i]][1], color_table[color_map[i]][2], 0);
	}
}

static void draw_screen()
{ // 102 x 58
	for (INT32 y = 4*SM; y < 62*SM; y++)
	{
		INT32 palette_offset = (((DrvVideoRAM[((y/SM) * 128)+125] & 2) >> 1) | (DrvVideoRAM[((y/SM) * 128) + 126] & 2)) << 2;

		for (INT32 x = 4*SM; x < 106*SM; x++)
		{
			pTransDraw[(y - (4*SM)) * nScreenWidth + (x - (4*SM))] = palette_offset | DrvVideoRAM[(x/SM) + ((y/SM) * 128)];
		}
	}
}

static INT32 DrvDraw()
{
	if (BurnRecalc) {
		DrvPaletteInit();
		BurnRecalc = 0;
	}

	draw_screen();

	BurnTransferCopy(BurnPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		memset (DrvInputs, 0, sizeof(DrvInputs));

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = nBurnSoundLen; // for sound
	INT32 nCyclesTotal[1] = { ((DrvDips[0] & 0x04) ? 2000000 /*PAL*/ : 1789772 /*NTSC*/) / (nBurnFPS / 100) };
	INT32 nCyclesDone[1] = { 0 };
	INT32 nSoundBufferPos = 0;

	F8Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, F8);

		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			sound_stream_update(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			sound_stream_update(pSoundBuf, nSegmentLength);
		}
		BurnSoundDCFilter();
	}

	F8Close();

	if (pBurnDraw) {
		BurnDrvRedraw();
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

		F8Scan(nAction);

		SCAN_VAR(read_write);
		SCAN_VAR(address_latch);

		SCAN_VAR(latch);
		SCAN_VAR(row_reg);
		SCAN_VAR(col_reg);
		SCAN_VAR(val_reg);

		SCAN_VAR(sound_mode);
		SCAN_VAR(incr);
		SCAN_VAR(decay_mult);
		SCAN_VAR(envelope);
		SCAN_VAR(sample_counter);
		SCAN_VAR(forced_ontime);
		SCAN_VAR(min_ontime);

		SCAN_VAR(half_bank);
		SCAN_VAR(base_bank);
	}

	return 0;
}

static INT32 ChannelfGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		if (i == 1 && BurnDrvGetTextA(DRV_BOARDROM)) {
			pszGameName = BurnDrvGetTextA(DRV_BOARDROM);
		} else {
			pszGameName = BurnDrvGetTextA(DRV_PARENT);
		}
	}

	if (pszGameName == NULL || i > 2) {
		*pszName = NULL;
		return 1;
	}

	// remove chf_
	memset(szFilename, 0, MAX_PATH);
	for (UINT32 j = 0; j < (strlen(pszGameName) - 4); j++) {
		szFilename[j] = pszGameName[j + 4];
	}

	*pszName = szFilename;

	return 0;
}


static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

// Channel F Bios

static struct BurnRomInfo CHF_channelfRomDesc[] = {
	{ "sl31254.rom", 0x0400, 0x9c047ba3, 1 | BRF_BIOS | BRF_ESS }, // Channel F bios (bottom half)

	{ "sl31253.rom", 0x0400, 0x04694ed9, 1 | BRF_BIOS | BRF_ESS }, // Channel F bios (top half)
	{ "sl90025.rom", 0x0400, 0x015c1e38, 1 | BRF_BIOS | BRF_ESS }, // Luxor Video Entertainment System (top half)
};

STD_ROM_PICK(CHF_channelf)
STD_ROM_FN(CHF_channelf)

struct BurnDriver BurnDrvCHF_Channelf = {
	"chf_channelf", NULL, NULL, NULL, "1976",
	"Fairchild Channel F (Bios)\0", "BIOS only", "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 2, HARDWARE_CHANNELF, GBF_BIOS, 0,
	ChannelfGetZipName, CHF_channelfRomInfo, CHF_channelfRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Democart 1

static struct BurnRomInfo CHF_democrt1RomDesc[] = {
	{ "democrt1.bin",	0x00800, 0x42727ad5, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_democrt1, CHF_democrt1, CHF_channelf)
STD_ROM_FN(CHF_democrt1)

struct BurnDriver BurnDrvCHF_democrt1 = {
	"chf_democrt1", NULL, "chf_channelf", NULL, "1977",
	"Democart 1\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_DEMO, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_democrt1RomInfo, CHF_democrt1RomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Democart 2

static struct BurnRomInfo CHF_democrt2RomDesc[] = {
	{ "democrt2.bin",	0x00800, 0x44cf1d89, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_democrt2, CHF_democrt2, CHF_channelf)
STD_ROM_FN(CHF_democrt2)

struct BurnDriver BurnDrvCHF_democrt2 = {
	"chf_democrt2", NULL, "chf_channelf", NULL, "197?",
	"Democart 2\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_DEMO, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_democrt2RomInfo, CHF_democrt2RomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Tic-Tac-Toe / Shooting Gallery / Doodle / Quadra-Doodle

static struct BurnRomInfo CHF_tctactoeRomDesc[] = {
	{ "tctactoe.bin",	0x00800, 0xff4768b0, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_tctactoe, CHF_tctactoe, CHF_channelf)
STD_ROM_FN(CHF_tctactoe)

struct BurnDriver BurnDrvCHF_tctactoe = {
	"chf_tctactoe", NULL, "chf_channelf", NULL, "1976",
	"Tic-Tac-Toe / Shooting Gallery / Doodle / Quadra-Doodle\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_tctactoeRomInfo, CHF_tctactoeRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Muehle / Tontauben-Schiessen / Kreatives Malspiel / Videoscope (Ger)

static struct BurnRomInfo CHF_muehleRomDesc[] = {
	{ "muehle.bin",	0x00800, 0x7124dc59, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_muehle, CHF_muehle, CHF_channelf)
STD_ROM_FN(CHF_muehle)

struct BurnDriver BurnDrvCHF_muehle = {
	"chf_muehle", "chf_tctactoe", "chf_channelf", NULL, "1978",
	"Muehle / Tontauben-Schiessen / Kreatives Malspiel / Videoscope (Ger)\0", NULL, "SABA", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_muehleRomInfo, CHF_muehleRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Desert Fox / Shooting Gallery

static struct BurnRomInfo CHF_dsrtfoxRomDesc[] = {
	{ "dsrtfox.bin",	0x00800, 0x1570934b, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_dsrtfox, CHF_dsrtfox, CHF_channelf)
STD_ROM_FN(CHF_dsrtfox)

struct BurnDriver BurnDrvCHF_dsrtfox = {
	"chf_dsrtfox", NULL, "chf_channelf", NULL, "1976",
	"Desert Fox / Shooting Gallery\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_SHOOT, 0,
	ChannelfGetZipName, CHF_dsrtfoxRomInfo, CHF_dsrtfoxRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Video Blackjack

static struct BurnRomInfo CHF_vblckjckRomDesc[] = {
	{ "vblckjck.bin",	0x00800, 0xe98d4456, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_vblckjck, CHF_vblckjck, CHF_channelf)
STD_ROM_FN(CHF_vblckjck)

struct BurnDriver BurnDrvCHF_vblckjck = {
	"chf_vblckjck", NULL, "chf_channelf", NULL, "1976",
	"Video Blackjack\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_CASINO, 0,
	ChannelfGetZipName, CHF_vblckjckRomInfo, CHF_vblckjckRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Spitfire

static struct BurnRomInfo CHF_spitfireRomDesc[] = {
	{ "spitfire.bin",	0x00800, 0x5357c5f6, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_spitfire, CHF_spitfire, CHF_channelf)
STD_ROM_FN(CHF_spitfire)

struct BurnDriver BurnDrvCHF_spitfire = {
	"chf_spitfire", NULL, "chf_channelf", NULL, "1976",
	"Spitfire\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_SHOOT, 0,
	ChannelfGetZipName, CHF_spitfireRomInfo, CHF_spitfireRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Spitfire (Prototype)

static struct BurnRomInfo CHF_spitfirepRomDesc[] = {
	{ "spitfire (proto).bin",	0x00800, 0x0a728afc, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_spitfirep, CHF_spitfirep, CHF_channelf)
STD_ROM_FN(CHF_spitfirep)

struct BurnDriver BurnDrvCHF_spitfirep = {
	"chf_spitfirep", "chf_spitfire", "chf_channelf", NULL, "1976",
	"Spitfire (Prototype)\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_CHANNELF, GBF_SHOOT, 0,
	ChannelfGetZipName, CHF_spitfirepRomInfo, CHF_spitfirepRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Space War

static struct BurnRomInfo CHF_spacewarRomDesc[] = {
	{ "spacewar.bin",	0x00800, 0x22ef49e8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_spacewar, CHF_spacewar, CHF_channelf)
STD_ROM_FN(CHF_spacewar)

struct BurnDriver BurnDrvCHF_spacewar = {
	"chf_spacewar", NULL, "chf_channelf", NULL, "1977",
	"Space War\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_SHOOT, 0,
	ChannelfGetZipName, CHF_spacewarRomInfo, CHF_spacewarRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Math Quiz 1

static struct BurnRomInfo CHF_mthquiz1RomDesc[] = {
	{ "mthquiz1.bin",	0x00800, 0xbb4c24a2, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_mthquiz1, CHF_mthquiz1, CHF_channelf)
STD_ROM_FN(CHF_mthquiz1)

struct BurnDriver BurnDrvCHF_mthquiz1 = {
	"chf_mthquiz1", NULL, "chf_channelf", NULL, "1976",
	"Math Quiz 1\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_QUIZ, 0,
	ChannelfGetZipName, CHF_mthquiz1RomInfo, CHF_mthquiz1RomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Math Quiz 2

static struct BurnRomInfo CHF_mthquiz2RomDesc[] = {
	{ "mthquiz2.bin",	0x00800, 0x4aa7ea97, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_mthquiz2, CHF_mthquiz2, CHF_channelf)
STD_ROM_FN(CHF_mthquiz2)

struct BurnDriver BurnDrvCHF_mthquiz2 = {
	"chf_mthquiz2", NULL, "chf_channelf", NULL, "1977",
	"Math Quiz 2\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_QUIZ, 0,
	ChannelfGetZipName, CHF_mthquiz2RomInfo, CHF_mthquiz2RomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Magic Numbers / Mind Reader / Nim

static struct BurnRomInfo CHF_magicnumRomDesc[] = {
	{ "magicnum.bin",	0x00800, 0x24da0529, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_magicnum, CHF_magicnum, CHF_channelf)
STD_ROM_FN(CHF_magicnum)

struct BurnDriver BurnDrvCHF_magicnum = {
	"chf_magicnum", NULL, "chf_channelf", NULL, "1977",
	"Magic Numbers / Mind Reader / Nim\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_magicnumRomInfo, CHF_magicnumRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Drag Race

static struct BurnRomInfo CHF_dragraceRomDesc[] = {
	{ "dragrace.bin",	0x00800, 0x6a64dda3, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_dragrace, CHF_dragrace, CHF_channelf)
STD_ROM_FN(CHF_dragrace)

struct BurnDriver BurnDrvCHF_dragrace = {
	"chf_dragrace", NULL, "chf_channelf", NULL, "1977",
	"Drag Race\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_dragraceRomInfo, CHF_dragraceRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Maze / Jailbreak / Blind-man's-bluff / Trailblazer

static struct BurnRomInfo CHF_mazeRomDesc[] = {
	{ "maze.bin",	0x00800, 0x4d42b296, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_maze, CHF_maze, CHF_channelf)
STD_ROM_FN(CHF_maze)

struct BurnDriver BurnDrvCHF_maze = {
	"chf_maze", NULL, "chf_channelf", NULL, "1977",
	"Maze / Jailbreak / Blind-man's-bluff / Trailblazer\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_MAZE, 0,
	ChannelfGetZipName, CHF_mazeRomInfo, CHF_mazeRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Maze / Jailbreak / Blind-man's-bluff / Trailblazer (Alt)

static struct BurnRomInfo CHF_mazeaRomDesc[] = {
	{ "mazealt.bin",	0x00800, 0x0a948b61, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_mazea, CHF_mazea, CHF_channelf)
STD_ROM_FN(CHF_mazea)

struct BurnDriver BurnDrvCHF_mazea = {
	"chf_mazea", "chf_maze", "chf_channelf", NULL, "1977",
	"Maze / Jailbreak / Blind-man's-bluff / Trailblazer (Alt)\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CHANNELF, GBF_MAZE, 0,
	ChannelfGetZipName, CHF_mazeaRomInfo, CHF_mazeaRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Backgammon / Acey-Deucey

static struct BurnRomInfo CHF_backgammRomDesc[] = {
	{ "backgmmn.bin",	0x00800, 0xa1731b52, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_backgamm, CHF_backgamm, CHF_channelf)
STD_ROM_FN(CHF_backgamm)

struct BurnDriver BurnDrvCHF_backgamm = {
	"chf_backgamm", NULL, "chf_channelf", NULL, "1977",
	"Backgammon / Acey-Deucey\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_backgammRomInfo, CHF_backgammRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Baseball

static struct BurnRomInfo CHF_baseballRomDesc[] = {
	{ "baseball.bin",	0x00800, 0x01129bcd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_baseball, CHF_baseball, CHF_channelf)
STD_ROM_FN(CHF_baseball)

struct BurnDriver BurnDrvCHF_baseball = {
	"chf_baseball", NULL, "chf_channelf", NULL, "1977",
	"Baseball\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_SPORTSMISC, 0,
	ChannelfGetZipName, CHF_baseballRomInfo, CHF_baseballRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Robot War / Torpedo Alley

static struct BurnRomInfo CHF_robotwarRomDesc[] = {
	{ "robotwar.bin",	0x00800, 0x38241e4f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_robotwar, CHF_robotwar, CHF_channelf)
STD_ROM_FN(CHF_robotwar)

struct BurnDriver BurnDrvCHF_robotwar = {
	"chf_robotwar", NULL, "chf_channelf", NULL, "1977",
	"Robot War / Torpedo Alley\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_robotwarRomInfo, CHF_robotwarRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Robot War (Prototype)

static struct BurnRomInfo CHF_robotwarpRomDesc[] = {
	{ "robot war (proto).bin",	0x00800, 0x7f62a23c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_robotwarp, CHF_robotwarp, CHF_channelf)
STD_ROM_FN(CHF_robotwarp)

struct BurnDriver BurnDrvCHF_robotwarp = {
	"chf_robotwarp", "chf_robotwar", "chf_channelf", NULL, "1977",
	"Robot War (Prototype)\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_robotwarpRomInfo, CHF_robotwarpRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Sonar Search

static struct BurnRomInfo CHF_sonrsrchRomDesc[] = {
	{ "sonrsrch.bin",	0x00800, 0xdbdc56bf, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_sonrsrch, CHF_sonrsrch, CHF_channelf)
STD_ROM_FN(CHF_sonrsrch)

struct BurnDriver BurnDrvCHF_sonrsrch = {
	"chf_sonrsrch", NULL, "chf_channelf", NULL, "1977",
	"Sonar Search\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_sonrsrchRomInfo, CHF_sonrsrchRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Memory Match 1 & 2

static struct BurnRomInfo CHF_memoryRomDesc[] = {
	{ "mmrymtch.bin",	0x00800, 0x104b5e18, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_memory, CHF_memory, CHF_channelf)
STD_ROM_FN(CHF_memory)

struct BurnDriver BurnDrvCHF_memory = {
	"chf_memory", NULL, "chf_channelf", NULL, "1978",
	"Memory Match 1 & 2\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_memoryRomInfo, CHF_memoryRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Dodge It

static struct BurnRomInfo CHF_dodgeitRomDesc[] = {
	{ "dodgeit.bin",	0x00800, 0xe3c1811c, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_dodgeit, CHF_dodgeit, CHF_channelf)
STD_ROM_FN(CHF_dodgeit)

struct BurnDriver BurnDrvCHF_dodgeit = {
	"chf_dodgeit", NULL, "chf_channelf", NULL, "1978",
	"Dodge It\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_dodgeitRomInfo, CHF_dodgeitRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Pinball Challenge

static struct BurnRomInfo CHF_pinballRomDesc[] = {
	{ "pinball.bin",	0x00800, 0xc610b330, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_pinball, CHF_pinball, CHF_channelf)
STD_ROM_FN(CHF_pinball)

struct BurnDriver BurnDrvCHF_pinball = {
	"chf_pinball", NULL, "chf_channelf", NULL, "1978",
	"Pinball Challenge\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_BREAKOUT, 0,
	ChannelfGetZipName, CHF_pinballRomInfo, CHF_pinballRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Pinball Challenge (Alt)

static struct BurnRomInfo CHF_pinballaRomDesc[] = {
	{ "pinballa.bin",	0x00800, 0x7cadf0fd, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_pinballa, CHF_pinballa, CHF_channelf)
STD_ROM_FN(CHF_pinballa)

struct BurnDriver BurnDrvCHF_pinballa = {
	"chf_pinballa", "chf_pinball", "chf_channelf", NULL, "1978",
	"Pinball Challenge (Alt)\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_CHANNELF, GBF_BREAKOUT, 0,
	ChannelfGetZipName, CHF_pinballaRomInfo, CHF_pinballaRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Hangman

static struct BurnRomInfo CHF_hangmanRomDesc[] = {
	{ "hangman.bin",	0x00c00, 0x9238d6ce, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_hangman, CHF_hangman, CHF_channelf)
STD_ROM_FN(CHF_hangman)

struct BurnDriver BurnDrvCHF_hangman = {
	"chf_hangman", NULL, "chf_channelf", NULL, "1978",
	"Hangman\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_hangmanRomInfo, CHF_hangmanRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Ordtävling (Swe)

static struct BurnRomInfo CHF_ordtvlngRomDesc[] = {
	{ "ordtvlng.bin",	0x00c00, 0x3a386e79, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_ordtvlng, CHF_ordtvlng, CHF_channelf)
STD_ROM_FN(CHF_ordtvlng)

struct BurnDriver BurnDrvCHF_ordtvlng = {
	"chf_ordtvlng", "chf_hangman", "chf_channelf", NULL, "1978",
	"Ordtavling (Swe)\0", NULL, "Luxor", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_ordtvlngRomInfo, CHF_ordtvlngRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Rat' Mal (Ger)

static struct BurnRomInfo CHF_ratmalRomDesc[] = {
	{ "ratmal.bin",	0x00c00, 0x87752425, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_ratmal, CHF_ratmal, CHF_channelf)
STD_ROM_FN(CHF_ratmal)

struct BurnDriver BurnDrvCHF_ratmal = {
	"chf_ratmal", "chf_hangman", "chf_channelf", NULL, "197?",
	"Rat' Mal (Ger)\0", NULL, "SABA", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_ratmalRomInfo, CHF_ratmalRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Checkers

static struct BurnRomInfo CHF_checkersRomDesc[] = {
	{ "checkers.bin",	0x00800, 0xfdae7044, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_checkers, CHF_checkers, CHF_channelf)
STD_ROM_FN(CHF_checkers)

struct BurnDriver BurnDrvCHF_checkers = {
	"chf_checkers", NULL, "chf_channelf", NULL, "1978",
	"Checkers\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_STRATEGY, 0,
	ChannelfGetZipName, CHF_checkersRomInfo, CHF_checkersRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Video Whizball

static struct BurnRomInfo CHF_whizballRomDesc[] = {
	{ "vwhzball.bin",	0x00c00, 0x65fdfe49, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_whizball, CHF_whizball, CHF_channelf)
STD_ROM_FN(CHF_whizball)

struct BurnDriver BurnDrvCHF_whizball = {
	"chf_whizball", NULL, "chf_channelf", NULL, "1978",
	"Video Whizball\0", NULL, "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_whizballRomInfo, CHF_whizballRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Schach (Ger)

static struct BurnRomInfo CHF_schachRomDesc[] = {
	{ "schach.bin",	0x01800, 0x04fb6dce, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_schach, CHF_schach, CHF_channelf)
STD_ROM_FN(CHF_schach)

struct BurnDriver BurnDrvCHF_schach = {
	"chf_schach", NULL, "chf_channelf", NULL, "197?",
	"Schach (Ger)\0", NULL, "SABA", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_STRATEGY, 0,
	ChannelfGetZipName, CHF_schachRomInfo, CHF_schachRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Bowling

static struct BurnRomInfo CHF_bowlingRomDesc[] = {
	{ "bowling.bin",	0x00800, 0x94322c79, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_bowling, CHF_bowling, CHF_channelf)
STD_ROM_FN(CHF_bowling)

struct BurnDriver BurnDrvCHF_bowling = {
	"chf_bowling", NULL, "chf_channelf", NULL, "1978",
	"Bowling\0", NULL, "Fairchil", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_bowlingRomInfo, CHF_bowlingRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Slot Machine

static struct BurnRomInfo CHF_slotmchnRomDesc[] = {
	{ "slotmchn.bin",	0x00800, 0xb7eabd08, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_slotmchn, CHF_slotmchn, CHF_channelf)
STD_ROM_FN(CHF_slotmchn)

struct BurnDriver BurnDrvCHF_slotmchn = {
	"chf_slotmchn", NULL, "chf_channelf", NULL, "1980",
	"Slot Machine\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_CHANNELF, GBF_CASINO, 0,
	ChannelfGetZipName, CHF_slotmchnRomInfo, CHF_slotmchnRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Galactic Space Wars / Lunar Lander

static struct BurnRomInfo CHF_galacticRomDesc[] = {
	{ "galactic.bin",	0x00800, 0xc8ef3410, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_galactic, CHF_galactic, CHF_channelf)
STD_ROM_FN(CHF_galactic)

struct BurnDriver BurnDrvCHF_galactic = {
	"chf_galactic", NULL, "chf_channelf", NULL, "1980",
	"Galactic Space Wars / Lunar Lander\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_galacticRomInfo, CHF_galacticRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Galactic Space Wars (Prototype)

static struct BurnRomInfo CHF_galacticpRomDesc[] = {
	{ "glactic space war (proto).bin",	0x00800, 0xa61258a8, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_galacticp, CHF_galacticp, CHF_channelf)
STD_ROM_FN(CHF_galacticp)

struct BurnDriver BurnDrvCHF_galacticp = {
	"chf_galacticp", "chf_galactic", "chf_channelf", NULL, "1980",
	"Galactic Space Wars (Prototype)\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_CHANNELF, GBF_ACTION, 0,
	ChannelfGetZipName, CHF_galacticpRomInfo, CHF_galacticpRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Pro Football

static struct BurnRomInfo CHF_pfootbllRomDesc[] = {
	{ "pfootbll.bin",	0x01000, 0xa1ae99be, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_pfootbll, CHF_pfootbll, CHF_channelf)
STD_ROM_FN(CHF_pfootbll)

struct BurnDriver BurnDrvCHF_pfootbll = {
	"chf_pfootbll", NULL, "chf_channelf", NULL, "1981",
	"Pro Football\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_SPORTSMISC, 0,
	ChannelfGetZipName, CHF_pfootbllRomInfo, CHF_pfootbllRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Football (Prototype)

static struct BurnRomInfo CHF_footballRomDesc[] = {
	{ "football (proto).bin",	0x01000, 0x7e5dafea, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_football, CHF_football, CHF_channelf)
STD_ROM_FN(CHF_football)

struct BurnDriver BurnDrvCHF_football = {
	"chf_football", "chf_pfootbll", "chf_channelf", NULL, "1981",
	"Football (Prototype)\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 2, HARDWARE_CHANNELF, GBF_SPORTSMISC, 0,
	ChannelfGetZipName, CHF_footballRomInfo, CHF_footballRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Casino Poker

static struct BurnRomInfo CHF_casinopRomDesc[] = {
	{ "casinop.bin",	0x01000, 0x5aa30c12, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_casinop, CHF_casinop, CHF_channelf)
STD_ROM_FN(CHF_casinop)

struct BurnDriver BurnDrvCHF_casinop = {
	"chf_casinop", NULL, "chf_channelf", NULL, "1980",
	"Casino Poker\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_CHANNELF, GBF_CASINO, 0,
	ChannelfGetZipName, CHF_casinopRomInfo, CHF_casinopRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Draw Poker (Prototype)

static struct BurnRomInfo CHF_drawpkrRomDesc[] = {
	{ "draw poker (proto).bin",	0x01000, 0x619fcc00, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_drawpkr, CHF_drawpkr, CHF_channelf)
STD_ROM_FN(CHF_drawpkr)

struct BurnDriver BurnDrvCHF_drawpkr = {
	"chf_drawpkr", "chf_casinop", "chf_channelf", NULL, "1980",
	"Draw Poker (Prototype)\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_PROTOTYPE, 1, HARDWARE_CHANNELF, GBF_CASINO, 0,
	ChannelfGetZipName, CHF_drawpkrRomInfo, CHF_drawpkrRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Alien Invasion

static struct BurnRomInfo CHF_alieninvRomDesc[] = {
	{ "alieninv.bin",	0x01000, 0x85e00865, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_alieninv, CHF_alieninv, CHF_channelf)
STD_ROM_FN(CHF_alieninv)

struct BurnDriver BurnDrvCHF_alieninv = {
	"chf_alieninv", NULL, "chf_channelf", NULL, "1981",
	"Alien Invasion\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_SHOOT, 0,
	ChannelfGetZipName, CHF_alieninvRomInfo, CHF_alieninvRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Werbetextcassette

static struct BurnRomInfo CHF_werbetxtRomDesc[] = {
	{ "werbetextcassette.bin",	0x01000, 0xda351c39, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_werbetxt, CHF_werbetxt, CHF_channelf)
STD_ROM_FN(CHF_werbetxt)

struct BurnDriver BurnDrvCHF_werbetxt = {
	"chf_werbetxt", NULL, "chf_channelf", NULL, "198?",
	"Werbetextcassette\0", NULL, "Electronic-Partner", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_CHANNELF, GBF_MISC, 0,
	ChannelfGetZipName, CHF_werbetxtRomInfo, CHF_werbetxtRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Color Organ (Prototype)

static struct BurnRomInfo CHF_clrorganRomDesc[] = {
	{ "color organ (proto).bin",	0x00800, 0x3fa2d4eb, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_clrorgan, CHF_clrorgan, CHF_channelf)
STD_ROM_FN(CHF_clrorgan)

struct BurnDriver BurnDrvCHF_clrorgan = {
	"chf_clrorgan", NULL, "chf_channelf", NULL, "19??",
	"Color Organ (Prototype)\0", NULL, "Zircon", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_DEMO | BDF_PROTOTYPE, 1, HARDWARE_CHANNELF, GBF_MISC, 0,
	ChannelfGetZipName, CHF_clrorganRomInfo, CHF_clrorganRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Channel F Multi-Cart (Final)

static struct BurnRomInfo CHF_multicrtRomDesc[] = {
	{ "multigame rom.bin",	0x40000, 0xa1ecbd58, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_multicrt, CHF_multicrt, CHF_channelf)
STD_ROM_FN(CHF_multicrt)

static INT32 multicrtInit()
{
	has_halfbank = 1;

	return DrvInit();
}

struct BurnDriver BurnDrvCHF_multicrt = {
	"chf_multicrt", NULL, "chf_channelf", NULL, "2004",
	"Channel F Multi-Cart (Final)\0", NULL, "homebrew", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_CHANNELF, GBF_MISC, 0,
	ChannelfGetZipName, CHF_multicrtRomInfo, CHF_multicrtRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	multicrtInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};


// Channel F Multi-Cart (Older)

static struct BurnRomInfo CHF_multicrtoRomDesc[] = {
	{ "multigame older.bin",	0x40000, 0xad4ebe3f, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_multicrto, CHF_multicrto, CHF_channelf)
STD_ROM_FN(CHF_multicrto)

static INT32 multicrtoInit()
{
	return DrvInit();
}

struct BurnDriver BurnDrvCHF_multicrto = {
	"chf_multicrto", "chf_multicrt", "chf_channelf", NULL, "2004",
	"Channel F Multi-Cart (Older)\0", NULL, "homebrew", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 2, HARDWARE_CHANNELF, GBF_MISC, 0,
	ChannelfGetZipName, CHF_multicrtoRomInfo, CHF_multicrtoRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	multicrtoInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};

// Hockey + Tennis (bios rip)

static struct BurnRomInfo CHF_builtinRomDesc[] = {
	{ "Hockey + Tennis (1976)(Fairchild).bin",	0x00800, 0xabdd6d62, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_builtin, CHF_builtin, CHF_channelf)
STD_ROM_FN(CHF_builtin)

struct BurnDriver BurnDrvCHF_builtin = {
	"chf_builtin", NULL, "chf_channelf", NULL, "1976",
	"Hockey + Tennis\0", "Channel F built-in games", "Fairchild", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_CHANNELF, GBF_BALLPADDLE, 0,
	ChannelfGetZipName, CHF_builtinRomInfo, CHF_builtinRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};

// Lights Out (HB)

static struct BurnRomInfo CHF_lightsoutRomDesc[] = {
	{ "Lights Out (2004)(Riddle, Sean).bin",	0x00800, 0x666c0b4a, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_lightsout, CHF_lightsout, CHF_channelf)
STD_ROM_FN(CHF_lightsout)

struct BurnDriver BurnDrvCHF_lightsout = {
	"chf_lightsout", NULL, "chf_channelf", NULL, "2004",
	"Lights Out (HB)\0", NULL, "Sean Riddle", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_lightsoutRomInfo, CHF_lightsoutRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};

// Pac-Man (HB)

static struct BurnRomInfo CHF_pacmanRomDesc[] = {
	{ "Pac-Man (2004)(Blackbird - e5frog).bin",	0x01000, 0x172f5770, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_pacman, CHF_pacman, CHF_channelf)
STD_ROM_FN(CHF_pacman)

struct BurnDriver BurnDrvCHF_pacman = {
	"chf_pacman", NULL, "chf_channelf", NULL, "2004",
	"Pac-Man (HB)\0", NULL, "Blackbird - e5frog", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_CHANNELF, GBF_MAZE, 0,
	ChannelfGetZipName, CHF_pacmanRomInfo, CHF_pacmanRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};

// Pac-Man (HB, v2)

static struct BurnRomInfo CHF_pacmanv2RomDesc[] = {
	{ "Pac-Man v2 (2004)(Blackbird - e5frog).bin",	0x02000, 0xa22dbf73, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_pacmanv2, CHF_pacmanv2, CHF_channelf)
STD_ROM_FN(CHF_pacmanv2)

struct BurnDriver BurnDrvCHF_pacmanv2 = {
	"chf_pacmanv2", "chf_pacman", "chf_channelf", NULL, "2004",
	"Pac-Man (HB, v2)\0", NULL, "Blackbird - e5frog", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HOMEBREW, 2, HARDWARE_CHANNELF, GBF_MAZE, 0,
	ChannelfGetZipName, CHF_pacmanv2RomInfo, CHF_pacmanv2RomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};

// Tetris (HB)

static struct BurnRomInfo CHF_tetrisRomDesc[] = {
	{ "Tetris (2004)(Trauner, Peter).bin",	0x00DFF, 0x2b900e96, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_tetris, CHF_tetris, CHF_channelf)
STD_ROM_FN(CHF_tetris)

struct BurnDriver BurnDrvCHF_tetris = {
	"chf_tetris", NULL, "chf_channelf", NULL, "2004",
	"Tetris (HB)\0", NULL, "Peter Trauner", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 2, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_tetrisRomInfo, CHF_tetrisRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};

// Wrdl-F (HB)

static struct BurnRomInfo CHF_wrdlfRomDesc[] = {
	{ "Wrdl-F (2022)(Arlasoft).rom",	0x01FE1, 0x0ae07b86, BRF_PRG | BRF_ESS },
};

STDROMPICKEXT(CHF_wrdlf, CHF_wrdlf, CHF_channelf)
STD_ROM_FN(CHF_wrdlf)

struct BurnDriver BurnDrvCHF_wrdlf = {
	"chf_wrdlf", NULL, "chf_channelf", NULL, "2022",
	"Wrdl-F (HB)\0", NULL, "Arlasoft", "Channel F",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HOMEBREW, 1, HARDWARE_CHANNELF, GBF_PUZZLE, 0,
	ChannelfGetZipName, CHF_wrdlfRomInfo, CHF_wrdlfRomName, NULL, NULL, NULL, NULL, ChannelfInputInfo, ChannelfDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &BurnRecalc, 16,
	102*SM, 58*SM, 4, 3
};
