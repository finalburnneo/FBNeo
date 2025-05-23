// FBNEO NES/Famicom and FDS emulator
// (c)2019-2020 dink, kev, iq_132, Gab75

#include "tiles_generic.h"
#include "bitswap.h"
#include "m6502_intf.h"
#include "nes_apu.h"
#include "ay8910.h"			// mapper 69 (sunsoft / gimmick!)
#include "burn_ym2413.h"	// mapper 85 (vrc7 / lagrange point)
#include "burn_gun.h"		// zapper games
#include "burn_led.h"		// fds disk status
#if !defined (_MSC_VER)
#include <unistd.h>         // for unlink()/rename() in ips_patch()/ips_make()
#endif
#include <errno.h>          // .. unlink()/rename()
#include "nes.h"

UINT8 NESDrvReset = 0;
static UINT32 DrvPalette[(0x40 * 8) /*nes*/ + 0x100 /*icons*/];
UINT8 NESRecalc;

UINT8 NESJoy1[8]     = { 0, 0, 0, 0, 0, 0, 0, 0 };
UINT8 NESJoy2[8]     = { 0, 0, 0, 0, 0, 0, 0, 0 };
UINT8 NESJoy3[8]     = { 0, 0, 0, 0, 0, 0, 0, 0 };
UINT8 NESJoy4[8]     = { 0, 0, 0, 0, 0, 0, 0, 0 };
UINT8 NESCoin[8]     = { 0, 0, 0, 0, 0, 0, 0, 0 };
UINT8 NESDips[4]     = { 0, 0, 0, 0 };
static UINT8 DrvInputs[4]   = { 0, 0, 0, 0 };

static UINT32 JoyShifter[2] = { 0, 0 };
static UINT8 JoyStrobe      = 0;

// Zapper emulation
INT16 ZapperX;
INT16 ZapperY;
UINT8 ZapperFire;
UINT8 ZapperReload;
UINT8 ZapperReloadTimer;

// FDS-Buttons
UINT8 FDSEject;
UINT8 FDSSwap;

// FDS-Timer (for insert/swap robot)
static UINT32 FDSFrameCounter;
static UINT32 FDSFrameAction;
static UINT32 FDSFrameDiskIcon;

// game-specific options not found in header.  see bottom of cartridge_load()
static UINT32 NESMode = 0;
#define NO_WORKRAM		0x0001 // 6000-7fff reads data openbus
#define BUS_CONFLICTS	0x0002 // rom conflicts with writes, needs special handling
#define ALT_MMC3        0x0004 // alternate mmc3 scanline timing
#define BAD_HOMEBREW    0x0008 // writes OAMDATA while rendering (can't possibly work on a real NES/FC)
#define USE_4SCORE      0x0100 // 4-Player device (NES)
#define USE_HORI4P      0x0200 // 4-Player device Mode 2 (Famicom)
#define USE_ZAPPER      0x0400 // Zapper Gun device
#define MAPPER_NOCLEAR  0x0800 // preserve mapper regs on soft-reset
#define IS_PAL          0x1000 // PAL-mode (not fully supported)
#define IS_FDS          0x2000 // Famicom Disk System
#define SHOW_OVERSCAN   0x4000 // - for debugging -
#define ALT_TIMING      0x8000 // for games that use "BIT PPUSTATUS; BIT PPUSTATUS; BPL -"
							   // Assimilate, Star Wars, full_palette.nes, etc.
#define ALT_TIMING2     0x0080 // Don Doko Don 2 doesn't like the nmi delay that gunnac, b-wings, etc needs.
#define VS_ZAPPER		0x0010 // VS. UniSystem Zapper
#define VS_REVERSED     0x0020 // VS. p1/p2 -> p2/p1 (inputs swapped)
#define RAM_RANDOM      0x0040 // Init. ram w/random bytes (Go! Dizzy Go!)
#define APU_HACKERY    0x10000 // Sam's Journey likes to clock the apu sweep gen via writes to 2017
#define FLASH_EEPROM   0x20000 // Flash eeprom (flashrom), loads / saves .ips file

// Usually for Multi-Cart mappers
static UINT32 RESETMode = 0;
#define RESET_POWER     0x0001
#define RESET_BUTTON    0x0002

// PPU types
enum {
	RP2C02 = 0, // NES/FC
	RP2C03,     // VS. (... and below!)
	RP2C04A,
	RP2C04B,
	RP2C04C,
	RP2C04D,
	RP2C05A,
	RP2C05B,
	RP2C05C,
	RP2C05D,
	RP2C05E
};

static const UINT8 PPUTypes[][6] = {
	{ "2C02" },     // NES/FC
	{ "2C03" },     // VS. (... and below!)
	{ "2C04A" },
	{ "2C04B" },
	{ "2C04C" },
	{ "2C04D" },
	{ "2C05A" },
	{ "2C05B" },
	{ "2C05C" },
	{ "2C05D" },
	{ "2C05E" }
};

static INT32 PPUType;

static UINT8* NES_CPU_RAM;

static UINT8* rom;
static INT32 cyc_counter = 0; // frame-based cycle counter
static UINT64 mega_cyc_counter = 0; // "from reset"-based cycle counter

static UINT8 cpu_open_bus;
static UINT32 scanline, scanline_row, pixel;
static UINT8 (*read_nt)(UINT16 address);
static void (*write_nt)(UINT16 address, UINT8 data);

enum Scanline  { VISIBLE = 0, POSTRENDER, VBLANK, PRERENDER };
enum Mirroring { VERTICAL = 0, HORIZONTAL, SINGLE_LOW, SINGLE_HIGH, FOUR_SCREEN, SACHEN };

// Reference: https://wiki.nesdev.com/w/index.php?title=PPU_registers

enum PPUCTRL {
	ctrl_ntbase     = (1 << 0) | (1 << 1),
	ctrl_incr       = 1 << 2,
	ctrl_sprtab     = 1 << 3,
	ctrl_bgtab      = 1 << 4,
	ctrl_sprsize    = 1 << 5,
	ctrl_slave      = 1 << 6,
	ctrl_nmi        = 1 << 7
};

enum PPUMASK {
	mask_greyscale	= 1 << 0,
	mask_bgleft		= 1 << 1,
	mask_sprleft	= 1 << 2,
	mask_bg			= 1 << 3,
	mask_spr		= 1 << 4,
	mask_red		= 1 << 5,
	mask_green		= 1 << 6,
	mask_blue		= 1 << 7
};

enum PPUSTATUS {
	status_openbus  = 0x1f,
	status_spovrf   = 1 << 5,
	status_sp0hit   = 1 << 6,
	status_vblank   = 1 << 7
};

struct OAMBUF
{
	UINT8 idx;   // # in OAM
	UINT8 x;     // following 4 bytes (x, y, tile, attr) mirror oam_ram
	UINT8 y;
	UINT8 tile;
	UINT8 attr;
	UINT8 tileL; // tile data from CHR
	UINT8 tileH;
};

struct cartridge {
	UINT8   *Cart;
	UINT8   *CartOrig; // for mapper 30 ips saves
	INT32    CartSize;

	UINT8	*PRGRom;
	INT32	 PRGRomSize;
	INT32	 PRGRomMask;

	UINT8	*WorkRAM;
	INT32	 WorkRAMSize;
	INT32	 WorkRAMMask;
	INT32	 BatteryBackedSRAM;

	UINT8	*CHRRom;
	UINT32	 CHRRomSize;
	UINT8	*CHRRam;
	UINT32	 CHRRamSize;

	UINT8   *ExtData;
	INT32    ExtDataSize;

	INT32	 Mapper;
	INT32	 SubMapper;
	INT32	 Mirroring;
	INT32	 Trainer;
	UINT32	 Crc;

	INT32	 FDSMode;
	INT32	 FDSDiskSides;
	UINT8	*FDSDiskRawOrig;	// FDS Disk loaded (unmodified)
	INT32    FDSDiskRawOrigSize;

	UINT8	*FDSDiskRaw;		// FDS Disk loaded (possibly modified)
	INT32    FDSDiskRawSize;
	UINT8	*FDSDisk;			// Same as above, but ptr adjusted to skip the header (if avail.)
	INT32    FDSDiskSize;
};

static cartridge Cart;

// forward decl's exposed from ppu (used in/by mappers)
static void set_mirroring(INT32 mode);
static void nametable_map(INT32 nt, INT32 ntbank);
static void nametable_mapraw(INT32 nt, UINT8 *ntraw, UINT8 type);
static void nametable_mapall(INT32 ntbank0, INT32 ntbank1, INT32 ntbank2, INT32 ntbank3);

static UINT8 ppu_ctrl;
static UINT8 ppu_mask;
static UINT8 ppu_status;
#define RENDERING (ppu_mask & (mask_bg | mask_spr)) // show bg (0x08) + show sprites (0x10)

// mapper stuff
enum { MEM_RAM = 0, MEM_RAM_RO = 1, MEM_ROM = 2 };
static UINT8* chr_maps[4];
static UINT32* chr_size_maps[4];
static UINT8* prg_maps[4];

static void mapper_set_chrtype(UINT32 type);

static INT32 mapper_init(INT32 mappernum);
static UINT8 (*mapper_prg_read)(UINT16 addr);
static UINT8 mapper_prg_read_int(UINT16 address); // mapper_prg_read points here by default
static void (*mapper_write)(UINT16 address, UINT8 data) = NULL;
static void (*mapper_map)() = NULL;
static void (*mapper_scanline)() = NULL;                // once per scanline
static void (*mapper_cycle)() = NULL;                   // once per cpu-cycle
static void (*mapper_ppu_clock)(UINT16 busaddr) = NULL; // called during visible & prerender
static void (*mapper_ppu_clockall)(UINT16 busaddr) = NULL; // called during every cycle
static void (*mapper_scan_cb)() = NULL;                 // state scanning
static void (*mapper_scan_cb_nvram)() = NULL;           // state scanning (nvram)

// loading & saving FDS Disk & Mapper 30 (flash eeprom) Carts
static void LoadIPSPatch(TCHAR *desc, UINT8 *rom_, INT32 rom_size);
static void SaveIPSPatch(TCHAR *desc, UINT8 *rom_orig, UINT8 *rom_, INT32 rom_size);

static void setup_maps()
{
	// set-up PRG/CHR mapping pointers
	chr_maps[MEM_RAM] = Cart.CHRRam;
	chr_maps[MEM_RAM_RO] = Cart.CHRRam;
	chr_maps[MEM_ROM] = Cart.CHRRom;

	chr_size_maps[MEM_RAM] = &Cart.CHRRamSize;
	chr_size_maps[MEM_RAM_RO] = &Cart.CHRRamSize;
	chr_size_maps[MEM_ROM] = &Cart.CHRRomSize;

	prg_maps[MEM_RAM] = Cart.WorkRAM;
	prg_maps[MEM_RAM_RO] = Cart.WorkRAM;
	prg_maps[MEM_ROM] = Cart.PRGRom;
}

static INT32 cartridge_load(UINT8* ROMData, UINT32 ROMSize, UINT32 ROMCRC)
{
	if (ROMData == NULL || ROMSize < 16384 ) {
		bprintf(0, _T("Invalid ROM data!\n"));
		return 1;
	}

	if (memcmp("NES\x1a", &ROMData[0], 4)) {
		bprintf(0, _T("Invalid ROM header!\n"));
		return 1;
	}

	NESMode = 0;

	INT32 nes20 = (ROMData[7] & 0xc) == 0x8;

	memset(&Cart, 0, sizeof(Cart));

	Cart.Cart = ROMData;
	Cart.CartSize = ROMSize;
	Cart.CartOrig = BurnMalloc(ROMSize);
	memcpy(Cart.CartOrig, Cart.Cart, ROMSize);

	Cart.Crc = ROMCRC;
	Cart.PRGRomSize = ROMData[4] * 0x4000;
	Cart.CHRRomSize = ROMData[5] * 0x2000;

	if (Cart.Crc == 0x2a798367) {
		// JY 45-in-1 can't be represented by regular nes header.
		Cart.CHRRomSize = 128 * 0x4000;
	}

	PPUType = RP2C02;

	if (nes20 && (ROMData[7] & 0x3) == 1) {
		switch (ROMData[13] & 0x0f) {
			case 0:
			case 1:
			case 6:
			case 7: PPUType = RP2C03; break;
			case 2: PPUType = RP2C04A; break;
			case 3: PPUType = RP2C04B; break;
			case 4: PPUType = RP2C04C; break;
			case 5: PPUType = RP2C04D; break;
			case 8: PPUType = RP2C05A; break;
			case 9: PPUType = RP2C05B; break;
			case 0xa: PPUType = RP2C05C; break;
			case 0xb: PPUType = RP2C05D; break;
			case 0xc: PPUType = RP2C05E; break;
		}
		bprintf(0, _T("VS. System - PPU %S (%x)\n"), PPUTypes[PPUType], PPUType);
		ROMData[6] |= 8; // "fix to 4-screen mode" - nesdev wiki
	}

	bprintf(0, _T("PRG Size: %d\n"), Cart.PRGRomSize);
	bprintf(0, _T("CHR Size: %d\n"), Cart.CHRRomSize);

	if (ROMData[6] & 0x8)
		Cart.Mirroring = 4;
	else
		Cart.Mirroring = ROMData[6] & 1;

	switch (Cart.Mirroring) {
		case 0: set_mirroring(HORIZONTAL); break;
		case 1: set_mirroring(VERTICAL); break;
		case 4: set_mirroring(FOUR_SCREEN); break;
	}

	// Parse MAPPER iNES + NES 2.0
	Cart.Mapper = (ROMData[6] >> 4) | (ROMData[7] & 0xf0);

	if (!memcmp("DiskDude!", &ROMData[7], 9)) {
		bprintf(PRINT_ERROR, _T("``DiskDude!'' spam, ignoring upper bits of mapper.\n"));

		Cart.Mapper &= 0x0f; // remove spam from upper bits of mapper
	}

	if (nes20) {
		Cart.Mapper |= (ROMData[8] & 0x0f) << 8;
		Cart.SubMapper = (ROMData[8] & 0xf0) >> 4;

		if (Cart.Mapper & 0xf00 || Cart.SubMapper != 0)
			bprintf(0, _T("NES 2.0 Extended Mapper: %d\tSub: %d\n"), Cart.Mapper, Cart.SubMapper);
	}

	Cart.BatteryBackedSRAM = (ROMData[6] & 0x2) >> 1;

	// Mapper EXT-hardware inits
	// Initted here, because mapper_init() is called on reset
	if (Cart.Mapper == 30 || Cart.Mapper == 406 || Cart.Mapper == 451) { // UNIROM-512 (30), Haradius Zero (406), Haratyler (451) ips patch
		if (Cart.BatteryBackedSRAM) {
			NESMode |= FLASH_EEPROM; // enable IPS loads/saves
			Cart.BatteryBackedSRAM = 0; // disable sram
		}
	}

	if (NESMode & FLASH_EEPROM) {
		TCHAR desc[64];
		_stprintf(desc, _T("Mapper %d"), Cart.Mapper);

		// Load IPS patch (aka: flasheeprom-saves @ exit)
		LoadIPSPatch(desc, Cart.Cart, Cart.CartSize);
	}

	if (Cart.Mapper == 69) { // SunSoft fme-7 (5b) audio expansion - ay8910
		AY8910Init(0, 1789773 / 2, 1);
		AY8910SetAllRoutes(0, 0.70, BURN_SND_ROUTE_BOTH);
		AY8910SetBuffered(M6502TotalCycles, 1789773);
	}

	if (Cart.Mapper == 85) { // VRC7 audio expansion - YM2413
		BurnYM2413Init(3579545, 1);
		BurnYM2413SetAllRoutes(2.00, BURN_SND_ROUTE_BOTH);
	}

	Cart.Trainer = (ROMData[6] & 0x4) >> 2;
	Cart.PRGRom = ROMData + 0x10 + (Cart.Trainer ? 0x200 : 0);

	// Default CHR-Ram size (8k), always set-up (for advanced mappers, etc)
	Cart.CHRRamSize = 0x2000;

	if (nes20) {
		// NES2.0 header specifies CHR-Ram size (Nalle Land, Haunted Halloween '86, VS. Topgun, VS. Castlevania)
		Cart.CHRRamSize = 64 << (ROMData[0xb] & 0xf);

		// Detect Extended Data (Super Russian Roulette..)
		INT32 total_nes_rom = 0x10 + (Cart.Trainer ? 0x200 : 0) + Cart.PRGRomSize + Cart.CHRRomSize;
		Cart.ExtDataSize = (ROMSize > total_nes_rom) ? (ROMSize - total_nes_rom) : 0;

		if (Cart.ExtDataSize) {
			bprintf(0, _T("%x bytes Extended Data detected\n"), Cart.ExtDataSize);
			Cart.ExtData = ROMData + total_nes_rom;
		}
	}

	if (Cart.Mapper == 30 || Cart.Mapper == 111) { // UNROM-512, GTROM(Cheapocabra) defaults to 32k chr-ram
		Cart.CHRRamSize = 0x8000;
	}

	if (Cart.Crc == 0xf0847322) {
		Cart.CHRRamSize = 0x4000; // 16k chr-ram for Videomation
	}

	if (Cart.Crc == 0xdd65a6cc) { // Street Heroes 262
		Cart.Mapper = 262;
	}

	Cart.CHRRam = (UINT8*)BurnMalloc(Cart.CHRRamSize);

	if (Cart.CHRRomSize) {
		Cart.CHRRom = Cart.PRGRom + Cart.PRGRomSize;
		mapper_set_chrtype(MEM_ROM);
		bprintf(0, _T("Using ROM-supplied CHR data\n"));
	} else {
		mapper_set_chrtype(MEM_RAM);
		bprintf(0, _T("Using RAM CHR data (%dk bytes)\n"), Cart.CHRRamSize / 1024);
	}

	// set-up Cart PRG-RAM/WORK-RAM (6000-7fff)

	Cart.WorkRAMSize = (Cart.Mapper != 5) ? 0x2000 : (8 * 0x2000);

	if (nes20) {
		// NES2.0 header specifies NV-Ram size (ex. Nova the Squirrel)
		INT32 l_shift0 = ((ROMData[0xa] & 0xf0) >> 4); // sram
		INT32 l_shift1 = ((ROMData[0xa] & 0x0f) >> 0); // work-ram
		INT32 use = (l_shift0 > l_shift1) ? l_shift0 : l_shift1; // use the larger of the 2?
		Cart.WorkRAMSize = (use == 0) ? 0 : (64 << use);
	}

	if (PPUType > RP2C02) Cart.WorkRAMSize = 0x800; // VS. 6000-7fff 2k (mirrored)

	switch (ROMCRC) {
		case 0x478a04d8:
			Cart.WorkRAMSize = 0x4000; // Genghis Khan 16k SRAM/WRAM
			break;

		case 0x6f5f3bd2:
		case 0x6f8906ad:
			Cart.WorkRAMSize = 0x8000; // Final Fantasy 1 & 2 (Japan/T-eng) 32k SRAM/WRAM
			break;
	}

	bprintf(0, _T("Cartridge RAM: %d\n"), Cart.WorkRAMSize);
	Cart.WorkRAM = (UINT8*)BurnMalloc(Cart.WorkRAMSize);
	if (Cart.WorkRAMSize == 0) NESMode |= NO_WORKRAM;

	if (Cart.Trainer) {
		// This is not a trainer in the traditional sense.  It was a little
		// block of ram on early nes/fc copy-machines to simulate certain
		// mappers with code.
		bprintf(0, _T("ROM has Trainer code, mapping @ 0x7000.\n"));
		if (Cart.WorkRAMSize == 0x2000) {
			memcpy(Cart.WorkRAM + 0x1000, ROMData + 0x10, 0x200);
		} else {
			bprintf(PRINT_ERROR, _T("Invalid WorkRam size, can't use Trainer data.\n"));
		}
	}

	setup_maps();

	// set-up MAPPER
	bprintf(0, _T("Cartridge Mapper: %d   Mirroring: "), Cart.Mapper);
	switch (Cart.Mirroring) {
		case 0: bprintf(0, _T("Horizontal")); break;
		case 1: bprintf(0, _T("Vertical")); break;
		case 4: bprintf(0, _T("Four-Screen")); break;
		default: bprintf(0, _T("???")); break;
	}
	bprintf(0, _T(" (%d)\n"), Cart.Mirroring);

	if (mapper_init(Cart.Mapper) == -1) {
		bprintf(0, _T("Unsupported mapper\n"));
		return 1;
	}

	if (Cart.BatteryBackedSRAM) {
		bprintf(0, _T("Game has battery-backed sRAM\n"));
	}

	Cart.PRGRomMask = Cart.PRGRomSize - 1;

	Cart.WorkRAMMask = Cart.WorkRAMSize - 1;
	if (Cart.WorkRAMSize > 0x2000) {
		// we need to mask addr 6000-7fff, any thing larger than 0x2000
		// will need the default mask of 0x1fff for this to work properly.
		// note: the extra ram still can be used by mappers, etc.
		// note2: VS. System uses 0x800 bytes wram, mirrored across 6000-7fff
		Cart.WorkRAMMask = 0x1fff;
	}

	// Game-specific stuff:
	// Mapper 7 or 4-way mirroring usually gets no workram (6000-7fff)
	if (Cart.Mapper == 7 || (Cart.Mirroring == 4 && ((PPUType == RP2C02) && !Cart.BatteryBackedSRAM ) )) {
		NESMode |= NO_WORKRAM; // VS. is exempt from this limitation.
	}

	NESMode |= (ROMCRC == 0xab29ab28) ? BUS_CONFLICTS : 0; // Dropzone
	NESMode |= (ROMCRC == 0xe3a6d7f6) ? BUS_CONFLICTS : 0; // Cybernoid
	NESMode |= (ROMCRC == 0x552a903a) ? BUS_CONFLICTS : 0; // Huge Insect
	NESMode |= (ROMCRC == 0xb90a1ca1) ? NO_WORKRAM : 0; // Low G Man
	NESMode |= (ROMCRC == 0xa905cc12) ? NO_WORKRAM : 0; // Bill & Ted
	NESMode |= (ROMCRC == 0x45b1869a) ? APU_HACKERY : 0; // rgbleek
	NESMode |= (ROMCRC == 0xa94b4cbb) ? APU_HACKERY : 0; // famidash
	NESMode |= (ROMCRC == 0x6a90da54) ? APU_HACKERY : 0; // famidashb
	NESMode |= (ROMCRC == 0xc00c4ea5) ? APU_HACKERY : 0; // Sam's Journey
	NESMode |= (ROMCRC == 0x585f3500) ? ALT_MMC3 : 0; // Darkwing Duck (T-Chi)
	NESMode |= (ROMCRC == 0x2d826113) ? ALT_MMC3 : 0; // cybercoaster
	NESMode |= (ROMCRC == 0x38f65b2d) ? BAD_HOMEBREW : 0; // Battler (HB)
	NESMode |= (ROMCRC == 0x7946fe78) ? ALT_TIMING : 0; // ftkantaro53
	NESMode |= (ROMCRC == 0xf167590d) ? ALT_TIMING : 0; // jay and silent bob (occasional hang on level transition w/o this)
	NESMode |= (ROMCRC == 0x273aeace) ? ALT_TIMING : 0; // full_nes_palette
	NESMode |= (ROMCRC == 0x6673e5da) ? ALT_TIMING : 0; // t.atom
	NESMode |= (ROMCRC == 0xd5e10c90) ? ALT_TIMING : 0; // astro boy
	NESMode |= (ROMCRC == 0xc9f3f439) ? ALT_TIMING : 0; // vs. freedom force
	NESMode |= (ROMCRC == 0x53eb8950) ? ALT_TIMING : 0; // freedom force
	NESMode |= (ROMCRC == 0x560142bc) ? ALT_TIMING2 : 0; // don doko don 2
	NESMode |= (ROMCRC == 0xe39e0be2) ? ALT_TIMING2 : 0; // laser invasion
	NESMode |= (ROMCRC == 0x47d22165) ? BAD_HOMEBREW : 0; // animal clipper
	NESMode |= (ROMCRC == 0x70eac605) ? ALT_TIMING : 0; // deblock
	NESMode |= (ROMCRC == 0x3616c7dd) ? ALT_TIMING : 0; // days of thunder
	NESMode |= (ROMCRC == 0xeb506bf9) ? ALT_TIMING : 0; // star wars
	NESMode |= (ROMCRC == 0xa2d504a8) ? ALT_TIMING : 0; // assimilate
	NESMode |= (ROMCRC == 0x4df9d7c8) ? ALT_TIMING : 0; // overlord
	NESMode |= (ROMCRC == 0x85784e11) ? ALT_TIMING : 0; // blargg full palette
	NESMode |= (ROMCRC == 0x5da28b4f) ? ALT_TIMING : 0; // cmc! wall demo
	NESMode |= (ROMCRC == 0xab862073) ? ALT_TIMING : 0; // ultimate frogger champ.
	NESMode |= (ROMCRC == 0x2a798367) ? ALT_TIMING : 0; // jy 45-in-1
	NESMode |= (ROMCRC == 0xb9fd4de8) ? ALT_TIMING : 0; // over obj (glitch as boss materializes w/o this)
	NESMode |= (ROMCRC == 0x516843ef) ? (SHOW_OVERSCAN) : 0; // Cobol (HB)
	NESMode |= (ROMCRC == 0xb4255e99) ? (IS_PAL | SHOW_OVERSCAN) : 0; // Moonglow (HB)
	NESMode |= (ROMCRC == 0x78716f4f) ? RAM_RANDOM : 0; // Go! Dizzy Go!
	NESMode |= (ROMCRC == 0x8c4f37e2) ? RAM_RANDOM : 0; // Minna no Taabou no Nakayoshi Daisakusen (Japan)
	NESMode |= (ROMCRC == 0x17336a80) ? RAM_RANDOM : 0; // Minna no Taabou no Nakayoshi Daisakusen (T-Eng)
	NESMode |= (ROMCRC == 0xc0b4bce5) ? RAM_RANDOM : 0; // Terminator 2 (T2) - Judgement Day
	NESMode |= (ROMCRC == 0xdfdec378) ? RAM_RANDOM : 0; // Huang Di (Player jumps through the top of screen without this)
	NESMode |= (ROMCRC == 0x4d58c832) ? IS_PAL : 0; // Hammerin' Harry
	NESMode |= (ROMCRC == 0x149e367f) ? IS_PAL : 0; // Lion King, The
	NESMode |= (ROMCRC == 0xbf80b241) ? IS_PAL : 0; // Mr. Gimmick
	NESMode |= (ROMCRC == 0x4cf12d39) ? IS_PAL : 0; // Elite
	NESMode |= (ROMCRC == 0x3a0b6dd2) ? IS_PAL : 0; // Hero Quest
	NESMode |= (ROMCRC == 0x6d1e30a7) ? IS_PAL : 0; // TMHT
	NESMode |= (ROMCRC == 0xa5bbb96b) ? IS_PAL : 0; // TMHTII
	NESMode |= (ROMCRC == 0x6453f65e) ? IS_PAL : 0; // Uforia
	NESMode |= (ROMCRC == 0x55cbc495) ? IS_PAL : 0; // Super Turrican
	NESMode |= (ROMCRC == 0x11a245a0) ? IS_PAL : 0; // Rod Land
	NESMode |= (ROMCRC == 0xa535e1be) ? IS_PAL : 0; // Rackets and Rivals
	NESMode |= (ROMCRC == 0x8236d3e0) ? IS_PAL : 0; // Probotector
	NESMode |= (ROMCRC == 0x1d2fb2b7) ? IS_PAL : 0; // Shadow Warriors
	NESMode |= (ROMCRC == 0x96b6a919) ? IS_PAL : 0; // Shadow Warriors II
	NESMode |= (ROMCRC == 0x22ad4753) ? IS_PAL : 0; // Parodius
	NESMode |= (ROMCRC == 0xdcd55bec) ? IS_PAL : 0; // Aladdin
	NESMode |= (ROMCRC == 0xe08a5881) ? IS_PAL : 0; // Beauty and the Beast
	NESMode |= (ROMCRC == 0xcbde707e) ? IS_PAL : 0; // International Cricket
	NESMode |= (ROMCRC == 0xe2685bbf) ? IS_PAL : 0; // Kick Off
	NESMode |= (ROMCRC == 0xab21ab5f) ? IS_PAL : 0; // Noah's Ark
	NESMode |= (ROMCRC == 0xab29ab28) ? IS_PAL : 0; // Dropzone
	NESMode |= (ROMCRC == 0xde7e6767) ? IS_PAL : 0; // Asterix
	NESMode |= (ROMCRC == 0x9b4877e5) ? IS_PAL : 0; // Asterixec
	NESMode |= (ROMCRC == 0xdc7a16e6) ? IS_PAL : 0; // Parasol Stars
	NESMode |= (ROMCRC == 0xfac97247) ? IS_PAL : 0; // Rainbow Islands (Ocean)
	NESMode |= (ROMCRC == 0x391be891) ? IS_PAL : 0; // Sensible Soccer
	NESMode |= (ROMCRC == 0x732b1a7a) ? IS_PAL : 0; // Smurfs, The

	if (NESMode) {
		bprintf(0, _T("Game-specific configuration:\n"));

		for (UINT32 i = 1; i != 0x0000; i <<= 1) {
			switch (NESMode & i) {
				case NO_WORKRAM:
					bprintf(0, _T("*  Disabling cart. work-ram (6000-7fff)\n"));
					break;

				case BUS_CONFLICTS:
					bprintf(0, _T("*  Enabling BUS-CONFLICTS.\n"));
					break;

				case ALT_TIMING:
					bprintf(0, _T("*  Enabling ALT-TIMING.\n"));
					break;

				case ALT_TIMING2:
					bprintf(0, _T("*  Enabling ALT-TIMING2.\n"));
					break;

				case APU_HACKERY:
					bprintf(0, _T("*  Enabling APU_HACKERY.\n"));
					break;

				case IS_PAL:
					bprintf(0, _T("*  Enabling PAL weirdness.\n"));
					break;

				case RAM_RANDOM:
					bprintf(0, _T("*  Init RAM w/random bytes.\n"));
					break;

				case FLASH_EEPROM:
					bprintf(0, _T("*  Flash EEPROM loads/saves.\n"));
					break;
			}
		}
	}

	return 0;
}

static INT32 filediff(TCHAR *file1, TCHAR *file2)
{
	FILE *fp1, *fp2;
	INT32 len1, len2;
	UINT8 c1, c2;

	fp1 = _tfopen(file1, _T("rb"));
	if (!fp1) return -1;
	fp2 = _tfopen(file2, _T("rb"));
	if (!fp2) {
		fclose(fp1);
		return -2;
	}

	fseek(fp1, 0, SEEK_END);
	len1 = ftell(fp1);
	fseek(fp1, 0, SEEK_SET);

	fseek(fp2, 0, SEEK_END);
	len2 = ftell(fp2);
	fseek(fp2, 0, SEEK_SET);

	if (!len1 || !len2 || len1 != len2) {
		fclose(fp1);
		fclose(fp2);
		bprintf(0, _T("filediff(): length mismatch\n"));
		return -3;
	}

	for (INT32 i = 0; i < len1; i++) {
		fread(&c1, 1, 1, fp1);
		fread(&c2, 1, 1, fp2);
		if (c1 != c2) {
			fclose(fp1);
			fclose(fp2);
			bprintf(0, _T("filediff(): difference at offset $%x\n"));
			return -3;
		}
	}

	fclose(fp1);
	fclose(fp2);

	return 0; // file1 == file2
}

static INT32 ips_make(UINT8 *orig_data, UINT8 *new_data, INT32 size_data, TCHAR *ips_dir, TCHAR *ips_fn)
{
	#ifndef BUILD_WIN32
	#define _wunlink unlink
	#define _wrename rename
	#endif
	char iPATCH[6] = "PATCH";
	char iEOF[4] = "EOF";
	TCHAR ips_path_fn[MAX_PATH];
	TCHAR ips_path_fn_temp[MAX_PATH];
	INT32 ips_address;
	UINT8 ips_address_c[3];

	INT32 ips_length;
	UINT8 ips_length_c[2];

	INT32 rle_good;
	INT32 rle_length;
	UINT8 rle_byte;

	INT32 is_difference = 0;
	for (INT32 i = 0; i < size_data; i++)
	{
		if (orig_data[i] != new_data[i]) {
			is_difference = 1;
			break;
		}
	}

	if (is_difference == 0) return -2; // no change!

	_stprintf(ips_path_fn, _T("%s%s"), ips_dir, ips_fn);
	_stprintf(ips_path_fn_temp, _T("%s%s.temp"), ips_dir, ips_fn);

	bprintf(0, _T("ips_make() writing to [%s]\n"), ips_path_fn_temp);
	FILE *f = _tfopen(ips_path_fn_temp, _T("wb"));
	if (!f) return -1; // uhoh! (can't open file for writing)

	fwrite(&iPATCH, 1, 5, f);
	for (INT32 i = 0; i < size_data; i++)
	{
		if (orig_data[i] == new_data[i]) continue;

		ips_address = i;

		rle_good = 0;
		rle_length = 0;
		rle_byte = new_data[i];
		ips_length = 0;

		if (ips_address == 0x454f46) { // 'EOF'
			ips_length = 1; // Patch @ BAD ADDRESS (0x454f46), let's go back 1 byte and start here.
			ips_address--;
			if (rle_byte == new_data[ips_address]) {
				//printf("BAD ADDRESS-1 + RLE possible.\n");
				rle_length++;
			}
		}

		while ( (i < size_data) && (ips_length < 0xfce2)
			   && ((orig_data[i] != new_data[i])
				   || (rle_good && rle_byte == new_data[i])) ) // rle-opti: so we don't create multiple rle-chunks if a couple bytes are unchanged
		{
			if (rle_byte == new_data[i]) {
				if (rle_length == ips_length && rle_length > 3) {
					rle_good = 1;
				}
				rle_length++;
				if (rle_length > 5 && rle_good == 0) {
					// dump patch from before rle so the next contiguous block
					// is the rle block.
					if (i - rle_length == 0x454f46-1) {
						// this is not a good spot to rewind to.. abort rle for now.
						rle_length = 0;
					} else {
						i -= rle_length;
						ips_length -= rle_length;
						i--;
						break;
					}
				}
			} else if (rle_good) {
				i--;
				break;
			} else {
				rle_length = 0;
				rle_byte = new_data[i];
			}

			ips_length++;
			i++;
		}
		//printf("%spatchy @ %x   len %x  rle_len %x\n", ((rle_good)?"RLE-":""),ips_address, ips_length, rle_length);
		ips_address_c[0] = (ips_address >> 16) & 0xff;
		ips_address_c[1] = (ips_address >>  8) & 0xff;
		ips_address_c[2] = (ips_address >>  0) & 0xff;

		if (rle_good) ips_length = 0;

		ips_length_c[0] = (ips_length >> 8) & 0xff;
		ips_length_c[1] = (ips_length >> 0) & 0xff;

		fwrite(&ips_address_c, 1, 3, f);
		fwrite(&ips_length_c, 1, 2, f);
		if (rle_good) {
			ips_length_c[0] = (rle_length >> 8) & 0xff;
			ips_length_c[1] = (rle_length >> 0) & 0xff;
			fwrite(&ips_length_c, 1, 2, f);
			fwrite(&rle_byte, 1, 1, f);
		} else {
			fwrite(&new_data[ips_address], 1, ips_length, f);
		}
	}

	fwrite(&iEOF, 1, 3, f);

	fclose(f);

	// Check if the newly written patch is the same as previously written..
	if (filediff(ips_path_fn_temp, ips_path_fn)) {
		// Patch differs!
		// re-order backups
		const INT32 MAX_BACKUPS = 10;
		for (INT32 i = MAX_BACKUPS; i >= 0; i--) {
			TCHAR szBackupNameTo[MAX_PATH];
			TCHAR szBackupNameFrom[MAX_PATH];

			_stprintf(szBackupNameTo, _T("%s%s.backup%d"), ips_dir, ips_fn, i + 1);
			_stprintf(szBackupNameFrom, _T("%s%s.backup%d"), ips_dir, ips_fn, i);
			if (i == MAX_BACKUPS) {
				_wunlink(szBackupNameFrom); // make sure there is only MAX_BACKUPS :)
			} else {
				_wrename(szBackupNameFrom, szBackupNameTo); //derp.ips.backup0 -> derp.ips.backup1
				if (i == 0) {
					_wrename(ips_path_fn, szBackupNameFrom); //derp.ips -> derp.ips.backup0
				}
			}
		}
		// Rename temp patch filename to real patch filename
		int rc = _wrename(ips_path_fn_temp, ips_path_fn);
		bprintf(0, _T("wrename rc = %d   errno = %d\n"), rc, errno);
		bprintf(0, _T("-- lastly temp to non-temp -- rename %s %s\n"), ips_path_fn_temp, ips_path_fn);
		bprintf(0, _T("--- Fresh new IPS patch written: %s\n"), ips_path_fn);
	} else {
		bprintf(0, _T("--- IPS Patch from this session same as last - aborting write!\n"));
		return -2; // no change!
	}

	return 0;
}

static INT32 ips_patch(UINT8 *data, INT32 size_data, TCHAR *ips_fn)
{
	UINT8 buf[7] = "\0\0\0\0\0\0";

	UINT32 ips_address;
	UINT32 ips_length;

	INT32 rle_repeat;
	UINT8 rle_byte;

	FILE *f = _tfopen(ips_fn, _T("rb"));
	if (!f) return -1;

	fread(&buf, 1, 5, f);
	if (memcmp(&buf, "PATCH", 5) != 0) {
		fclose(f);
		return -2;
	}

	while (!feof(f))
	{
		memset(&buf, 0, 3);
		fread(buf, 1, 3, f);
		if (memcmp(&buf, "EOF", 3) == 0) {
			break;
		}
		ips_address = ((buf[0] << 16) & 0xff0000) | ((buf[1] << 8) & 0xff00) | (buf[2] & 0xff);

		memset(&buf, 0, 3);
		fread(buf, 1, 2, f);
		ips_length = ((buf[0] << 8) & 0xff00) | (buf[1] & 0xff);

		if (ips_length == 0) { // RLE chunk
			fread(buf, 1, 3, f);
			rle_repeat = ((buf[0] << 8) & 0xff00) | (buf[1] & 0xff);
			rle_byte = buf[2];

			if (ips_address + rle_repeat <= size_data) { // ok to patch
				memset(&data[ips_address], rle_byte, rle_repeat);
			} else {
				//printf("patch grow disabled, aborting.\n");
				break;
			}
		} else { // normal chunk
			if (ips_address + ips_length <= size_data) { // ok to patch
				fread(&data[ips_address], 1, ips_length, f);
			} else {
				//printf("patch grow disabled, aborting.\n");
				break;
			}
		}
	}

	fclose(f);

	return 0;
}

static void SaveIPSPatch(TCHAR *desc, UINT8 *rom_orig, UINT8 *rom_, INT32 rom_size)
{
	TCHAR patch_fn[MAX_PATH];
	_stprintf(patch_fn, _T("%s.ips"), BurnDrvGetText(DRV_NAME));
	INT32 ips = ips_make(rom_orig, rom_, rom_size, szAppEEPROMPath, patch_fn);
	bprintf(0, _T("* %s patch: "), desc);
	switch (ips) {
		case  0: bprintf(0, _T("Saved.\n")); break;
		case -1: bprintf(0, _T("Can't Save (File I/O Error).\n")); break;
		case -2: bprintf(0, _T("No Change.\n")); break;
	}
}

static void LoadIPSPatch(TCHAR *desc, UINT8 *rom_, INT32 rom_size)
{
	TCHAR szFilename[MAX_PATH];
	_stprintf(szFilename, _T("%s%s.ips"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
	INT32 ips = ips_patch(rom_, rom_size, szFilename);
	bprintf(0, _T("* %s ips patch: %s\n"), desc, (ips == 0) ? _T("Loaded") : _T("Can't Load/Not Found."));
}

static INT32 fds_load(UINT8* ROMData, UINT32 ROMSize, UINT32 ROMCRC)
{
	bprintf(0, _T("FDS Loader\n"));
	if (NULL == ROMData || ROMSize < 8192 ) {
		bprintf(0, _T("Invalid ROM data!\n"));
		return 1;
	}

	// FDS mem map:
	// 6000 - dfff RAM
	// e000 - ffff ROM (FDS Bios)

	memset(&Cart, 0, sizeof(Cart));

	PPUType = RP2C02;

	Cart.FDSMode = 1;
	Cart.FDSDiskRaw = (UINT8*)BurnMalloc(0x100000);
	Cart.FDSDiskRawOrig = (UINT8*)BurnMalloc(0x100000);
	Cart.FDSDisk = Cart.FDSDiskRaw;
	Cart.FDSDiskRawSize = ROMSize;
	Cart.FDSDiskRawOrigSize = ROMSize;
	Cart.FDSDiskSize = ROMSize;
	Cart.FDSDiskSides = ROMSize / 65500;
	if (BurnLoadRom(Cart.FDSDiskRaw, 0, 1)) return 1; // load FDS Disk Image
	if (BurnLoadRom(Cart.FDSDiskRawOrig, 0, 1)) return 1; // load FDS Disk Image

	// Load IPS patch (aka: disk-saves @ exit)
	LoadIPSPatch(_T("FDS Disk"), Cart.FDSDiskRaw, Cart.FDSDiskRawSize);

	if (!memcmp("FDS\x1a", &Cart.FDSDiskRaw[0], 4) && ROMSize > 0x10) {
		Cart.FDSDisk += 0x10;
		Cart.FDSDiskSize -= 0x10;
		bprintf(0, _T("* Skipping useless fds header..\n"));
	}

	Cart.Crc = ROMCRC;
	Cart.PRGRomSize = 0x8000;
	Cart.CHRRomSize = 0; // ram only
	Cart.Mirroring = 1; // for bios, but fds-mapper-controlled
	Cart.Mapper = 0x808; // fake mapper# for fds

	bprintf(0, _T("Disk Size: %d (%d sides)\n"), ROMSize, Cart.FDSDiskSides);
	bprintf(0, _T("PRG Size: %d\n"), Cart.PRGRomSize);
	bprintf(0, _T("CHR Size: %d\n"), Cart.CHRRomSize);

	switch (Cart.Mirroring) {
		case 0: set_mirroring(HORIZONTAL); break;
		case 1: set_mirroring(VERTICAL); break;
		case 4: set_mirroring(FOUR_SCREEN); break;
	}

//	Cart.BatteryBackedSRAM = (ROMData[6] & 0x2) >> 1;
	Cart.PRGRom = ROMData;

	// Default CHR-Ram size (8k), always set-up (for advanced mappers, etc)
	Cart.CHRRamSize = 0x2000;
	Cart.CHRRam = (UINT8*)BurnMalloc(Cart.CHRRamSize);
	mapper_set_chrtype(MEM_RAM);
	bprintf(0, _T("Using RAM CHR data (%dk bytes)\n"), Cart.CHRRamSize / 1024);

	// set-up Cart PRG-RAM/WORK-RAM (6000-7fff)
	Cart.WorkRAMSize = 0x2000;
	bprintf(0, _T("Cartridge RAM: %d\n"), Cart.WorkRAMSize);
	Cart.WorkRAM = (UINT8*)BurnMalloc(Cart.WorkRAMSize);

	setup_maps();

	// set-up MAPPER
	bprintf(0, _T("Cartridge Mapper: %d   Mirroring: "), Cart.Mapper);
	switch (Cart.Mirroring) {
		case 0: bprintf(0, _T("Horizontal")); break;
		case 1: bprintf(0, _T("Vertical")); break;
		case 4: bprintf(0, _T("Four-Screen")); break;
		default: bprintf(0, _T("???")); break;
	}
	bprintf(0, _T(" (%d)\n"), Cart.Mirroring);

	if (mapper_init(Cart.Mapper) == -1) {
		bprintf(0, _T("Unsupported mapper\n"));
		return 1;
	}

	if (Cart.BatteryBackedSRAM) {
		bprintf(0, _T("Game has battery-backed sRAM\n"));
	}

	Cart.PRGRomMask = Cart.PRGRomSize - 1;
	Cart.WorkRAMMask = Cart.WorkRAMSize - 1;

	// Game-specific stuff:
	NESMode = IS_FDS;

	if (NESMode) {
		bprintf(0, _T("Game-specific configuration:\n"));

		switch (NESMode) {
			case NO_WORKRAM:
				bprintf(0, _T("*  Disabling cart. work-ram (6000-7fff)\n"));
				break;

			case BUS_CONFLICTS:
				bprintf(0, _T("*  Enabling BUS-CONFLICTS.\n"));
				break;

			case ALT_TIMING:
				bprintf(0, _T("*  Enabling ALT-TIMING.\n"));
				break;

			case IS_PAL:
				bprintf(0, _T("*  Enabling PAL weirdness.\n"));
				break;
		}
	}

	return 0;
}

// ---- mapper system begins! ----
static UINT32 PRGMap[4];
static UINT8  PRGType[4];
static UINT32 CHRMap[8];
static UINT8  CHRType[8]; // enum { MEM_RAM = 0, MEM_RAM_RO = 1, MEM_ROM = 2};
static UINT8  mapper_regs[0x20]; // General-purpose mapper registers (8bit)
static UINT16 mapper_regs16[0x20]; // General-purpose mapper registers (16bit)
static UINT32 mapper_irq_exec; // cycle-delayed irq for mapper_irq();
static void mapper_irq(INT32 cyc); // forward

// mapping expansion ram/rom (6000 - 7fff)  refer to mapper69 for hookup info
static INT32 PRGExpMap;
static void  (*cart_exp_write)(UINT16 address, UINT8 data) = NULL;
static UINT8 (*cart_exp_read)(UINT16 address) = NULL;
static INT32 cart_exp_write_abort; // abort fallthrough - see mapper68
// mapping 4020 - 5fff, use these callbacks. refer to Sachen 8259
static void  (*psg_area_write)(UINT16 address, UINT8 data) = NULL;
static UINT8 (*psg_area_read)(UINT16 address) = NULL;
static UINT8 read_nt_int(UINT16 address);

static void mapper_map_prg(INT32 pagesz, INT32 slot, INT32 bank, INT32 type = MEM_ROM) // 8000 - ffff
{
	INT32 ramromsize = (type == MEM_ROM) ? Cart.PRGRomSize : Cart.WorkRAMSize;

	if (!ramromsize) return;

	if (bank < 0) { // negative bank == map page from end of rom
		bank = (ramromsize / (pagesz * 1024)) + bank;
	}

	for (INT32 i = 0; i < (pagesz / 8); i++) {
		PRGMap[(pagesz / 8) * slot + i] = (pagesz * 1024 * bank + 0x2000 * i) % ramromsize;
		PRGType[(pagesz / 8) * slot + i] = type;
	}
}

static void mapper_map_exp_prg(INT32 bank, INT32 type = MEM_ROM) // 6000 - 7fff area (cartridge expansion ram/sram or battery backed ram)
{
	INT32 ramromsize = (type == MEM_ROM) ? Cart.PRGRomSize : Cart.WorkRAMSize;

	if (bank < 0) { // negative bank == map page from end of rom
		bank = (ramromsize / 0x2000) + bank;
	}

	PRGExpMap = (0x2000 * bank) % ramromsize;
}

static void mapper_set_chrtype(UINT32 type)
{
	for (UINT32 i = 0; i < 8; i++)
		CHRType[i] = type;
}

static void mapper_map_chr(INT32 pagesz, INT32 slot, INT32 bank)
{
	for (UINT32 i = 0; i < pagesz; i++) {
		const int index = pagesz * slot + i;
		CHRMap[index] = (pagesz * 1024 * bank + 1024 * i) % *chr_size_maps[CHRType[index]];
#if 0
		switch (CHRType[pagesz * slot + i]) {
			case MEM_ROM:
				CHRMap[pagesz * slot + i] = (pagesz * 1024 * bank + 1024 * i) % Cart.CHRRomSize;
				break;

			case MEM_RAM:
			case MEM_RAM_RO:
				CHRMap[pagesz * slot + i] = (pagesz * 1024 * bank + 1024 * i) % Cart.CHRRamSize;
				break;
		}
#endif
	}
}

#define MAP_CHR_RAMROM_DEBUG 0

#if MAP_CHR_RAMROM_DEBUG
static INT32 debug_chr_slots[8] = { -1, };
static INT32 debug_chr_types[8] = { -1, };
#endif

static void mapper_map_chr_ramrom(INT32 pagesz, INT32 slot, INT32 bank, INT32 type)
{
	if (type > MEM_ROM) {
		bprintf(0, _T("mapper_map_chr_ramrom(): invalid type field!!\n"));
	}

#if MAP_CHR_RAMROM_DEBUG
	INT32 debug_spew = (debug_chr_slots[slot & 0x7] != bank) ||
		(debug_chr_types[slot & 0x7] != type);
	if (debug_spew) {
		debug_chr_slots[slot & 0x07] = bank;
		debug_chr_types[slot & 0x07] = type;
		bprintf(0, _T("mapper_map_chr_ramrom(%x, %x, %x, %S) - scanline %d\n"), pagesz, slot, bank, (type == MEM_ROM) ? "ROM" : "RAM", scanline);
	}
#endif

	for (UINT32 i = 0; i < pagesz; i++) {
		switch (type) {
			case MEM_ROM:
				CHRMap[pagesz * slot + i] = (pagesz * 1024 * bank + 1024 * i) % Cart.CHRRomSize;
				CHRType[pagesz * slot + i] = MEM_ROM;
#if MAP_CHR_RAMROM_DEBUG
				if (debug_spew)
					bprintf(0, _T("ROM: CHRMap[%x] = %x\n"),pagesz * slot + i, (pagesz * 1024 * bank + 1024 * i) % Cart.CHRRomSize);
#endif
				break;

			case MEM_RAM:
			case MEM_RAM_RO:
				CHRMap[pagesz * slot + i] = (pagesz * 1024 * bank + 1024 * i) % Cart.CHRRamSize;
				CHRType[pagesz * slot + i] = type;
#if MAP_CHR_RAMROM_DEBUG
				if (debug_spew)
					bprintf(0, _T("RAM: CHRMap[%x] = %x\n"),pagesz * slot + i, (pagesz * 1024 * bank + 1024 * i) % Cart.CHRRamSize);
#endif
				break;
		}
	}
}

// FIND_CHEAT_ROMOFFSET helps to find the rom offset of gamegenie cheats
// when an address hits :)
#define FIND_CHEAT_ROMOFFSET 0

#if FIND_CHEAT_ROMOFFSET
static int l_address, l_offset;
#endif

static UINT8 mapper_prg_read_int(UINT16 address) // mapper_prg_read points here
{
#if 0
	// tests: save incase there is a problem that needs to be debugged
	if (PRGType[(address & ~0x8000) / 0x2000] > 2) {
		bprintf(0, _T("mapper_prg_read_int(): address %x, bad PRGType[%x]!\n"), address, PRGType[(address & ~0x8000) / 0x2000]);
	}
#endif

#if FIND_CHEAT_ROMOFFSET
	l_address = address;
	l_offset = PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1fff);
#endif

	return prg_maps[PRGType[(address & ~0x8000) / 0x2000]] [PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1fff)];
#if 0
	// the old version, more "readable", but slower.  save to keep sane.
	switch (PRGType[(address & ~0x8000) / 0x2000]) {
		case MEM_ROM:
			return Cart.PRGRom[PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1fff)];
		case MEM_RAM:
			return Cart.WorkRAM[PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1fff)];
	}

	bprintf(0, _T("PRGType[] corruption?\n"));
	return 0x00;
#endif
}

static void mapper_prg_write(UINT16 address, UINT8 data)
{
	switch (PRGType[(address & ~0x8000) / 0x2000]) {
		case MEM_RAM:
			Cart.WorkRAM[PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1fff)] = data;
	}
}

static UINT8 mapper_chr_read(UINT32 address)
{
#if 0
	// tests: save incase there is a problem that needs to be debugged
	if (CHRType[address / 1024] > 2) {
		bprintf(0, _T("mapper_chr_read(): address %x, bad CHRType[%x]!\n"), address, CHRType[address / 1024]);
	}
	if (CHRType[address / 1024] == MEM_ROM && Cart.CHRRom == NULL) {
		bprintf(0, _T("mapper_chr_read(): address %x, marked ROM but no chr rom!\n"), address);
	}
	if (address > 0x1fff)  bprintf(0, _T("mapper_chr_read(): %x address overflow.\n"), address);
#endif

	return chr_maps[CHRType[address / 1024]] [CHRMap[address / 1024] + (address & (1024 - 1))];

#if 0
	// the old version, more "readable", but slower.  save to keep sane.
	switch (CHRType[address / 1024]) {
		case MEM_ROM:
			return Cart.CHRRom[CHRMap[address / 1024] + (address & (1024 - 1))];

		case MEM_RAM:
		case MEM_RAM_RO:
			return Cart.CHRRam[CHRMap[address / 1024] + (address & (1024 - 1))];
	}

	return 0x00;
#endif
}

static void mapper_chr_write(UINT16 address, UINT8 data)
{
	if (CHRType[address / 1024] == MEM_RAM) {
		Cart.CHRRam[CHRMap[address / 1024] + (address & (1024 - 1))] = data;
	}
}

// Flippy Disk ICON (thanks iq_132! :)
static UINT8 disk_ab[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x0f, 0x07, 0x07, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0f, 0x01, 0x0e, 0x01, 0x03, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x0f, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0c, 0x0d, 0x00, 0x0d, 0x0e, 0x0a, 0x00, 0x02, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x0f, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0d, 0x00, 0x00, 0x0c, 0x09, 0x0f, 0x00, 0x00, 0x06, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x08, 0x00, 0x00, 0x0d, 0x09, 0x09, 0x09, 0x0a, 0x00, 0x00, 0x03, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x02, 0x00, 0x00, 0x06, 0x09, 0x09, 0x09, 0x0d, 0x00, 0x00, 0x06, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x00, 0x09, 0x09, 0x00, 0x07, 0x03, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x00, 0x00, 0x00, 0x0e, 0x09, 0x09, 0x09, 0x02, 0x00, 0x00, 0x0a, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x0f, 0x07, 0x00, 0x09, 0x09, 0x00, 0x07, 0x0f, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x03, 0x00, 0x00, 0x00, 0x01, 0x09, 0x09, 0x09, 0x02, 0x00, 0x00, 0x00, 0x0c, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x00, 0x00, 0x07, 0x03, 0x07, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x01, 0x00, 0x00, 0x00, 0x0d, 0x0e, 0x01, 0x0e, 0x05, 0x00, 0x00, 0x00, 0x08, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x01, 0x09, 0x09, 0x09, 0x0d, 0x00, 0x00, 0x00, 0x0b, 0x09,
	0x00, 0x03, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x03, 0x00, 0x09, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x02, 0x00, 0x00, 0x00, 0x01, 0x09, 0x09, 0x09, 0x02, 0x00, 0x00, 0x00, 0x01, 0x09,
	0x00, 0x03, 0x07, 0x07, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x07, 0x07, 0x03, 0x00, 0x09, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x01, 0x09, 0x09, 0x09, 0x0d, 0x00, 0x00, 0x00, 0x01, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x02, 0x00, 0x00, 0x00, 0x01, 0x09, 0x09, 0x09, 0x02, 0x00, 0x00, 0x00, 0x01, 0x09,
	0x00, 0x07, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x0f, 0x07, 0x07, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0d, 0x0e, 0x0c, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x0f, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x0b, 0x03, 0x06, 0x00, 0x00, 0x00, 0x01, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x0f, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x03, 0x09, 0x03, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x03, 0x09, 0x03, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x00, 0x00, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x03, 0x09, 0x0b, 0x00, 0x00, 0x00, 0x0a, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x03, 0x07, 0x00, 0x09, 0x09, 0x00, 0x07, 0x03, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x0a, 0x0d, 0x0a, 0x00, 0x0a, 0x02, 0x03, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x0f, 0x07, 0x00, 0x09, 0x09, 0x00, 0x07, 0x0f, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x02, 0x0c, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x00, 0x00, 0x07, 0x03, 0x07, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x0f, 0x09, 0x0b, 0x00, 0x00, 0x00, 0x05, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x07, 0x07, 0x03, 0x07, 0x07, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x0c, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00, 0x03, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03, 0x07, 0x07, 0x07, 0x07, 0x07, 0x00, 0x09, 0x04, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x0c, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00, 0x03, 0x09, 0x09,
	0x00, 0x03, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x03, 0x00, 0x09, 0x04, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x0c, 0x09, 0x09, 0x00, 0x00, 0x00, 0x0a, 0x09, 0x09, 0x09,
	0x00, 0x03, 0x07, 0x07, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x07, 0x07, 0x03, 0x00, 0x09, 0x04, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x01, 0x03, 0x06, 0x00, 0x00, 0x0a, 0x0f, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x02, 0x01, 0x0c, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x07, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x07, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09
};

static UINT8 disk_ab_pal[] = {
	0x01, 0x01, 0x01,
	0x87, 0x87, 0x87,
	0x4c, 0x4c, 0x4c,
	0xc4, 0xc4, 0xc4,
	0x25, 0x1d, 0xed,
	0x27, 0x27, 0x27,
	0x66, 0x66, 0x66,
	0xb1, 0xe5, 0xef,
	0xa6, 0xa6, 0xa6,
	0xff, 0xff, 0xff,
	0x19, 0x19, 0x19,
	0x98, 0x98, 0x98,
	0xe1, 0xe1, 0xe1,
	0x3b, 0x3b, 0x3b,
	0x75, 0x75, 0x75,
	0xba, 0xba, 0xba
};

// ---[ mapper FDS (Famicom Disk System) #0x808 (2056)
#define mapperFDS_ioenable		(mapper_regs[0x1f - 0]) // 4023
#define mapperFDS_control		(mapper_regs[0x1f - 1]) // 4025

#define mapperFDS_eighth_bit 	(mapper_regs[0x1f - 2])
#define mapperFDS_in_irq		(mapper_regs[0x1f - 3])

#define mapperFDS_irqenable		(mapper_regs[0x1f - 4])
#define mapperFDS_irqrepeat		(mapper_regs[0x1f - 5])
#define mapperFDS_irqcount		(mapper_regs16[0x1f - 0])
#define mapperFDS_irqlatch		(mapper_regs16[0x1f - 1])

#define mapperFDS_diskside		(mapper_regs[0x1f - 6])
#define mapperFDS_diskinsert	(mapper_regs[0x1f - 7])
#define mapperFDS_diskaccessed	(mapper_regs[0x1f - 8])
#define mapperFDS_diskaddr		(mapper_regs16[0x1f - 2])
#define mapperFDS_diskirqcount	(mapper_regs16[0x1f - 3])
#define mapperFDS_arm_timer() 	do { mapperFDS_diskirqcount = 150; } while (0)
#define mapperFDS_filesize      (mapper_regs16[0x1f - 4])
#define mapperFDS_blockid       (mapper_regs16[0x1f - 5])
#define mapperFDS_blockstart    (mapper_regs16[0x1f - 6])
#define mapperFDS_blocklen      (mapper_regs16[0x1f - 7])
#define fds_disk() 				(Cart.FDSDisk[(mapperFDS_diskside * 65500) + mapperFDS_blockstart + mapperFDS_diskaddr])

static char fdsfile[10]; // for debugging.. remove later!

enum FDS_FrameActions { FA_IDLE = 0, INSERT_DISK = 1, SWAP_DISK = 2, FA_FADEOUT};
enum FDS_DiskBlockIDs { DSK_INIT = 0, DSK_VOLUME, DSK_FILECNT, DSK_FILEHDR, DSK_FILEDATA };

static void FDS_FrameAction(INT32 todo);

static void FDS_Insert(INT32 status)
{
	static INT32 debounce = 0;
	if (status && debounce == 0) {
		mapperFDS_diskinsert ^= 1;
		bprintf(0, _T("FDS_Insert: %d\n"), mapperFDS_diskinsert);
	}
	debounce = status;
}

static void FDS_SwapSides(INT32 status, INT32 no_debounce = 0)
{
	static INT32 debounce = 0;
	if (status && (debounce == 0 || no_debounce) && mapperFDS_diskinsert == 0) {
		mapperFDS_diskside = (mapperFDS_diskside + 1) % Cart.FDSDiskSides;
		bprintf(0, _T("FDS_SwapSides: %d\n"), mapperFDS_diskside);
	}
	debounce = status;
}

static void FDS_SwapSidesAuto(INT32 status)
{
	static INT32 debounce = 0;
	if (status && debounce == 0) {
		FDS_FrameAction(SWAP_DISK);
	}
	debounce = status;
}

static void FDS_FrameAction(INT32 todo)
{
	FDSFrameCounter = 0;
	FDSFrameAction = todo;
}

static void FDS_FrameTicker()
{
	switch (FDSFrameAction) {
		case FA_IDLE:
			break;

		case INSERT_DISK:
			if (FDSFrameCounter == 5) {
				mapperFDS_diskinsert = 1;
				FDSFrameAction = FA_IDLE;
			}
			break;

		case SWAP_DISK:
			if (FDSFrameCounter == 0) {
				FDSFrameDiskIcon = ((mapperFDS_diskside + 1) % Cart.FDSDiskSides) & 1;
			}
			if (FDSFrameCounter == 5) {
				mapperFDS_diskinsert = 0;
			}
			if (FDSFrameCounter == 75) {
				bprintf(0, _T("(auto) "));
				FDS_SwapSides(1, 1);
			}
			if (FDSFrameCounter == 155) {
				mapperFDS_diskinsert = 1;
				FDSFrameAction = FA_FADEOUT;
			}
			break;
		case FA_FADEOUT:
			if (FDSFrameCounter == 155+20) {
				FDSFrameAction = FA_IDLE;
			}
			break;
	}
	FDSFrameCounter++;
}

struct fds_sound {
	UINT8 volmaster;
	UINT8 wavwren_hold;
	UINT8 volgain; // r:4090 w:4080
	UINT8 modgain; // r:4092 w:4084
	UINT8 waveram[0x40];
	UINT8 modwaveram[0x40];

	UINT8 env_master;

	UINT32 wavefreq;
	UINT32 wavepos;
	UINT32 modphase;
	UINT32 modfreq;
	INT32 mod_accu;

	UINT8 mod_counter;
	UINT8 env_all_stop;
	UINT8 wav_stop;
	UINT8 mod_cnt_stop;

	INT32 env_vol_accu;
	UINT8 env_vol_stop;
	UINT8 env_vol_direction;
	UINT8 env_vol_period;

	INT32 env_mod_accu;
	UINT8 env_mod_stop;
	UINT8 env_mod_direction;
	UINT8 env_mod_period;

	INT32 filt_prev;
	INT32 filt_sa;
	INT32 filt_sb;
};

static fds_sound fds;

static void mapperFDS_reset()
{
	BurnLEDReset();

	memset(&fds, 0, sizeof(fds));

	// Auto-"Insertion/Swap" robot
	if (~NESDips[0] & 2)
		FDS_FrameAction(INSERT_DISK);

	// Init 2khz lp filter coefficient @ 1.786mhz
	double omeg = exp(-2.0 * 3.1415926 * 2000 / (29781 * 60));
    fds.filt_sa = (INT32)(omeg * (double)(1 << 12));
    fds.filt_sb = (1 << 12) - fds.filt_sa;
}

static void mapperFDS_scan()
{
	SCAN_VAR(fds);
	SCAN_VAR(FDSFrameCounter);
	SCAN_VAR(FDSFrameAction);

	// 6000-7fff WRAM(cart_ext) is scanned by system
	ScanVar(Cart.PRGRom, 0x6000, "PRGRAM 8000-dfff");
}

static UINT8 fds_sound_read(UINT16 address)
{
	if (address >= 0x4040 && address <= 0x407f) {
		return fds.waveram[address & 0x3f] | (cpu_open_bus & 0xc0);
	}

	switch (address) {
		case 0x4090: return (fds.volgain & 0x3f) | (cpu_open_bus & 0xc0);
		case 0x4092: return (fds.modgain & 0x3f) | (cpu_open_bus & 0xc0);
	}

	return cpu_open_bus;
}

static void fds_sound_write(UINT16 address, UINT8 data)
{
	if (address >= 0x4040 && address <= 0x407f) {
		if (fds.wavwren_hold) {
			fds.waveram[address & 0x3f] = data & 0x3f;
		}
		return;
	}

	switch (address) {
		case 0x4080:
			fds.env_vol_stop = (data & 0x80) >> 7;
			fds.env_vol_direction = (data & 0x40) >> 6;
			fds.env_vol_period = data & 0x3f;
			if (data & 0x80) {
				fds.volgain = data & 0x3f;
			}
			break;
		case 0x4082:
			fds.wavefreq = (fds.wavefreq & 0x0f00) | data;
			break;
		case 0x4083:
			fds.wavefreq = (fds.wavefreq & 0x00ff) | ((data & 0xf) << 8);
			fds.env_all_stop = (data & 0x40) >> 6;
			fds.wav_stop = (data & 0x80) >> 7;

			if (data & 0x80) {
				// phase reset
				fds.wavepos = 0;
			}
			break;
		case 0x4084:
			fds.env_mod_stop = (data & 0x80) >> 7;
			fds.env_mod_direction = (data & 0x40) >> 6;
			fds.env_mod_period = data & 0x3f;
			if (data & 0x80) {
				fds.modgain = data & 0x3f;
			}
			break;
		case 0x4085:
			fds.mod_counter = data & 0x7f;
			fds.modphase &= 0x3f0000;
			break;
		case 0x4086:
			fds.modfreq = (fds.modfreq & 0x0f00) | data;
			break;
		case 0x4087:
			fds.modfreq = (fds.modfreq & 0x00ff) | ((data & 0xf) << 8);
			fds.mod_cnt_stop = (data & 0x80) >> 7;
			if (data & 0x80) {
				fds.mod_accu = 0;
			}
			break;
		case 0x4088:
			if (fds.mod_cnt_stop) {
				fds.modwaveram[(fds.modphase >> 16) & 0x3f] = data & 7;
				fds.modphase = (fds.modphase + 0x10000) & 0x3fffff;
				fds.modwaveram[(fds.modphase >> 16) & 0x3f] = data & 7;
				fds.modphase = (fds.modphase + 0x10000) & 0x3fffff;
			}
			break;
		case 0x4089:
			fds.volmaster = data & 3;
			fds.wavwren_hold = (data & 0x80) >> 7;
			break;
		case 0x408a:
			fds.env_master = data;
			break;
	}
}

static void env_vol()
{
	if (fds.env_vol_stop == 0) {
		fds.env_vol_accu--;
		if (fds.env_vol_accu <= 0) {
			fds.env_vol_accu += ((fds.env_vol_period + 1) * fds.env_master) << 3;
			if (fds.env_vol_direction) {
				if (fds.volgain < 0x3f) {
					fds.volgain++;
				}
			} else {
				if (fds.volgain > 0) {
					fds.volgain--;
				}
			}
		}
	}
}

static void env_mod()
{
	if (fds.env_mod_stop == 0) {
		fds.env_mod_accu--;
		if (fds.env_mod_accu <= 0) {
			fds.env_mod_accu += ((fds.env_mod_period + 1) * fds.env_master) << 3;
			if (fds.env_mod_direction) {
				if (fds.modgain < 0x3f) {
					fds.modgain++;
				}
			} else {
				if (fds.modgain > 0) {
					fds.modgain--;
				}
			}
		}
	}
}

static void mod_sweep()
{
	INT32 mod = 0;
	if (fds.mod_cnt_stop == 0) {
		fds.mod_accu += fds.modfreq;
		if(fds.mod_accu >= 0x10000) {
			fds.mod_accu -= 0x10000;
			const INT32 BIASTBL[8] = { 0, 1, 2, 4, 0, -4, -2, -1 };
			UINT8 w = fds.modwaveram[(fds.modphase >> 16) & 0x3f];
			fds.mod_counter = (w == 4) ? 0 : (fds.mod_counter + BIASTBL[w]) & 0x7f;
		}
		fds.modphase = (fds.modphase + fds.modfreq) & 0x3fffff;
	}

	if (fds.wav_stop == 0) {
		if (fds.modgain != 0) {
			INT32 pos = (fds.mod_counter < 64) ? fds.mod_counter : (fds.mod_counter-128);

			// nesdev to the rescue! :) (yet again!)
			INT32 temp = pos * ((fds.modgain < 0x20) ? fds.modgain : 0x20);
			INT32 remainder = temp & 0x0f;
			temp >>= 4;
			if (remainder > 0 && ~temp & 0x80) {
				temp += (pos < 0) ? -1 : 2;
			}

			if (temp >= 192) temp -= 256;
			else if (temp < -64) temp += 256;

			temp = fds.wavefreq * temp;
			remainder = temp & 0x3f;
			temp >>= 6;
			if (remainder >= 32) temp += 1;
			mod = temp;
		}

		fds.wavepos = (fds.wavepos + fds.wavefreq + mod) & 0x3fffff;
	}
}

static INT16 mapperFDS_mixer()
{
	UINT32 sample = 0;

	if (fds.env_all_stop == 0 && fds.wav_stop == 0 && fds.env_master != 0) {
		env_vol();
		env_mod();
	}

	mod_sweep();

	if (fds.wavwren_hold == 0) {
		// render sample
		INT32 voltab[4] = { 0x173, 0xf7, 0xb9, 0x95 };
		UINT8 envvol = (fds.volgain > 0x20) ? 0x20 : fds.volgain;
		sample = (fds.waveram[(fds.wavepos >> 16) & 0x3f] * envvol) * voltab[fds.volmaster];
		sample >>= 7;

		// 2khz lp filter
		sample = ((fds.filt_prev * fds.filt_sa) + (sample * fds.filt_sb)) >> 12;
		fds.filt_prev = sample;
	} else {
		sample = fds.filt_prev;
	}

	return sample;
}

static void mapperFDS_cycle()
{
	if (mapperFDS_irqenable) {
		mapperFDS_irqcount--;
		// m6502 cpu core isn't great.. compensate by running irq for 5 cycles longer.
		// Testcases: Maerchen Veil HUD, KaetteKita Mario Bros (white bricks cutscene)
		if (mapperFDS_irqcount == 0xffff-5) {
			mapperFDS_irqcount = mapperFDS_irqlatch;
			if (mapperFDS_irqrepeat == 0) {
				mapperFDS_irqenable = 0;
			}
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			mapperFDS_in_irq = 1;
		}
	}

	if (mapperFDS_diskirqcount > 0) {
		mapperFDS_diskirqcount--;
		if (mapperFDS_diskirqcount == 0) {
			if (mapperFDS_control & 0x80) {
				M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
				mapperFDS_eighth_bit = 1;
			}
		}
	}
}

static void mapperFDS_map()
{
	set_mirroring((mapperFDS_control & 8) ? HORIZONTAL : VERTICAL);
}

static UINT8 mapperFDS_read(UINT16 address)
{
	UINT8 ret = 0;

	if (mapperFDS_ioenable & 2 && address >= 0x4040 && address <= 0x4097)
		return fds_sound_read(address);

	if (~mapperFDS_ioenable & 1) return cpu_open_bus; // io disabled.

	switch (address) {
		case 0x4030:
			if (mapperFDS_in_irq) ret |= 1;
			if (mapperFDS_eighth_bit) ret |= 2;

			mapperFDS_in_irq = mapperFDS_eighth_bit = 0;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;

		case 0x4031:
			ret = 0xff;
			if (mapperFDS_diskinsert && mapperFDS_control & 0x04) {
				mapperFDS_diskaccessed = 1;

				ret = 0;

				switch (mapperFDS_blockid) {
					case DSK_FILEHDR:
						if (mapperFDS_diskaddr < mapperFDS_blocklen) {
							ret = fds_disk();
							switch (mapperFDS_diskaddr) {
								case 13: mapperFDS_filesize = ret; break;
								case 14:
									mapperFDS_filesize |= ret << 8;
									strncpy(fdsfile, (char*)&Cart.FDSDisk[(mapperFDS_diskside * 65500) + mapperFDS_blockstart + 3], 8);
									bprintf(0, _T("Read file: %S (size: %d)\n"), fdsfile, mapperFDS_filesize);
									break;
							}
							mapperFDS_diskaddr++;
						}
						break;
					default:
						if (mapperFDS_diskaddr < mapperFDS_blocklen) {
							ret = fds_disk();
							mapperFDS_diskaddr++;
						}
						break;
				}

				mapperFDS_arm_timer();
				mapperFDS_eighth_bit = 0;
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
			break;

		case 0x4032:
			ret = cpu_open_bus & 0xf8;
			if (mapperFDS_diskinsert == 0) {
				ret |= 4 /*write protect or eject*/ | 1 /*disk not inserted*/;
			}
			if ((mapperFDS_diskinsert == 0) || (~mapperFDS_control & 1/*motor off*/) || (mapperFDS_control & 2/*transfer reset*/)) {
				ret |= 2 /*disk not ready*/;
			}
			break;

		case 0x4033: // battery status of drive unit
			ret |= 0x80;
			break;
	}

	return ret;
}

static void mapperFDS_prg_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8000 && address <= 0xdfff) {
		Cart.PRGRom[address & 0x7fff] = data;
	}
}

static void mapperFDS_write(UINT16 address, UINT8 data)
{
	if (mapperFDS_ioenable & 2 && address >= 0x4040 && address <= 0x4097)
		return fds_sound_write(address, data);

	if ((~mapperFDS_ioenable & 1) && address >= 0x4024) return; // io disabled.

	switch (address) {
		case 0x4020:
			mapperFDS_irqlatch = (mapperFDS_irqlatch & 0xff00) | data;
			break;
		case 0x4021:
			mapperFDS_irqlatch = (mapperFDS_irqlatch & 0x00ff) | (data << 8);
			break;

		case 0x4022:
			mapperFDS_irqenable = (data & 2) && (mapperFDS_ioenable & 1);
			mapperFDS_irqrepeat = data & 1;
			if (mapperFDS_irqenable) {
				mapperFDS_irqcount = mapperFDS_irqlatch;
			} else {
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				mapperFDS_in_irq = 0;
			}
			break;

		case 0x4023:
			mapperFDS_ioenable = data;
			if (~data & 1) {
				mapperFDS_irqenable = 0;
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				mapperFDS_in_irq = mapperFDS_eighth_bit = 0;
			}
			break;

		case 0x4024:
			if (mapperFDS_diskinsert && ~mapperFDS_control & 0x04) {

				if (mapperFDS_diskaccessed == 0) {
					mapperFDS_diskaccessed = 1;
					break;
				}

				switch (mapperFDS_blockid) {
					case DSK_FILEHDR:
						if (mapperFDS_diskaddr < mapperFDS_blocklen) {
							fds_disk() = data;
							switch (mapperFDS_diskaddr) {
								case 13: mapperFDS_filesize = data; break;
								case 14:
									mapperFDS_filesize |= data << 8;
									fdsfile[0] = 0;
									strncpy(fdsfile, (char*)&Cart.FDSDisk[(mapperFDS_diskside * 65500) + mapperFDS_blockstart + 3], 8);
									bprintf(0, _T("Write file: %S (size: %d)\n"), fdsfile, mapperFDS_filesize);
									break;
							}
							mapperFDS_diskaddr++;
						}
						break;
					default:
						if (mapperFDS_diskaddr < mapperFDS_blocklen) {
							fds_disk() = data;
							mapperFDS_diskaddr++;
						}
						break;
				}

			}
			break;

		case 0x4025:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);

			//bprintf(0, _T("0x4025: nCurrentFrame %d  param %x   ppc %x\n"), nCurrentFrame, data, M6502GetPrevPC(-1));
			if (mapperFDS_diskinsert) {
				if (data & 0x40 && ~mapperFDS_control & 0x40) {
					mapperFDS_diskaccessed = 0;

					mapperFDS_arm_timer();

					// blockstart  - address of block on disk
					// diskaddr    - address relative to blockstart
					mapperFDS_blockstart += mapperFDS_diskaddr;
					mapperFDS_diskaddr = 0;

					mapperFDS_blockid++;
					if (mapperFDS_blockid > DSK_FILEDATA)
						mapperFDS_blockid = DSK_FILEHDR;

					switch (mapperFDS_blockid) {
						case DSK_VOLUME:
							mapperFDS_blocklen = 0x38;
							break;
						case DSK_FILECNT:
							mapperFDS_blocklen = 0x02;
							break;
						case DSK_FILEHDR:
							mapperFDS_blocklen = 0x10;
							break;
						case DSK_FILEDATA:		 // [blockid]<filedata>
							mapperFDS_blocklen = 0x01 + mapperFDS_filesize;
							break;
					}
				}

				if (data & 0x02) { // transfer reset
					mapperFDS_blockid = DSK_INIT;
					mapperFDS_blockstart = 0;
					mapperFDS_blocklen = 0;
					mapperFDS_diskaddr = 0;
					mapperFDS_arm_timer();
				}
				if (data & 0x40 && ~data & 0x02) { // turn on motor
					mapperFDS_arm_timer();
					BurnLEDSetStatus(0, 1);
					BurnLEDSetColor((mapperFDS_control & 0x04) ? LED_COLOR_GREEN : LED_COLOR_RED);
				} else {
					BurnLEDSetStatus(0, 0);
				}
			}
			mapperFDS_control = data;

			mapper_map(); // set mirror

			break;
	}
}

#undef mapperFDS_ioenable
#undef mapperFDS_control
#undef mapperFDS_eighth_bit
#undef mapperFDS_in_irq
#undef mapperFDS_irqenable
#undef mapperFDS_irqrepeat
#undef mapperFDS_irqcount
#undef mapperFDS_irqlatch
#undef mapperFDS_diskside
#undef mapperFDS_diskinsert
#undef mapperFDS_diskaccessed
#undef mapperFDS_diskaddr
#undef mapperFDS_diskirqcount
#undef mapperFDS_arm_timer
#undef mapperFDS_filesize
#undef mapperFDS_blockid
#undef mapperFDS_blockstart

// ---[ mapper 01 (MMC1)
#define mapper01_serialbyte	(mapper_regs[0x1f - 0])
#define mapper01_bitcount	(mapper_regs[0x1f - 1])
#define mapper01_lastwrite  (mapper_regs[0x1f - 2])
#define mapper01_exbits		(mapper_regs[0x1f - 3])
#define mapper01_prg2x		(mapper_regs[0x1f - 4])
static INT32 *mapper01_last_cyc = (INT32*)&mapper_regs16[0];
static INT32 *mapper105_timer = (INT32*)&mapper_regs16[2];

static void mapper01_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		if (mega_cyc_counter - mapper01_last_cyc[0] < 2) {
			// https://wiki.nesdev.com/w/index.php/MMC1
			// Bill & Ted's Excellent Adventure resets the MMC1 by doing INC
			// on a ROM location containing $FF; the MMC1 sees the $FF
			// written back and ignores the $00 written on the next cycle.
			mapper01_last_cyc[0] = mega_cyc_counter;
			return;
		}
        if (data & 0x80) { // bit 7, mapper reset
			mapper01_bitcount = 0;
			mapper01_serialbyte = 0;
			mapper_regs[0] |= 0x0c;
			if (mapper_map) mapper_map();
		} else {
			mapper01_serialbyte |= (data & 1) << mapper01_bitcount;
			mapper01_bitcount++;
			if (mapper01_bitcount == 5) {
				UINT8 reg = (address >> 13) & 0x3;
				mapper_regs[reg] = mapper01_serialbyte;
				//bprintf(0, _T("mmc1_reg[%x]  =  %x\n"), reg, mapper01_serialbyte);
				switch (reg) {
					case 1: mapper01_lastwrite = 0; break;
					case 2: mapper01_lastwrite = 1; break;
				}
                mapper01_bitcount = 0;
                mapper01_serialbyte = 0;
                if (mapper_map) mapper_map();
			}
		}
		mapper01_last_cyc[0] = mega_cyc_counter;
    }
}

static void mapper105_map()
{
	if (mapper_regs[1] & 0x8) {
		// mmc1-mode, can only swap into the second 128k chip ( | 0x8)
		if (mapper_regs[0] & 0x8) {
			if (mapper_regs[0] & 0x4) {
				mapper_map_prg(16, 0, (mapper_regs[3] & 0x7) | 0x8);
				mapper_map_prg(16, 1, 0x7 | 0x8);
			} else {
				mapper_map_prg(16, 0, 0x0 | 0x8);
				mapper_map_prg(16, 1, (mapper_regs[3] & 0x7) | 0x8);
			}
		} else {
			mapper_map_prg(32, 0, ((mapper_regs[3] & 0x7) | 0x8) >> 1);
		}
	} else {
		// non-mmc1 prg mode, swaps in the first 128k
		mapper_map_prg(32, 0, ((mapper_regs[1] & 0x6)) >> 1);
	}

	if (mapper_regs[1] & 0x10) {
		mapper105_timer[0] = 0;
		M6502SetIRQLine(0, 0, CPU_IRQSTATUS_NONE);
	}

	mapper_map_chr( 8, 0, 0 );

	switch (mapper_regs[0] & 3) {
		case 0: set_mirroring(SINGLE_LOW); break;
		case 1: set_mirroring(SINGLE_HIGH); break;
		case 2: set_mirroring(VERTICAL); break;
		case 3: set_mirroring(HORIZONTAL); break;
	}
}

static void mapper105_cycle()
{
	if (~mapper_regs[1] & 0x10) {
		mapper105_timer[0]++;
		if (mapper105_timer[0] > ((0x10 | (NESDips[2] >> 4)) << 25)) {
			mapper105_timer[0] = 0;
			mapper_irq(0);
		}
	}
}

static void mapper01_exp_write(UINT16 address, UINT8 data) // 6000 - 7fff
{
	if (~mapper_regs[3] & 0x10) {
		Cart.WorkRAM[PRGExpMap + (address & 0x1fff)] = data;
	}
	cart_exp_write_abort = 1; // don't fall-through after callback!
}

static UINT8 mapper01_exp_read(UINT16 address)             // 6000 - 7fff
{
	return (~mapper_regs[3] & 0x10) ? Cart.WorkRAM[PRGExpMap + (address & 0x1fff)] : cpu_open_bus;
}

static void mapper01_map()
{
	mapper01_exbits = ((mapper01_lastwrite == 1) && (mapper_regs[0] & 0x10)) ? mapper_regs[2] : mapper_regs[1];

	if (Cart.WorkRAMSize > 0x2000) {
		// MMC1 Bankable WRAM/SRAM
		UINT8 page = 0;
		switch (Cart.WorkRAMSize) {
			case 0x8000: // 32k (Final Fantasy 1 & 2)
				page = (mapper_regs[1] >> 2) & 3;
				break;
			default:
				page = (mapper_regs[1] >> 3) & 1;
				break;
		}
		mapper_map_exp_prg(page);
	}

	INT32 bigcart = 0;

	if (Cart.PRGRomSize >= 0x80000) {
		bigcart = 1;
		// SUROM (512K/1M rom), use extra mapping bit
		mapper01_prg2x = mapper01_exbits & 0x10;
	}

	if (mapper_regs[0] & 0x8) {
		if (mapper_regs[0] & 0x4) {
            mapper_map_prg(16, 0, (mapper_regs[3] & 0xf) | mapper01_prg2x);
			if (bigcart) {
				mapper_map_prg(16, 1, 0xf | mapper01_prg2x);
			} else {
				mapper_map_prg(16, 1, -1);
			}
        } else {
            mapper_map_prg(16, 0, 0 | mapper01_prg2x);
            mapper_map_prg(16, 1, (mapper_regs[3] & 0xf) | mapper01_prg2x);
        }
	} else {
		mapper_map_prg(32, 0, ((mapper_regs[3] & 0xf) | mapper01_prg2x) >> 1);
	}

	if (mapper_regs[0] & 0x10) {
        mapper_map_chr( 4, 0, mapper_regs[1]);
        mapper_map_chr( 4, 1, mapper_regs[2]);
	} else {
		mapper_map_chr( 8, 0, mapper_regs[1] >> 1);
	}

	switch (mapper_regs[0] & 3) {
        case 0: set_mirroring(SINGLE_LOW); break;
        case 1: set_mirroring(SINGLE_HIGH); break;
        case 2: set_mirroring(VERTICAL); break;
        case 3: set_mirroring(HORIZONTAL); break;
	}
}
#undef mapper01_serialbyte
#undef mapper01_bitcount

// ---[ mapper 02 (UxROM)
static void mapper02_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		mapper_regs[0] = data;
		mapper_map();
	}
}

static void mapper02_map()
{
    mapper_map_prg(16, 0, mapper_regs[0] & 0xff);
    mapper_map_prg(16, 1, -1);
    mapper_map_chr( 8, 0, 0);
}

// ---[ mapper 41 Caltron 6-in-1
#define mapper41_prg	(mapper_regs[0])
#define mapper41_chr	(mapper_regs[1])
#define mapper41_mirror	(mapper_regs[2])

static void mapper41_write(UINT16 address, UINT8 data)
{
	if (address >= 0x6000 && address <= 0x67ff) {
		mapper41_prg = address & 0x07;
		mapper41_chr = (mapper41_chr & 0x03) | ((address & 0x18) >> 1);
		mapper41_mirror = (address & 0x20) >> 5;
	}
	if (address >= 0x8000 && address <= 0xffff) {
		if (mapper41_prg & 4) {
			mapper41_chr = (mapper41_chr & 0x0c) | (data & 0x03);
		}
	}

	mapper_map();
}

static void mapper41_map()
{
	mapper_map_prg(32, 0, mapper41_prg);
	mapper_map_chr( 8, 0, mapper41_chr);

	set_mirroring((mapper41_mirror & 0x01) ? HORIZONTAL : VERTICAL);
}
#undef mapper41_prg
#undef mapper41_chr
#undef mapper41_mirror

// ---[ mapper 53 SuperVision 16-in-1
#define mapper53_reg(x)	(mapper_regs[0 + (x)])

static void mapper53_write(UINT16 address, UINT8 data)
{
	mapper53_reg((address & 0x8000) >> 15) = data;

	mapper_map();
}

static UINT8 mapper53_exp_read(UINT16 address)
{
	return Cart.PRGRom[PRGExpMap + (address & 0x1fff)];
}

static void mapper53_map()
{
	UINT8 bank = (mapper53_reg(0) & 0xf) << 3;
	mapper_map_exp_prg((0xf | (bank << 1)) + 4);
	if (mapper53_reg(0) & 0x10) {
		mapper_map_prg(16, 0, ((bank | (mapper53_reg(1) & 7)) + 2));
		mapper_map_prg(16, 1, ((bank | 7) + 2));
	} else {
		mapper_map_prg(32, 0, 0);
	}

	mapper_map_chr( 8, 0, 0);

	set_mirroring((mapper53_reg(0) & 0x20) ? HORIZONTAL : VERTICAL);
}
#undef mapper53_reg

// ---[ mapper 104 Golden Five, Pegasus 5-in-1
#define mapper104_prg(x)	(mapper_regs[0 + (x)])

static void mapper104_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8000 && address <= 0xbfff) {
		if (data & 0x08) {
			mapper104_prg(0) = (mapper104_prg(0) & 0x0f) | (data & 0x07) << 4;
			mapper104_prg(1) = (data & 0x07) << 4;
		}
	}
	if (address >= 0xc000 && address <= 0xffff) {
		mapper104_prg(0) = (mapper104_prg(0) & 0x70) | (data & 0x0f);
	}

	mapper_map();
}

static void mapper104_map()
{
	mapper_map_prg(16, 0, mapper104_prg(0));
	mapper_map_prg(16, 1, mapper104_prg(1) | 0x0f);
	mapper_map_chr( 8, 0, 0);
}
#undef mapper104_prg

// ---[ mapper 190 Magic Kid Googoo
#define mapper190_prg		(mapper_regs[0])
#define mapper190_chr(x)	(mapper_regs[1 + (x)])
static void mapper190_write(UINT16 address, UINT8 data)
{
	switch (address & ~0x1fff) {
		case 0x8000:
			mapper190_prg = data & 0x07;
			break;
		case 0xa000:
			mapper190_chr(address & 0x03) = data & 0x3f;
			break;
		case 0xc000:
			mapper190_prg = (data & 0x07) | 0x08;
			break;
	}

	mapper_map();
}

static void mapper190_map()
{
    mapper_map_prg(16, 0, mapper190_prg);
    mapper_map_prg(16, 1, 0); // second 16k bank mapped to first prg bank

	mapper_map_chr( 2, 0, mapper190_chr(0));
	mapper_map_chr( 2, 1, mapper190_chr(1));
	mapper_map_chr( 2, 2, mapper190_chr(2));
	mapper_map_chr( 2, 3, mapper190_chr(3));

	set_mirroring(VERTICAL);
}
#undef mapper190_prg
#undef mapper190_chr

// ---[ mapper 193 NTDEC TC-112 (War in the Gulf, Fighting Hero, ...)
#define mapper193_prg		(mapper_regs[0])
#define mapper193_chr(x)	(mapper_regs[1 + (x)])
#define mapper193_mirror	(mapper_regs[4])

static void mapper193_write(UINT16 address, UINT8 data)
{
	switch (address & 0xe007) {
		case 0x6000:
		case 0x6001:
		case 0x6002:
			mapper193_chr(address & 3) = data;
			break;
		case 0x6003:
			mapper193_prg = data;
			break;
		case 0x6004:
			mapper193_mirror = data;
			break;
	}

	mapper_map();
}

static void mapper193_map()
{
	mapper_map_prg( 8, 0, mapper193_prg);
	mapper_map_prg( 8, 1, -3);
	mapper_map_prg( 8, 2, -2);
	mapper_map_prg( 8, 3, -1);

	mapper_map_chr( 4, 0, mapper193_chr(0) >> 2);

	mapper_map_chr( 2, 2, mapper193_chr(1) >> 1);
	mapper_map_chr( 2, 3, mapper193_chr(2) >> 1);

	set_mirroring((mapper193_mirror & 0x01) ? HORIZONTAL : VERTICAL);
}
#undef mapper193_prg
#undef mapper193_chr
#undef mapper193_mirror

// ---[ mapper 72: Jaleco JF-17 (JF-19?), Pinball Quest Japan
#define mapper72_prg		(mapper_regs[0])
#define mapper72_prglatch	(mapper_regs[1])
#define mapper72_chr		(mapper_regs[2])
#define mapper72_chrlatch	(mapper_regs[3])

static void mapper72_write(UINT16 address, UINT8 data)
{
	if (mapper72_prglatch == 0 && data & 0x80) {
		mapper72_prg = data & 0x0f;
	}
	if (mapper72_chrlatch == 0 && data & 0x40) {
		mapper72_chr = data & 0x0f;
	}
	mapper72_prglatch = data & 0x80;
	mapper72_chrlatch = data & 0x40;

	mapper_map();
}

static void mapper72_map()
{
	mapper_map_prg(16, 0, mapper72_prg);
	mapper_map_prg(16, 1, -1);

	mapper_map_chr( 8, 0, mapper72_chr);
}

#undef mapper72_prg
#undef mapper72_chr
#undef mapper72_mirror

// ---[ mapper 15 Contra 168-in-1 Multicart
#define mapper15_prg		(mapper_regs[0])
#define mapper15_prgbit		(mapper_regs[1])
#define mapper15_prgmode	(mapper_regs[2])
#define mapper15_mirror		(mapper_regs[3])

static void mapper15_write(UINT16 address, UINT8 data)
{
	mapper15_mirror = data & 0x40;
	mapper15_prg = (data & 0x7f) << 1;
	mapper15_prgbit = (data & 0x80) >> 7;
	mapper15_prgmode = address & 0xff; // must ignore weird writes.

	mapper_map();
}

static void mapper15_map()
{
	switch (mapper15_prgmode) {
		case 0x00:
			mapper_map_prg( 8, 0, (mapper15_prg + 0) ^ mapper15_prgbit);
			mapper_map_prg( 8, 1, (mapper15_prg + 1) ^ mapper15_prgbit);
			mapper_map_prg( 8, 2, (mapper15_prg + 2) ^ mapper15_prgbit);
			mapper_map_prg( 8, 3, (mapper15_prg + 3) ^ mapper15_prgbit);
			break;
		case 0x01:
			mapper_map_prg( 8, 0, (mapper15_prg + 0) | mapper15_prgbit);
			mapper_map_prg( 8, 1, (mapper15_prg + 1) | mapper15_prgbit);
			mapper_map_prg( 8, 2, (mapper15_prg + 0) | 0x0e | mapper15_prgbit);
			mapper_map_prg( 8, 3, (mapper15_prg + 1) | 0x0e | mapper15_prgbit);
			break;
		case 0x02:
			mapper_map_prg( 8, 0, (mapper15_prg + 0) | mapper15_prgbit);
			mapper_map_prg( 8, 1, (mapper15_prg + 0) | mapper15_prgbit);
			mapper_map_prg( 8, 2, (mapper15_prg + 0) | mapper15_prgbit);
			mapper_map_prg( 8, 3, (mapper15_prg + 0) | mapper15_prgbit);
			break;
		case 0x03:
			mapper_map_prg( 8, 0, (mapper15_prg + 0) | mapper15_prgbit);
			mapper_map_prg( 8, 1, (mapper15_prg + 1) | mapper15_prgbit);
			mapper_map_prg( 8, 2, (mapper15_prg + 0) | mapper15_prgbit);
			mapper_map_prg( 8, 3, (mapper15_prg + 1) | mapper15_prgbit);
			break;
	}

	mapper_map_chr_ramrom( 8, 0, 0, (mapper15_prgmode == 3) ? MEM_RAM_RO : MEM_RAM);

	set_mirroring((mapper15_mirror & 0x40) ? HORIZONTAL : VERTICAL);
}

#undef mapper15_prg
#undef mapper15_prgbit
#undef mapper15_prgmode
#undef mapper15_mirror

// ---[ mapper 389 Caltron 9-in-1
#define mapper389_reg8  (mapper_regs[0])
#define mapper389_reg9  (mapper_regs[1])
#define mapper389_rega  (mapper_regs[2])

static void mapper389_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000) {
		case 0x8000: mapper389_reg8 = address & 0xff; break;
		case 0x9000: mapper389_reg9 = address & 0xff; break;
		default: mapper389_rega = address & 0xff; break;
	}

	mapper_map();
}

static void mapper389_map()
{
	if (mapper389_reg9 & 0x02) { // UNROM-064 mode
		mapper_map_prg(16, 0, (mapper389_reg8 >> 2) | ((mapper389_rega >> 2) & 0x03));
		mapper_map_prg(16, 1, (mapper389_reg8 >> 2) | 0x03);
	} else { // NROM-256 mode
		mapper_map_prg(32, 0, mapper389_reg8 >> 3);
	}
	mapper_map_chr( 8, 0, ((mapper389_reg9 & 0x38) >> 1) | (mapper389_rega & 0x03));

	set_mirroring((mapper389_reg8 & 0x01) ? HORIZONTAL : VERTICAL);
}

#undef mapper389_reg8
#undef mapper389_reg9
#undef mapper389_rega

// ---[ mapper 216 (??) Magic Jewelry 2
#define mapper216_prg   (mapper_regs[0])
#define mapper216_chr   (mapper_regs[1])
static void mapper216_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8000) {
		mapper216_prg = address & 1;
		mapper216_chr = (address & 0x000e) >> 1;
		mapper_map();
	}
}

static UINT8 mapper216_read(UINT16 address)
{
	return 0; // must be 0 @ 0x5000
}

static void mapper216_map()
{
    mapper_map_prg(32, 0, mapper216_prg);
	mapper_map_chr( 8, 0, mapper216_chr);
}
#undef mapper216_prg
#undef mapper216_chr

// ---[ mapper 132 (TXC Micro Genius) Qi Wang
#define mapper132_reg(x)   (mapper_regs[0 + (x)])
#define mapper132_reghi    (mapper_regs[8])
static void mapper132_write(UINT16 address, UINT8 data)
{
	if (address >= 0x4100 && address <= 0x4103) {
		mapper132_reg(address & 3) = data;
		//mapper_map(); // only 8000+!
	}

	if (address >= 0x8000) {
		mapper132_reghi = data;
		mapper_map();
	}
}

static UINT8 mapper132_read(UINT16 address)
{
	if (address == 0x4100) {
		return (mapper132_reg(1) ^ mapper132_reg(2)) | 0x40;
	}
	return cpu_open_bus;
}

static void mapper132_map()
{
    mapper_map_prg(32, 0, (mapper132_reg(2) >> 2) & 1);
	mapper_map_chr( 8, 0, mapper132_reg(2) & 3);
}
#undef mapper132_reg
#undef mapper132_reghi


// flashrom simulator (flash eeprom)
#define flashrom_cmd            (mapper_regs[0x1f - 0x9]) // must not conflict with mmc3 for 406 (Haradius Zero)
#define flashrom_busy           (mapper_regs16[0x00])
#define flashrom_chiptype       (mapper_regs[0x1f - 0xa])
enum { AMIC = 0, MXIC = 1, MC_SST = 2 };

static UINT8 flashrom_read(UINT16 address)
{
	if (flashrom_cmd == 0x90) { // flash chip identification
		//bprintf(0, _T("flashrom chip ID\n"));
		if (flashrom_chiptype == AMIC) {
			switch (address & 0x03) {
				case 0x00: return 0x37; // manufacturer ID
				case 0x01: return 0x86; // device ID
				case 0x03: return 0x7f; // Continuation ID
			}
		} else if (flashrom_chiptype == MXIC) {
			switch (address & 0x03) {
				case 0x00: return 0xc2; // manufacturer ID
				case 0x01: return 0xa4; // device ID
			}
		} else if (flashrom_chiptype == MC_SST) {
			switch (address & 0x03) {
				case 0x00: return 0xbf; // manufacturer ID
				case 0x01: return 0xb7; // device ID
			}
		}
	}

	if (flashrom_busy > 0) { // flash chip program or "erasing sector or chip" mode (it takes time..)
		flashrom_busy--;

		UINT8 status = (flashrom_busy & 0x01) << 6; // toggle bit I
		switch (flashrom_cmd) {
			case 0x82: // embedded erase sector/chip
				status |= (flashrom_busy & 0x01) << 2; // toggle bit II
				status |= 1 << 3; // "erasing" status bit
				if (flashrom_busy < 2) {
					//MXIC MX29F040.pdf, bottom pg. 7
					//"SET-UP AUTOMATIC CHIP/SECTOR ERASE" (last paragraph)
					//...and terminates when the data on Q7 is "1" and
					//the data on Q6 stops toggling for two consecutive read
					//cycles, at which time the device returns to the Read mode
					status = (1 << 7); // Courier doesn't like when the other status bits are set.
				}
				break;
			case 0xa0: // embedded program
				status |= ~mapper_prg_read_int(address) & 0x80;
				break;
		}
		//bprintf(0, _T("erase/pgm status  %x\n"), status);
		if (flashrom_busy == 0) {
			flashrom_cmd = 0; // done! (req: Courier doesn't write 0xf0 (return to read array))
		}
		return status;
	}

	return mapper_prg_read_int(address);
}

static void flashrom_prg_write(UINT16 address, UINT8 data)
{
	//bprintf(0, _T("write byte %x (map/addr: %x  %x)  ->  %x\n"), PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1fff), PRGMap[(address & ~0x8000) / 0x2000], address & 0x1fff, data);
	Cart.PRGRom[PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1fff)] = data;
}

static void flashrom_write(UINT16 address, UINT8 data)
{
	if (data == 0xf0) {
		// read array / reset
		flashrom_cmd = 0;
		flashrom_busy = 0;
		return;
	}

	switch (flashrom_cmd) {
		case 0x00:
		case 0x80:
			if ((address & 0xfff) == 0x555 && data == 0xaa)
				flashrom_cmd++;
			break;
		case 0x01:
		case 0x81:
			if (((address & 0xfff) == 0x2aa ||
				 (address & 0xfff) == 0xaaa) && data == 0x55)
				flashrom_cmd++;

			break;
		case 0x02:
			if ((address & 0xfff) == 0x555) {
				flashrom_cmd = data;
			}
			break;
		case 0x82: {
			switch (data) {
				case 0x10:
					bprintf(0, _T("mapper %d: flashrom - full flash erase not impl. (will break game!)\n"), Cart.Mapper);
					flashrom_busy = Cart.PRGRomSize / 0x100; // fake it
					break;
				case 0x30:
					bprintf(0, _T("mapper %d: flashrom - sector erase.  addr %x [%x]\n"), Cart.Mapper, address, (PRGMap[(address & ~0x8000) / 0x2000] & 0x7ff000));

					if (flashrom_chiptype == MC_SST) {
						for (INT32 i = 0; i < 0x1000; i++) {
							Cart.PRGRom[PRGMap[(address & ~0x8000) / 0x2000] + (address & 0x1000) + i] = data;
						}
						flashrom_busy = 0xfff;
					} else {
						for (INT32 i = 0; i < 0x10000; i++) {
							Cart.PRGRom[(PRGMap[(address & ~0x8000) / 0x2000] & 0x7f0000) + i] = 0xff;
						}
						flashrom_busy = 0xffff;
					}
					break;
			}
			break;
		}
		case 0xa0:
			//bprintf(0, _T("write byte %x  ->  %x\n"), address, data);
			flashrom_prg_write(address, data);
			flashrom_busy = 8;
			flashrom_cmd = 0;
			break;
	}
}



// ---[ mapper 30 (UNROM-512)
#define mapper30_mirroring_en   (mapper_regs[1])
static void mapper30_write(UINT16 address, UINT8 data)
{
	if (address >= 0xc000) {
		mapper_regs[0] = data;
		mapper_map();
	} else {
		flashrom_write(address, data);
	}
}

static void mapper30_map()
{
    mapper_map_prg(16, 0, mapper_regs[0] & 0x1f);
    mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, (mapper_regs[0] >> 5) & 0x03);
	if (mapper30_mirroring_en) {
		set_mirroring((mapper_regs[0] & 0x80) ? SINGLE_HIGH : SINGLE_LOW);
	}
}

// ---[ mapper 03 (CNROM)
#define mapper03_need_update   (mapper_regs[1])
static void mapper03_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		if (NESMode & BUS_CONFLICTS) {
			data &= mapper_prg_read(address);
		}
		mapper_regs[0] = data;
		mapper03_need_update = 1;

		if (Cart.Crc != 0xab29ab28) // dropzone gets delayed a bit.
			mapper_map();
	}
}

static void mapper03_map()
{
    mapper_map_chr( 8, 0, mapper_regs[0]);
}

// dropzone likes to change the chr bank too soon sometimes, this causes a line
// through the player when he is near the bottom of the screen.
// Solution: delay chr bank switches until after sprites load
static void mapper03_cycle()
{
	if (pixel > 321 && mapper03_need_update) {
		mapper03_need_update = 0;
		mapper_map();
	}

}
#undef mapper03_need_update

// ---[ mapper 04 (mmc3) & mmc3-based: 12, 76, 95, 108, 115, 118, 119, 189, 262
#define mapper4_banksel         (mapper_regs[0x1f - 0])
#define mapper4_mirror			(mapper_regs[0x1f - 1])
#define mapper4_irqlatch 		(mapper_regs[0x1f - 2])
#define mapper4_irqcount		(mapper_regs[0x1f - 3])
#define mapper4_irqenable		(mapper_regs[0x1f - 4])
#define mapper4_irqreload		(mapper_regs[0x1f - 5])
#define mapper4_writeprotect    (mapper_regs[0x1f - 6])
#define mapper12_lowchr			(mapper_regs16[0x1f - 0])
#define mapper12_highchr		(mapper_regs16[0x1f - 1])
#define mapper04_vs_prottype    (mapper_regs16[0x1f - 2])
#define mapper04_vs_protidx	    (mapper_regs16[0x1f - 3])
#define mapper115_prg           (mapper_regs[0x1f - 7])
#define mapper115_chr           (mapper_regs[0x1f - 8])
#define mapper115_prot          (mapper_regs[0x1f - 9])
#define mapper258_reg           (mapper_regs[0x1f - 0xa])
#define mapper262_reg           (mapper_regs[0x1f - 0xa])
#define mapper189_reg           (mapper_regs[0x1f - 0xa]) // same as 262
#define mapper268_reg(x)        (mapper_regs[(0x1f - 0xa) + (x)])
// mapper 165 mmc3 w/mmc4-like 4k chr banking latch
#define mapper165_chrlatch(x)   (mapper_regs[(0x1f - 0x0a) + (x)])
#define mapper165_update        (mapper_regs[0x1f - 0xb])

static UINT8 mapper04_vs_rbi_tko_prot(UINT16 address)
{
	static const UINT8 protdata[2][0x20] = {
		{
			0xff, 0xff, 0xff, 0xff, 0xb4, 0xff, 0xff, 0xff,
			0xff, 0x6f, 0xff, 0xff, 0xff, 0xff, 0x94, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
		},
		{
			0xff, 0xbf, 0xb7, 0x97, 0x97, 0x17, 0x57, 0x4f,
			0x6f, 0x6b, 0xeb, 0xa9, 0xb1, 0x90, 0x94, 0x14,
			0x56, 0x4e, 0x6f, 0x6b, 0xeb, 0xa9, 0xb1, 0x90,
			0xd4, 0x5c, 0x3e, 0x26, 0x87, 0x83, 0x13, 0x00
		}

	};
	switch (address) {
		case 0x54ff:
			return 0x05;
		case 0x5678:
			return mapper04_vs_protidx ^ 1;
		case 0x578f:
			return 0x81 | ((mapper04_vs_protidx) ? 0x50 : 0x08);
		case 0x5567:
			mapper04_vs_protidx ^= 1;
			return 0x3e ^ ((mapper04_vs_protidx) ? 0x09 : 0x00);
		case 0x5e00:
			mapper04_vs_protidx = 0;
			break;
		case 0x5e01:
			return protdata[mapper04_vs_prottype][mapper04_vs_protidx++ & 0x1f];
	}
	return cpu_open_bus;
}

static void mapper04_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
        switch (address & 0xE001) {
            case 0x8000: mapper4_banksel = data; break;
            case 0x8001: mapper_regs[(mapper4_banksel & 0x7)] = data; break;
			case 0xA000: mapper4_mirror = ~data & 1; break;
			case 0xA001: mapper4_writeprotect = data; break;
            case 0xC000: mapper4_irqlatch = data; break;
            case 0xC001: mapper4_irqreload = 1; break;
            case 0xE000: mapper4_irqenable = 0; M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
            case 0xE001: mapper4_irqenable = 1; break;
        }
		mapper_map();
    }
}

static void mapper04_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

    if (~mapper4_banksel & 0x80) {
		mapper_map_chr(2, 0, (mapper_regs[0] + mapper12_lowchr) >> 1);
        mapper_map_chr(2, 1, (mapper_regs[1] + mapper12_lowchr) >> 1);

		mapper_map_chr(1, 4, mapper_regs[2] + mapper12_highchr);
		mapper_map_chr(1, 5, mapper_regs[3] + mapper12_highchr);
		mapper_map_chr(1, 6, mapper_regs[4] + mapper12_highchr);
		mapper_map_chr(1, 7, mapper_regs[5] + mapper12_highchr);
	} else {
		mapper_map_chr(1, 0, mapper_regs[2] + mapper12_lowchr);
		mapper_map_chr(1, 1, mapper_regs[3] + mapper12_lowchr);
		mapper_map_chr(1, 2, mapper_regs[4] + mapper12_lowchr);
		mapper_map_chr(1, 3, mapper_regs[5] + mapper12_lowchr);

		mapper_map_chr(2, 2, (mapper_regs[0] + mapper12_highchr) >> 1);
		mapper_map_chr(2, 3, (mapper_regs[1] + mapper12_highchr) >> 1);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

// mapper 555: Nintendo Campus Challenge 1991
// note: extension to mmc3
static INT32 *mapper555_timer = (INT32*)&mapper_regs16[0];

#define mapper555_reg               (mapper_regs[0x1f - 7])
#define mapper555_prgbase           (mapper_regs[0x1f - 8])
#define mapper555_prgmask           (mapper_regs[0x1f - 9])
#define mapper555_chrbase           (mapper_regs[0x1f - 10])
#define mapper555_chrmode           (mapper_regs[0x1f - 11])

static void mapper555_map_prg(INT32 pagesz, INT32 slot, INT32 bank)
{
	mapper_map_prg(8, slot, mapper555_prgbase | (bank & mapper555_prgmask));
}

static void mapper555_map_chr(INT32 pagesz, INT32 slot, INT32 bank)
{
	const UINT8 masks[] = { 0x7f, 0x3f, 0x07, 0x07 };
	const UINT8 mask = masks[(mapper555_chrmode) ? (((bank & 0x40) >> 5) + mapper555_chrmode) : 0];

	mapper_map_chr_ramrom(1, slot, mapper555_chrbase + (bank & mask), (mask == 0x07) ? MEM_RAM : MEM_ROM);
}

static void mapper555_map()
{
	mapper555_map_prg(8, 1, mapper_regs[7]);

	const INT32 prg_rev = (mapper4_banksel & 0x40) ? 2 : 0;
	mapper555_map_prg(8, 0 ^ prg_rev, mapper_regs[6]);
	mapper555_map_prg(8, 2 ^ prg_rev, -2);

	mapper555_map_prg( 8, 3, -1);

	const INT32 chr_rev = (mapper4_banksel & 0x80) ? 4 : 0;
	mapper555_map_chr(1, 0 ^ chr_rev, mapper_regs[0] & ~1);
	mapper555_map_chr(1, 1 ^ chr_rev, mapper_regs[0] | 1);
	mapper555_map_chr(1, 2 ^ chr_rev, mapper_regs[1] & ~1);
	mapper555_map_chr(1, 3 ^ chr_rev, mapper_regs[1] | 1);

	mapper555_map_chr(1, 4 ^ chr_rev, mapper_regs[2]);
	mapper555_map_chr(1, 5 ^ chr_rev, mapper_regs[3]);
	mapper555_map_chr(1, 6 ^ chr_rev, mapper_regs[4]);
	mapper555_map_chr(1, 7 ^ chr_rev, mapper_regs[5]);

	if (Cart.Mirroring != 4) { // MMC3: 4 (FOUR_SCREEN) keeps it locked!
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
	}
}

// callbacks for read/writing WRAM
static void mapper555_prgwrite(UINT16 address, UINT8 data)
{
	cart_exp_write_abort = (mapper4_writeprotect & 0xc0) != 0x80;
}

static UINT8 mapper555_prgread(UINT16 address)
{
	return (mapper4_writeprotect & 0x80) ? Cart.WorkRAM[address & Cart.WorkRAMMask] : cpu_open_bus;
}

static UINT8 mapper555_read(UINT16 address)
{
	if (address < 0x5000) {
		return cpu_open_bus;
	}
	if (address >= 0x5000 && address <= 0x57ff) {
		return Cart.WorkRAM[0x2000 | (address & 0x7ff)];
	} else {
		return (mapper555_timer[0] > ((0x10 | (NESDips[2] >> 4)) << 25)) ? 0x80 : 0x00;
	}
	return cpu_open_bus;
}

static void mapper555_write(UINT16 address, UINT8 data)
{
	if (address < 0x5000) return;

	if (address >= 0x5000 && address <= 0x57ff) {
		Cart.WorkRAM[0x2000 | (address & 0x7ff)] = data;
		return;
	}

	if (address >= 0x5800 && address <= 0x5bff) {
		mapper555_reg = data;

		mapper555_prgbase = (data << 3) & 0x20;
		mapper555_prgmask =((data & 0x03) << 3) | 0x07;
		mapper555_chrbase = (data & 0x04) << 5;
		mapper555_chrmode = (data & 0x06) == 0x02; // 2 = "tqrom, aka mapper119" mode. normal mmc3 otherwise

		if (~mapper555_reg & 8) { // timer disabled, reset timer
			mapper555_timer[0] = 0;
		}

		mapper_map();
		return;
	}
}

static void mapper555_cycle()
{
	if (mapper555_reg & 8) {
		mapper555_timer[0]++;
	}
}

static void mapper258_map()
{
	if (mapper258_reg & 0x80) {
		// mmc3 prg-override (similar to mapper 115)
		if (mapper258_reg & 0x20) {
			mapper_map_prg(32, 0, (mapper258_reg & 0x7) >> 1);
		} else {
			mapper_map_prg(16, 0, mapper258_reg & 0x7);
			mapper_map_prg(16, 1, mapper258_reg & 0x7);
		}
	} else {
		mapper_map_prg(8, 1, mapper_regs[7] & 0xf);

		if (~mapper4_banksel & 0x40) {
			mapper_map_prg(8, 0, mapper_regs[6] & 0xf);
			mapper_map_prg(8, 2, -2);
		} else {
			mapper_map_prg(8, 0, -2);
			mapper_map_prg(8, 2, mapper_regs[6] & 0xf);
		}
		mapper_map_prg(8, 3, -1);
	}

    if (~mapper4_banksel & 0x80) {
		mapper_map_chr(2, 0, (mapper_regs[0] + mapper12_lowchr) >> 1);
        mapper_map_chr(2, 1, (mapper_regs[1] + mapper12_lowchr) >> 1);

		mapper_map_chr(1, 4, mapper_regs[2] + mapper12_highchr);
		mapper_map_chr(1, 5, mapper_regs[3] + mapper12_highchr);
		mapper_map_chr(1, 6, mapper_regs[4] + mapper12_highchr);
		mapper_map_chr(1, 7, mapper_regs[5] + mapper12_highchr);
	} else {
		mapper_map_chr(1, 0, mapper_regs[2] + mapper12_lowchr);
		mapper_map_chr(1, 1, mapper_regs[3] + mapper12_lowchr);
		mapper_map_chr(1, 2, mapper_regs[4] + mapper12_lowchr);
		mapper_map_chr(1, 3, mapper_regs[5] + mapper12_lowchr);

		mapper_map_chr(2, 2, (mapper_regs[0] + mapper12_highchr) >> 1);
		mapper_map_chr(2, 3, (mapper_regs[1] + mapper12_highchr) >> 1);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static UINT8 mapper258_read(UINT16 address)
{
	const UINT8 lut[8] = { 0, 0, 0, 1, 2, 4, 0xf, 0 };

	return cpu_open_bus | lut[address & 7];
}

static void mapper258_write(UINT16 address, UINT8 data)
{
	if ((address & 0x7) == 0x0) {
		mapper258_reg = data;

		mapper_map();
	}
}

static void mapper76_map()
{
	mapper_map_prg(8, 0, mapper_regs[6]);
    mapper_map_prg(8, 1, mapper_regs[7]);
	mapper_map_prg(8, 2, -2);

	mapper_map_chr(2, 0, mapper_regs[2]);
	mapper_map_chr(2, 1, mapper_regs[3]);
	mapper_map_chr(2, 2, mapper_regs[4]);
	mapper_map_chr(2, 3, mapper_regs[5]);

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static void mapper95_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

	mapper_map_prg(8, 0, mapper_regs[6]);
	mapper_map_prg(8, 2, -2);

	mapper_map_chr(2, 0, mapper_regs[0] >> 1);
	mapper_map_chr(2, 1, mapper_regs[1] >> 1);

	nametable_mapall((mapper_regs[0] >> 5) & 1, (mapper_regs[0] >> 5) & 1, (mapper_regs[1] >> 5) & 1, (mapper_regs[1] >> 5) & 1);

	mapper_map_chr(1, 4, mapper_regs[2]);
	mapper_map_chr(1, 5, mapper_regs[3]);
	mapper_map_chr(1, 6, mapper_regs[4]);
	mapper_map_chr(1, 7, mapper_regs[5]);
}

static void mapper268_prg(INT32 slot, INT32 bank)
{
	UINT32 prgmask = ((~mapper268_reg(3) & 0x10) >> 4) * 0x0f; // prg a13, a14, a15, a16 (GNROM)
		   prgmask |= (~mapper268_reg(0) & 0x40) >> 2; // prg a17~
		   prgmask |= (~mapper268_reg(1) & 0x80) >> 2; // prg a18~
		   prgmask |=   mapper268_reg(1) & 0x40;       // prg a19
		   prgmask |= ( mapper268_reg(1) & 0x20) << 2; // prg a20

	UINT32 prgbase  = (mapper268_reg(3) & 0x0e);       // prg a16, a15, a14 (GNROM)
		   prgbase |= (mapper268_reg(0) & 0x07) << 4;  // prg a19, a18, a17
		   prgbase |= (mapper268_reg(1) & 0x10) << 3;  // prg a20
		   prgbase |= (mapper268_reg(1) & 0x0c) << 6;  // prg a22, a21
		   prgbase |= (mapper268_reg(0) & 0x30) << 6;  // prg a24, a23
		   prgbase &= ~prgmask;

	UINT32 gnprgmask = (mapper268_reg(3) & 0x10) ? ((mapper268_reg(1) & 0x02) | 0x01) : 0x00;

	mapper_map_prg(8, slot, prgbase | (bank & prgmask) | (slot & gnprgmask));
}

// mapper 268: coolboy / mindkids
static void mapper268_map()
{
	if (mapper268_reg(3) & (0x40)) {
		bprintf(0, _T(" **  Mapper 268: 'weird' modes not supported yet.\n"));
	}

	mapper268_prg(1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
		mapper268_prg(0, mapper_regs[6]);
		mapper268_prg(2, -2);
    } else {
		mapper268_prg(0, -2);
		mapper268_prg(2, mapper_regs[6]);
    }
	mapper268_prg(3, -1);

	// chr - only mmc3 for now! -dink feb3. 2022
	INT32 chrmask = (mapper268_reg(0) & 0x80) ? 0x7f : 0xff;
	INT32 chrbase = ((mapper268_reg(0) & 0x08) << 4) & ~chrmask;

	if (~mapper4_banksel & 0x80) {
		mapper_map_chr(2, 0, ((mapper_regs[0] & chrmask) + chrbase) >> 1);
        mapper_map_chr(2, 1, ((mapper_regs[1] & chrmask) + chrbase) >> 1);

		mapper_map_chr(1, 4, (mapper_regs[2] & chrmask) + chrbase);
		mapper_map_chr(1, 5, (mapper_regs[3] & chrmask) + chrbase);
		mapper_map_chr(1, 6, (mapper_regs[4] & chrmask) + chrbase);
		mapper_map_chr(1, 7, (mapper_regs[5] & chrmask) + chrbase);
	} else {
		mapper_map_chr(1, 0, (mapper_regs[2] & chrmask) + chrbase);
		mapper_map_chr(1, 1, (mapper_regs[3] & chrmask) + chrbase);
		mapper_map_chr(1, 2, (mapper_regs[4] & chrmask) + chrbase);
		mapper_map_chr(1, 3, (mapper_regs[5] & chrmask) + chrbase);

		mapper_map_chr(2, 2, ((mapper_regs[0] & chrmask) + chrbase) >> 1);
		mapper_map_chr(2, 3, ((mapper_regs[1] & chrmask) + chrbase) >> 1);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static void mapper268_write(UINT16 address, UINT8 data)
{
	if (address < 0x8000) {
		cart_exp_write_abort = (mapper4_writeprotect & 0xc0) == 0x80;

		if ((Cart.SubMapper == 1 && address >= 0x5000 && address <= 0x5fff) ||
		   (Cart.SubMapper == 0 && address >= 0x6000 && address <= 0x6fff))
		{
			if ((mapper268_reg(3) & 0x90) != 0x80) {
				mapper268_reg(address & 3) = data;

				mapper_map();
			}
		}
	} else {
		mapper04_write(address, data);
	}
}

static void mapper12_write(UINT16 address, UINT8 data)
{
	if (address & 0x4000) {
		mapper12_lowchr  = (data & 0x01) << 8;
		mapper12_highchr = (data & 0x10) << 4;

		mapper_map();
	}
}

static UINT8 mapper12_read(UINT16 address)
{
	UINT8 ret = cpu_open_bus;

	if (address & 0x4000) {
		ret = mapper12_lowchr >> 8;
		ret |= mapper12_highchr >> 4;
	}
	return ret;
}

static void mapper95_write(UINT16 address, UINT8 data)
{
	if ((address & 0x8001) == 0x8000)
		data &= 0x3f;

	mapper04_write(address & 0x8001, data);

	mapper_map();
}

static void mapper115_map()
{
	if (mapper115_prg & 0x80) {
		// mmc3 prg-override
		if (mapper115_prg & 0x20) {
			mapper_map_prg(32, 0, (mapper115_prg & 0xf) >> 1);
		} else {
			mapper_map_prg(16, 0, mapper115_prg & 0xf);
			mapper_map_prg(16, 1, mapper115_prg & 0xf);
		}
	} else {
		mapper_map_prg(8, 1, mapper_regs[7]);

		if (~mapper4_banksel & 0x40) {
			mapper_map_prg(8, 0, mapper_regs[6]);
			mapper_map_prg(8, 2, -2);
		} else {
			mapper_map_prg(8, 0, -2);
			mapper_map_prg(8, 2, mapper_regs[6]);
		}
		mapper_map_prg( 8, 3, -1);
	}

    if (~mapper4_banksel & 0x80) {
		mapper_map_chr(2, 0, (mapper_regs[0] + mapper12_lowchr) >> 1);
        mapper_map_chr(2, 1, (mapper_regs[1] + mapper12_lowchr) >> 1);

		mapper_map_chr(1, 4, mapper_regs[2] + mapper12_highchr);
		mapper_map_chr(1, 5, mapper_regs[3] + mapper12_highchr);
		mapper_map_chr(1, 6, mapper_regs[4] + mapper12_highchr);
		mapper_map_chr(1, 7, mapper_regs[5] + mapper12_highchr);
	} else {
		mapper_map_chr(1, 0, mapper_regs[2] + mapper12_lowchr);
		mapper_map_chr(1, 1, mapper_regs[3] + mapper12_lowchr);
		mapper_map_chr(1, 2, mapper_regs[4] + mapper12_lowchr);
		mapper_map_chr(1, 3, mapper_regs[5] + mapper12_lowchr);

		mapper_map_chr(2, 2, (mapper_regs[0] + mapper12_highchr) >> 1);
		mapper_map_chr(2, 3, (mapper_regs[1] + mapper12_highchr) >> 1);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static UINT8 mapper115_read(UINT16 address)
{
	UINT8 ret = cpu_open_bus;

	switch (address) {
		case 0x5080:
			ret = mapper115_prot;
			break;
		case 0x6000:
			ret = mapper115_prg;
			break;
		case 0x6001:
			ret = mapper115_chr;
			break;
	}
	return ret;
}

static void mapper115_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x5080:
			mapper115_prot = data;
			break;
		case 0x6000:
			mapper115_prg = data;
			break;
		case 0x6001:
			mapper115_chr = data;
			mapper12_highchr = (data & 1) << 8;
			mapper12_lowchr = (data & 1) << 8;
			break;
	}

	mapper_map();
}

static void mapper118_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

    if (~mapper4_banksel & 0x80) {
		mapper_map_chr(2, 0, mapper_regs[0] >> 1);
        mapper_map_chr(2, 1, mapper_regs[1] >> 1);
		nametable_map(0, (mapper_regs[0] >> 7) & 1);
		nametable_map(1, (mapper_regs[0] >> 7) & 1);
		nametable_map(2, (mapper_regs[1] >> 7) & 1);
		nametable_map(3, (mapper_regs[1] >> 7) & 1);

		mapper_map_chr(1, 4, mapper_regs[2]);
		mapper_map_chr(1, 5, mapper_regs[3]);
		mapper_map_chr(1, 6, mapper_regs[4]);
		mapper_map_chr(1, 7, mapper_regs[5]);
	} else {
		mapper_map_chr(1, 0, mapper_regs[2]);
		mapper_map_chr(1, 1, mapper_regs[3]);
		mapper_map_chr(1, 2, mapper_regs[4]);
		mapper_map_chr(1, 3, mapper_regs[5]);
		nametable_map(0, (mapper_regs[2] >> 7) & 1);
		nametable_map(1, (mapper_regs[3] >> 7) & 1);
		nametable_map(2, (mapper_regs[4] >> 7) & 1);
		nametable_map(3, (mapper_regs[5] >> 7) & 1);

		mapper_map_chr(2, 2, mapper_regs[0] >> 1);
		mapper_map_chr(2, 3, mapper_regs[1] >> 1);
	}
}

static void mapper189_write(UINT16 address, UINT8 data)
{
	mapper189_reg = data | (data >> 4);
	mapper_map();
}

static void mapper189_map()
{
	mapper_map_prg(32, 0, mapper189_reg & 0x07);

    if (~mapper4_banksel & 0x80) {
		mapper_map_chr(2, 0, mapper_regs[0] >> 1);
        mapper_map_chr(2, 1, mapper_regs[1] >> 1);

		mapper_map_chr(1, 4, mapper_regs[2]);
		mapper_map_chr(1, 5, mapper_regs[3]);
		mapper_map_chr(1, 6, mapper_regs[4]);
		mapper_map_chr(1, 7, mapper_regs[5]);
	} else {
		mapper_map_chr(1, 0, mapper_regs[2]);
		mapper_map_chr(1, 1, mapper_regs[3]);
		mapper_map_chr(1, 2, mapper_regs[4]);
		mapper_map_chr(1, 3, mapper_regs[5]);

		mapper_map_chr(2, 2, mapper_regs[0] >> 1);
		mapper_map_chr(2, 3, mapper_regs[1] >> 1);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static void mapper119_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

    if (~mapper4_banksel & 0x80) {
		mapper_map_chr_ramrom(2, 0, (mapper_regs[0] >> 1) & 0x1f, ((mapper_regs[0]) & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(2, 1, (mapper_regs[1] >> 1) & 0x1f, ((mapper_regs[1]) & 0x40) ? MEM_RAM : MEM_ROM);

		mapper_map_chr_ramrom(1, 4, mapper_regs[2] & 0x3f, (mapper_regs[2] & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(1, 5, mapper_regs[3] & 0x3f, (mapper_regs[3] & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(1, 6, mapper_regs[4] & 0x3f, (mapper_regs[4] & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(1, 7, mapper_regs[5] & 0x3f, (mapper_regs[5] & 0x40) ? MEM_RAM : MEM_ROM);
	} else {
		mapper_map_chr_ramrom(1, 0, mapper_regs[2] & 0x3f, (mapper_regs[2] & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(1, 1, mapper_regs[3] & 0x3f, (mapper_regs[3] & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(1, 2, mapper_regs[4] & 0x3f, (mapper_regs[4] & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(1, 3, mapper_regs[5] & 0x3f, (mapper_regs[5] & 0x40) ? MEM_RAM : MEM_ROM);

		mapper_map_chr_ramrom(2, 2, (mapper_regs[0] >> 1) & 0x1f, ((mapper_regs[0]) & 0x40) ? MEM_RAM : MEM_ROM);
		mapper_map_chr_ramrom(2, 3, (mapper_regs[1] >> 1) & 0x1f, ((mapper_regs[1]) & 0x40) ? MEM_RAM : MEM_ROM);
	}

	if (Cart.Mirroring != 4)
		set_mirroring((mapper4_mirror) ? VERTICAL : HORIZONTAL);
}

static void mapper165_ppu_clock(UINT16 address)
{
	if (mapper165_update) {
		mapper_map();
		mapper165_update = 0;
	}

	switch (address & 0x3ff8) {
		case 0x0fd0:
			mapper165_chrlatch(0) = 0;
			mapper165_update = 1;
			break;
		case 0x0fe8:
			mapper165_chrlatch(0) = 1;
			mapper165_update = 1;
			break;
		case 0x1fd0:
			mapper165_chrlatch(1) = 2;
			mapper165_update = 1;
			break;
		case 0x1fe8:
			mapper165_chrlatch(1) = 4;
			mapper165_update = 1;
			break;
	}
}

static void mapper165_chrmap(INT32 slot, INT32 bank)
{
	mapper_map_chr_ramrom(4, slot, bank >> 2, (bank == 0x00) ? MEM_RAM : MEM_ROM);
}

static void mapper165_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

	mapper165_chrmap(0, mapper_regs[mapper165_chrlatch(0)]);
	mapper165_chrmap(1, mapper_regs[mapper165_chrlatch(1)]);

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

// mapper 194: mmc3 + chrram banks 0-1
static void mapper194_chrmap(INT32 slot, INT32 bank)
{
	mapper_map_chr_ramrom(1, slot, bank, (bank >= 0x0 && bank <= 0x1) ? MEM_RAM : MEM_ROM);
}

static void mapper194_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

    if (~mapper4_banksel & 0x80) {
		mapper194_chrmap(0, mapper_regs[0] & 0xfe);
		mapper194_chrmap(1, mapper_regs[0] | 0x01);
        mapper194_chrmap(2, mapper_regs[1] & 0xfe);
        mapper194_chrmap(3, mapper_regs[1] | 0x01);

		mapper194_chrmap(4, mapper_regs[2]);
		mapper194_chrmap(5, mapper_regs[3]);
		mapper194_chrmap(6, mapper_regs[4]);
		mapper194_chrmap(7, mapper_regs[5]);
	} else {
		mapper194_chrmap(0, mapper_regs[2]);
		mapper194_chrmap(1, mapper_regs[3]);
		mapper194_chrmap(2, mapper_regs[4]);
		mapper194_chrmap(3, mapper_regs[5]);

		mapper194_chrmap(4, mapper_regs[0] & 0xfe);
		mapper194_chrmap(5, mapper_regs[0] | 0x01);
		mapper194_chrmap(6, mapper_regs[1] & 0xfe);
		mapper194_chrmap(7, mapper_regs[1] | 0x01);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static void mapper192_chrmap(INT32 slot, INT32 bank)
{
	mapper_map_chr_ramrom(1, slot, bank, (bank >= 0x8 && bank <= 0xb) ? MEM_RAM : MEM_ROM);
}

static void mapper192_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

    if (~mapper4_banksel & 0x80) {
		mapper192_chrmap(0, mapper_regs[0] & 0xfe);
		mapper192_chrmap(1, mapper_regs[0] | 0x01);
        mapper192_chrmap(2, mapper_regs[1] & 0xfe);
        mapper192_chrmap(3, mapper_regs[1] | 0x01);

		mapper192_chrmap(4, mapper_regs[2]);
		mapper192_chrmap(5, mapper_regs[3]);
		mapper192_chrmap(6, mapper_regs[4]);
		mapper192_chrmap(7, mapper_regs[5]);
	} else {
		mapper192_chrmap(0, mapper_regs[2]);
		mapper192_chrmap(1, mapper_regs[3]);
		mapper192_chrmap(2, mapper_regs[4]);
		mapper192_chrmap(3, mapper_regs[5]);

		mapper192_chrmap(4, mapper_regs[0] & 0xfe);
		mapper192_chrmap(5, mapper_regs[0] | 0x01);
		mapper192_chrmap(6, mapper_regs[1] & 0xfe);
		mapper192_chrmap(7, mapper_regs[1] | 0x01);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

// mapper 195: mmc3 + chrram banks 0-3, cpu r/w @ 5000-5fff)

static void mapper195_chrmap(INT32 slot, INT32 bank)
{
	mapper_map_chr_ramrom(1, slot, bank, (bank >= 0x0 && bank <= 0x3) ? MEM_RAM : MEM_ROM);
}

static void mapper195_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

    if (~mapper4_banksel & 0x80) {
		mapper195_chrmap(0, mapper_regs[0] & 0xfe);
		mapper195_chrmap(1, mapper_regs[0] | 0x01);
        mapper195_chrmap(2, mapper_regs[1] & 0xfe);
        mapper195_chrmap(3, mapper_regs[1] | 0x01);

		mapper195_chrmap(4, mapper_regs[2]);
		mapper195_chrmap(5, mapper_regs[3]);
		mapper195_chrmap(6, mapper_regs[4]);
		mapper195_chrmap(7, mapper_regs[5]);
	} else {
		mapper195_chrmap(0, mapper_regs[2]);
		mapper195_chrmap(1, mapper_regs[3]);
		mapper195_chrmap(2, mapper_regs[4]);
		mapper195_chrmap(3, mapper_regs[5]);

		mapper195_chrmap(4, mapper_regs[0] & 0xfe);
		mapper195_chrmap(5, mapper_regs[0] | 0x01);
		mapper195_chrmap(6, mapper_regs[1] & 0xfe);
		mapper195_chrmap(7, mapper_regs[1] | 0x01);
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static void mapper195_write(UINT16 address, UINT8 data)
{
	if (address >= 0x5000 && address <= 0x5fff) {
		Cart.CHRRam[address&0xfff] = data;
	}
}

static UINT8 mapper195_read(UINT16 address)
{
	if (address >= 0x5000 && address <= 0x5fff) {
		return Cart.CHRRam[address&0xfff];
	}
	return cpu_open_bus;
}

static void mapper262_map()
{
    mapper_map_prg(8, 1, mapper_regs[7]);

    if (~mapper4_banksel & 0x40) {
        mapper_map_prg(8, 0, mapper_regs[6]);
        mapper_map_prg(8, 2, -2);
    } else {
        mapper_map_prg(8, 0, -2);
        mapper_map_prg(8, 2, mapper_regs[6]);
    }

	if (mapper262_reg & 0x40) {
	   mapper_map_chr_ramrom(8, 0, 0, MEM_RAM);
	} else {
		mapper_set_chrtype(MEM_ROM);

		if (~mapper4_banksel & 0x80) {
			mapper_map_chr(1, 0, (mapper_regs[0] & 0xfe) + ((mapper262_reg & 8) << 5));
			mapper_map_chr(1, 1, (mapper_regs[0] | 0x01) + ((mapper262_reg & 8) << 5));

			mapper_map_chr(1, 2, (mapper_regs[1] & 0xfe) + ((mapper262_reg & 4) << 6));
			mapper_map_chr(1, 3, (mapper_regs[1] | 0x01) + ((mapper262_reg & 4) << 6));

			mapper_map_chr(1, 4, mapper_regs[2] + ((mapper262_reg & 1) << 8));
			mapper_map_chr(1, 5, mapper_regs[3] + ((mapper262_reg & 1) << 8));
			mapper_map_chr(1, 6, mapper_regs[4] + ((mapper262_reg & 2) << 7));
			mapper_map_chr(1, 7, mapper_regs[5] + ((mapper262_reg & 2) << 7));
		} else {
			mapper_map_chr(1, 0, mapper_regs[2] + ((mapper262_reg & 8) << 5));
			mapper_map_chr(1, 1, mapper_regs[3] + ((mapper262_reg & 8) << 5));
			mapper_map_chr(1, 2, mapper_regs[4] + ((mapper262_reg & 4) << 6));
			mapper_map_chr(1, 3, mapper_regs[5] + ((mapper262_reg & 4) << 6));

			mapper_map_chr(1, 4, (mapper_regs[0] & 0xfe) + ((mapper262_reg & 1) << 8));
			mapper_map_chr(1, 5, (mapper_regs[0] | 0x01) + ((mapper262_reg & 1) << 8));

			mapper_map_chr(1, 6, (mapper_regs[1] & 0xfe) + ((mapper262_reg & 2) << 7));
			mapper_map_chr(1, 7, (mapper_regs[1] | 0x01) + ((mapper262_reg & 2) << 7));
		}
	}

	if (Cart.Mirroring != 4)
		set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

static UINT8 mapper262_read(UINT16 address)
{
	if (address == 0x4100) {
		// 0 = Street heroes, 0xff = Shihun
		return 0;
	}

	return 0;
}

static void mapper262_write(UINT16 address, UINT8 data)
{
	if (address == 0x4100) {
		mapper262_reg = data;
		mapper_map();
	}
}

// ---[ mapper 451: Haratyler HG (AMIC flashrom)
#define mapper451_bank          (mapper_regs[0])

static void mapper451_write(UINT16 address, UINT8 data)
{
	flashrom_write(address, data);

	if (address & 0x8000) {
        switch (address & 0xe000) {
			case 0xa000: {
				mapper4_mirror = ~address & 1;
				mapper_map();
				break;
			}
			case 0xc000: {
				mapper4_irqlatch = (address & 0xff) - 1;
				mapper4_irqreload = 0;
				if ((address & 0xff) == 0xff) {
					mapper4_irqlatch = 0;
					mapper4_irqenable = 0;
					M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				} else {
					mapper4_irqenable = 1;
				}
				break;
			}
			case 0xe000: {
				mapper451_bank = address & 3;
				mapper_map();
				break;
			}
        }
    }
}

static void mapper451_map()
{
	mapper_map_prg(8, 0, (0x00) );
	mapper_map_prg(8, 1, (0x10 + (mapper451_bank & 1) + ((mapper451_bank & 2) << 2)) );
	mapper_map_prg(8, 2, (0x20 + (mapper451_bank & 1) + ((mapper451_bank & 2) << 2)) );
    mapper_map_prg(8, 3, (0x30) );

	mapper_map_chr(8, 0, mapper451_bank & 1);

	set_mirroring(mapper4_mirror ? VERTICAL : HORIZONTAL);
}

// mapper 406 - haradius zero (mmc3 + MXIC flash chip(prg,rw) + MXIC flash chip(chr,r)
// dev notes: https://pastebin.com/9UH8yGg6
static void mapper406_write(UINT16 address, UINT8 data)
{
	flashrom_write(address, data);
	mapper04_write((address & 0xfffe) | ((address >> 1) & 1), data);
}

#undef flashrom_cmd
#undef flashrom_busy

static void mapper04_scanline()
{
	if (NESMode & ALT_MMC3 && !RENDERING) {
		return;
	}

	{ // a12 counter stop
		const INT32 spr = (ppu_ctrl & ctrl_sprtab) >> 3;
		const INT32 bg = (ppu_ctrl & ctrl_bgtab) >> 4;
		const INT32 _16 = (ppu_ctrl & ctrl_sprsize) >> 5;

		// if spr table == bg table data, a12 will never clock
		// in 16x8 sprite mode, a12 will "most likely" clock

		if (spr == bg && _16 == 0) return;
	}

	INT32 cnt = mapper4_irqcount || (NESMode & ALT_MMC3);
	if (mapper4_irqcount == 0 || mapper4_irqreload) {
		mapper4_irqcount = mapper4_irqlatch;
	} else {
		mapper4_irqcount--;
	}

	if ((cnt || mapper4_irqreload) && mapper4_irqenable && mapper4_irqcount == 0) {
		if ((~NESMode & ALT_MMC3 && RENDERING) || NESMode & ALT_MMC3) { // aka if (RENDERING)
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
	}
	mapper4_irqreload = 0;
}

//#undef mapper4_mirror // used in mapper_init()
#undef mapper4_irqlatch
#undef mapper4_irqcount
#undef mapper4_irqenable
#undef mapper4_irqreload
#undef mapper4_banksel

// ---[ mapper 05 (MMC5) Castlevania III, Uchuu Keibitai SDF
// PPU Hooks
static UINT8 *mmc5_nt_ram; // pointer to our ppu's nt ram
#define MMC58x16 (ppu_ctrl & ctrl_sprsize)

// Mapper registers & ram
static UINT8 mmc5_expram[1024];

#define mmc5_prgmode		(mapper_regs[0x0])
#define mmc5_chrmode		(mapper_regs[0x1])
#define mmc5_prgprot1		(mapper_regs[0x2])
#define mmc5_prgprot2		(mapper_regs[0x3])
#define mmc5_expram_mode	(mapper_regs[0x4])
#define mmc5_mirror(x)		(mapper_regs[0x1b + (x)])
#define mmc5_filltile		(mapper_regs[0x5])
#define mmc5_fillcolor		(mapper_regs[0x6])
#define mmc5_prgexp			(mapper_regs[0x7])

#define mmc5_prg(x)			(mapper_regs16[0x4 + (x)])
#define mmc5_chr(x)			(mapper_regs16[0x10 + (x)])
#define mmc5_upperchr		(mapper_regs[0x8])

#define mmc5_split			(mapper_regs[0x9])
#define mmc5_splitside		(mapper_regs[0xa])
#define mmc5_splittile		(mapper_regs[0xb])
#define mmc5_splitscroll	(mapper_regs[0xc])
#define mmc5_splitscrollmod (mapper_regs[0xd])
#define mmc5_splitchr		(mapper_regs[0xe])

#define mmc5_irqenable		(mapper_regs[0xf])
#define mmc5_irqcompare		(mapper_regs[0x10])
#define mmc5_irqpend		(mapper_regs[0x11])

#define mmc5_mult0			(mapper_regs[0x12])
#define mmc5_mult1			(mapper_regs[0x13])

#define mmc5_irqcount		(mapper_regs[0x14])
#define mmc5_inframe		(mapper_regs[0x15])

#define mmc5_lastchr		(mapper_regs[0x16])
#define mmc5_expramattr		(mapper_regs[0x17])

#define mmc5_pcmwrmode		(mapper_regs[0x18])
#define mmc5_pcmirq			(mapper_regs[0x19])
#define mmc5_pcmdata		(mapper_regs[0x1a])

enum { CHR_GUESS = 0, CHR_TILE, CHR_SPRITE, CHR_LASTREG };

static void mapper5_reset()
{
	memset(&mmc5_expram, 0xff, sizeof(mmc5_expram));

	mmc5_prgmode = 3;
	mmc5_chrmode = 3;
	mmc5_prgexp = 7;

	mmc5_prg(0) = 0x7f;
	mmc5_prg(1) = 0x7f;
	mmc5_prg(2) = 0x7f;
	mmc5_prg(3) = 0x7f;

	mmc5_filltile = 0xff;
	mmc5_fillcolor = 0xff;
	mmc5_mult0 = 0xff;
	mmc5_mult1 = 0xff; // default

	mmc5_pcmwrmode = 0x00;
	mmc5_pcmirq = 0x00;
	mmc5_pcmdata = 0x00;
}

static void mapper5_scan()
{
	SCAN_VAR(mmc5_expram);
}

static INT16 mapper5_mixer()
{
	return (INT16)(mmc5_pcmdata << 4);
}

static void mmc5_mapchr(UINT8 type)
{
	// https://wiki.nesdev.com/w/index.php/MMC5#CHR_mode_.28.245101.29
	// When using 8x8 sprites, only registers $5120-$5127 are used. Registers $5128-$512B are completely ignored.
	// When using 8x16 sprites, registers $5120-$5127 specify banks for sprites, registers $5128-$512B apply to background tiles, and the last set of registers written to (either $5120-$5127 or $5128-$512B) will be used for I/O via PPUDATA ($2007).

	if (~MMC58x16) mmc5_lastchr = 0;

	UINT8 bank = (MMC58x16 == 0) || (MMC58x16 && type == CHR_SPRITE);

	if (MMC58x16 && type == CHR_LASTREG)
		bank = mmc5_lastchr < 8;

	UINT8 banks[0x2][0xf] = {
/*bg*/  { 0xb, 0xb, 0xb, 0x9, 0xb, 0x9, 0xb, 0x8, 0x9, 0xa, 0xb, 0x8, 0x9, 0xa, 0xb },
/*spr*/ { 0x7, 0x3, 0x7, 0x1, 0x3, 0x5, 0x7, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7 } };

	switch (mmc5_chrmode) {
		case 0:
			mapper_map_chr(8, 0, mmc5_chr(banks[bank][0x0]));
			break;

		case 1:
			mapper_map_chr(4, 0, mmc5_chr(banks[bank][0x1]));
			mapper_map_chr(4, 1, mmc5_chr(banks[bank][0x2]));
			break;

		case 2:
			mapper_map_chr(2, 0, mmc5_chr(banks[bank][0x3]));
			mapper_map_chr(2, 1, mmc5_chr(banks[bank][0x4]));
			mapper_map_chr(2, 2, mmc5_chr(banks[bank][0x5]));
			mapper_map_chr(2, 3, mmc5_chr(banks[bank][0x6]));
			break;

		case 3:
			mapper_map_chr(1, 0, mmc5_chr(banks[bank][0x7]));
			mapper_map_chr(1, 1, mmc5_chr(banks[bank][0x8]));
			mapper_map_chr(1, 2, mmc5_chr(banks[bank][0x9]));
			mapper_map_chr(1, 3, mmc5_chr(banks[bank][0xa]));
			mapper_map_chr(1, 4, mmc5_chr(banks[bank][0xb]));
			mapper_map_chr(1, 5, mmc5_chr(banks[bank][0xc]));
			mapper_map_chr(1, 6, mmc5_chr(banks[bank][0xd]));
			mapper_map_chr(1, 7, mmc5_chr(banks[bank][0xe]));
			break;
	}
}

static void mapper5_map()
{
	switch (mmc5_prgmode) {
		case 0:
			mapper_map_prg(32, 0, (mmc5_prg(3) & 0x7c) >> 2);
			break;

		case 1:
			mapper_map_prg(16, 0, (mmc5_prg(1) & 0x7e) >> 1, (mmc5_prg(1) & 0x80) ? MEM_ROM : MEM_RAM);
			mapper_map_prg(16, 1, (mmc5_prg(3) & 0x7e) >> 1);
			break;

		case 2:
			mapper_map_prg(16, 0, (mmc5_prg(1) & 0x7e) >> 1, (mmc5_prg(1) & 0x80) ? MEM_ROM : MEM_RAM);
			mapper_map_prg(8, 2, mmc5_prg(2) & 0x7f, (mmc5_prg(2) & 0x80) ? MEM_ROM : MEM_RAM);
			mapper_map_prg(8, 3, mmc5_prg(3) & 0x7f);
			break;

		case 3:
			mapper_map_prg(8, 0, mmc5_prg(0) & 0x7f, (mmc5_prg(0) & 0x80) ? MEM_ROM : MEM_RAM);
			mapper_map_prg(8, 1, mmc5_prg(1) & 0x7f, (mmc5_prg(1) & 0x80) ? MEM_ROM : MEM_RAM);
			mapper_map_prg(8, 2, mmc5_prg(2) & 0x7f, (mmc5_prg(2) & 0x80) ? MEM_ROM : MEM_RAM);
			mapper_map_prg(8, 3, mmc5_prg(3) & 0x7f);
			break;
	}

	mapper_map_exp_prg(mmc5_prgexp);

	// Note: mapper5_ppu_clk() takes care of chr banking
}

static UINT8 mapper5_read(UINT16 address)
{
	if (address >= 0x5000 && address <= 0x5015) {
		switch (address) {
			case 0x5010: {
				//bprintf(0, _T("mmc5 irq ack\n"));
				UINT8 ret = ((mmc5_pcmirq & 1) << 7) | (~mmc5_pcmwrmode & 1);
				mmc5_pcmirq &= ~1; // clear flag
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				return ret;
			}
			default:
				return nesapuRead(0, (address & 0x1f) | 0x80);
		}
	}

	if (address >= 0x5c00 && address <= 0x5fff) {
		return (mmc5_expram_mode & 2) ? mmc5_expram[address & 0x3ff] : cpu_open_bus;
	}

	switch (address) {
		case 0x5204: {
			UINT8 ret =	(mmc5_irqpend << 7) | (mmc5_inframe << 6) | (cpu_open_bus & 0x3f);
			mmc5_irqpend = 0;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return ret;
		}

		case 0x5205:
		case 0x5206: return ((mmc5_mult0 * mmc5_mult1) >> ((address & 0x2) << 2)) & 0xff;
	}

	return cpu_open_bus;
}


static void mapper5_exp_write(UINT16 address, UINT8 data) // 6000 - 7fff
{
	cart_exp_write_abort = 1; // == don't write to Cart.WorkRAM after exiting this callback
	if (!Cart.WorkRAMSize) return;
	Cart.WorkRAM[PRGExpMap + (address & 0x1fff)] = data;
}

static UINT8 mapper5_exp_read(UINT16 address)             // 6000 - 7fff
{
	if (!Cart.WorkRAMSize) return 0;
	return Cart.WorkRAM[PRGExpMap + (address & 0x1fff)];
}

static void mapper5_prg_write(UINT16 address, UINT8 data)
{
	if (mmc5_prgprot1 == 0x02 && mmc5_prgprot2 == 0x01) {
		mapper_prg_write(address, data);
	}
}

static void mapper5_write(UINT16 address, UINT8 data)
{
	if (address >= 0x5000 && address <= 0x5015) {
		// Audio section
		switch (address) {
			case 0x5010:
				mmc5_pcmwrmode = ~data & 1;
				mmc5_pcmirq = data & 0x80;
				break;
			case 0x5011:
				if (mmc5_pcmwrmode) {
					if (data == 0x00 && mmc5_pcmirq) {
						mapper_irq(0);
						mmc5_pcmirq |= 0x01;
					}
					mmc5_pcmdata = data;
				}
				break;
			default:
				nesapuWrite(0, (address & 0x1f) | 0x80, data);
				break;
		}
		return;
	}

	if (address >= 0x5c00 && address <= 0x5fff) {
		switch (mmc5_expram_mode) {
			case 0: // (3) Counterintuitively, writes in these modes (0, 1) are only allowed when the PPU is rendering. - https://www.nesdev.org/wiki/MMC5#Internal_extended_RAM_mode_($5104)
			case 1:
				if (!RENDERING) data = 0x00;
				// fallthrough..
			case 2:
				mmc5_expram[address & 0x3ff] = data;
				break;
		}
		return;
	}

	switch (address) {
		case 0x5100: mmc5_prgmode = data & 3; mapper_map(); break;
		case 0x5101: mmc5_chrmode = data & 3; break;
		case 0x5102: mmc5_prgprot1 = data; break;
		case 0x5103: mmc5_prgprot2 = data; break;
		case 0x5104: mmc5_expram_mode = data & 3; break;
		case 0x5105:
			mmc5_mirror(0) = (data >> 0) & 3;
			mmc5_mirror(1) = (data >> 2) & 3;
			mmc5_mirror(2) = (data >> 4) & 3;
			mmc5_mirror(3) = (data >> 6) & 3;
			break;

		case 0x5106: mmc5_filltile = data; break;
		case 0x5107: mmc5_fillcolor = ((data & 3) << 6) | ((data & 3) << 4) | ((data & 3) << 2) | (data & 3); break;

		case 0x5113: mmc5_prgexp = data & 7; mapper_map(); break;
		case 0x5114:
		case 0x5115:
		case 0x5116:
		case 0x5117: mmc5_prg(address & 3) = data; mapper_map(); break;

		case 0x5120:
		case 0x5121:
		case 0x5122:
		case 0x5123:
		case 0x5124:
		case 0x5125:
		case 0x5126:
		case 0x5127:
		case 0x5128:
		case 0x5129:
		case 0x512a:
		case 0x512b:
			mmc5_chr(address & 0xf) = (mmc5_upperchr << 8) | data;
			mmc5_lastchr = address & 0xf;
			if (mmc5_inframe == 0) {
				mmc5_mapchr(CHR_LASTREG);
			}
			break;

		case 0x5130: mmc5_upperchr = data & 3; break;

		case 0x5200:
			mmc5_splittile = data & 0x1f;
			mmc5_splitside = data & 0x40;
			mmc5_split = (data & 0x80) >> 7;
			break;
		case 0x5201:
			mmc5_splitscroll = data >> 3;
			mmc5_splitscrollmod = (mmc5_splitscroll < 30 ? 30 : 32);
			break;
		case 0x5202: mmc5_splitchr = data; break;

		case 0x5203:
			mmc5_irqcompare = data;
			break;
		case 0x5204:
			mmc5_irqenable = (data & 0x80) >> 7;
			M6502SetIRQLine(0, (mmc5_irqenable && mmc5_irqpend) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			break;

		case 0x5205: mmc5_mult0 = data; break;
		case 0x5206: mmc5_mult1 = data; break;
	}
}

static UINT8 mapper5_ntread(UINT16 address)
{
	if (mmc5_expram_mode == 1) {
		if ((address & 0x3c0) < 0x3c0) {
			UINT8 temp = mmc5_expram[(32 * ((address >> 5) & 0x1f)) + (address & 0x1f)];
			mmc5_expramattr = (temp & 0xc0) >> 6;
            mapper_map_chr(4, 0, (mmc5_upperchr << 6) | (temp & 0x3f));
            mapper_map_chr(4, 1, (mmc5_upperchr << 6) | (temp & 0x3f));
        } else {
			return (mmc5_expramattr << 6) | (mmc5_expramattr << 4) | (mmc5_expramattr << 2) | mmc5_expramattr;
        }
    }

	if (mmc5_split && mmc5_expram_mode < 2) {
		UINT8 cur_tile = ((pixel >> 3) + 2) % 32;

		if ((mmc5_splitside && cur_tile >= mmc5_splittile) || (!mmc5_splitside && cur_tile < mmc5_splittile)) {
			mapper_map_chr(4, 0, mmc5_splitchr);
			mapper_map_chr(4, 1, mmc5_splitchr);
			UINT8 y = ((scanline >> 3) + mmc5_splitscroll) % mmc5_splitscrollmod;
			address = (pixel & 2) ? ((y * 32) & 0x3e0) | cur_tile : 0x3c0 | ((y << 1) & 0x38) | (cur_tile >> 2);
			return mmc5_expram[address & 0x3ff];
		} else {
			mmc5_mapchr(CHR_GUESS);
		}
	}

	switch (mmc5_mirror((address & 0x1fff) >> 10)) {
		case 0: return mmc5_nt_ram[(address & 0x3ff)];
		case 1: return mmc5_nt_ram[(address & 0x3ff) + 0x400];
		case 2: return (mmc5_expram_mode < 2) ? mmc5_expram[address & 0x3ff] : 0;
		case 3: return ((address & 0x3c0) == 0x3c0) ? mmc5_fillcolor : mmc5_filltile;
	}

	return 0x00;
}

static void mapper5_ntwrite(UINT16 address, UINT8 data)
{
	switch (mmc5_mirror((address & 0x1fff) >> 10)) {
		case 0: mmc5_nt_ram[(address & 0x3ff)] = data; break;
		case 1: mmc5_nt_ram[(address & 0x3ff) + 0x400] = data; break;
		case 2: if (mmc5_expram_mode < 2) mmc5_expram[address & 0x3ff] = data; break;
		case 3: /* invalid */ break;
	}
}

static void mmc5_lineadvance()
{
	if (mmc5_inframe == 0) {
		mmc5_inframe = 1;
		mmc5_irqcount = 0;
		mmc5_irqpend = 0;
		M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
	} else if (mmc5_inframe) {
		mmc5_irqcount++;
		if (mmc5_irqcount == mmc5_irqcompare) {
			mmc5_irqpend = 1;
			if (mmc5_irqenable) {
				M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		}
	}
}

static void mapper5_ppu_clk(UINT16 address)
{
	if (scanline < 240 && RENDERING) {
		switch (pixel) {
			case   1: if (~NESMode & ALT_TIMING2) mmc5_lineadvance(); break; // castelvania iii
			case  16: if (NESMode & ALT_TIMING2) mmc5_lineadvance(); break; // laser invasion prefers cycle 16 (grr..)
			case 257: if (mmc5_inframe) mmc5_mapchr(CHR_SPRITE); break; // sprite bank switch
			case 321: if (mmc5_inframe) mmc5_mapchr(CHR_TILE); break; // bg bank switch
		}
	} else {
		if (mmc5_inframe) {
			// when coming out of frame (vblank, !RENDERING) map in the bank of the last register written
			mmc5_mapchr(CHR_LASTREG);
		}
		mmc5_inframe = 0;
	}
}

#undef mmc5_prgmode
#undef mmc5_chrmode
#undef mmc5_prgprot1
#undef mmc5_prgprot2
#undef mmc5_expram_mode
#undef mmc5_mirror
#undef mmc5_filltile
#undef mmc5_fillcolor
#undef mmc5_prgexp
#undef mmc5_prg
#undef mmc5_chr
#undef mmc5_upperchr
#undef mmc5_split
#undef mmc5_splitside
#undef mmc5_splittile
#undef mmc5_splitscroll
#undef mmc5_splitscrollmod
#undef mmc5_splitchr
#undef mmc5_irqenable
#undef mmc5_irqcompare
#undef mmc5_irqpend
#undef mmc5_mult0
#undef mmc5_mult1
#undef mmc5_irqcount
#undef mmc5_inframe
#undef mmc5_lastchr
#undef mmc5_expramattr

// ---[ mapper 07 (AxROM) Battle Toads, Marble Madness, RC Pro-Am
static void mapper07_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		mapper_regs[0] = data;
		mapper_map();
	}
}

static void mapper07_map()
{
	set_mirroring((mapper_regs[0] & 0x10) ? SINGLE_HIGH : SINGLE_LOW);
    mapper_map_prg(32, 0, mapper_regs[0] & 0x7);
}

// ---[ mapper 09 (mmc2) / 10 (mmc4) Mike Tyson's Punch-Out!!, Fire Emblem
#define mapper9_prg				(mapper_regs[0xf - 0])
#define mapper9_chr_lo(x)		(mapper_regs[0xf - 1 - x])
#define mapper9_chr_hi(x)		(mapper_regs[0xf - 3 - x])
#define mapper9_chr_lo_C000		(mapper_regs[0xf - 5])
#define mapper9_chr_hi_E000		(mapper_regs[0xf - 6])
#define mapper9_mirror			(mapper_regs[0xf - 7])
#define mapper9_update          (mapper_regs[0xf - 8])

static void mapper09_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		switch (address & 0xf000) {
			case 0xa000: mapper9_prg       = data & 0xf; break;
			case 0xb000: mapper9_chr_lo(0) = data & 0x1f; break;
			case 0xc000: mapper9_chr_lo(1) = data & 0x1f; break;
			case 0xd000: mapper9_chr_hi(0) = data & 0x1f; break;
			case 0xe000: mapper9_chr_hi(1) = data & 0x1f; break;
			case 0xf000: mapper9_mirror    = data & 0x1; break;
		}
		mapper_map();
	}
}

static void mapper09_map()
{
	set_mirroring((mapper9_mirror) ? HORIZONTAL : VERTICAL);
	mapper_map_prg( 8, 0, mapper9_prg);
	mapper_map_chr( 4, 0, mapper9_chr_lo(mapper9_chr_lo_C000));
	mapper_map_chr( 4, 1, mapper9_chr_hi(mapper9_chr_hi_E000));
}

static void mapper10_map()
{
	set_mirroring((mapper9_mirror) ? HORIZONTAL : VERTICAL);
	mapper_map_prg(16, 0, mapper9_prg);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 4, 0, mapper9_chr_lo(mapper9_chr_lo_C000));
	mapper_map_chr( 4, 1, mapper9_chr_hi(mapper9_chr_hi_E000));
}

static void mapper09_ppu_clk(UINT16 busaddr)
{
	switch (busaddr & 0x3fff) {
		case 0x0fd8:
			mapper9_chr_lo_C000 = 0;
			mapper9_update = 1;
			break;
		case 0x0fe8:
			mapper9_chr_lo_C000 = 1;
			mapper9_update = 1;
			break;
	}

	switch (busaddr & 0x3ff8) {
		case 0x1fd8:
			mapper9_chr_hi_E000 = 0;
			mapper9_update = 1;
			break;
		case 0x1fe8:
			mapper9_chr_hi_E000 = 1;
			mapper9_update = 1;
			break;
	}

	if (mapper9_update) {
		// mmc2 needs update immediately on latch
		mapper9_update = 0;
		mapper_map();
	}
}

static void mapper10_ppu_clk(UINT16 busaddr)
{
	if (mapper9_update) {
		// mmc4 needs delayed update.  right window borders break in fire emblem
		// without
		mapper9_update = 0;
		mapper_map();
	}

	switch (busaddr & 0x3ff8) {
		case 0x0fd8:
			mapper9_chr_lo_C000 = 0;
			mapper9_update = 1;
			break;
		case 0x0fe8:
			mapper9_chr_lo_C000 = 1;
			mapper9_update = 1;
			break;
		case 0x1fd8:
			mapper9_chr_hi_E000 = 0;
			mapper9_update = 1;
			break;
		case 0x1fe8:
			mapper9_chr_hi_E000 = 1;
			mapper9_update = 1;
			break;
	}
}

#undef mapper9_prg
#undef mapper9_chr_lo
#undef mapper9_chr_hi
#undef mapper9_chr_lo_C000
#undef mapper9_chr_hi_E000
#undef mapper9_mirror
#undef mapper9_update

// ---[ mapper 99 (VS NES)
static void mapper99_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;

	mapper_map();
}

static void mapper99_map()
{
	mapper_map_chr(8, 0, (mapper_regs[0] >> 2) & 1);
	mapper_map_prg(32, 0, 0);
	if (Cart.PRGRomSize > 0x8000) {
		mapper_map_prg(8, 0, mapper_regs[0] & 4); // gumshoe
	}
}

// ---[ mapper 13 (CPROM) Videomation
static void mapper13_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		mapper_regs[0] = data;
	}

	mapper_map();
}

static void mapper13_map()
{
	mapper_map_chr(4, 0, 0);
	mapper_map_chr(4, 1, mapper_regs[0] & 0x03);
}

// ---[ mapper 133 (Sachen ??) Jovial Race
static void mapper133_write(UINT16 address, UINT8 data)
{
	if ((address & 0x4100) == 0x4100) {
		mapper_regs[0] = data;
	}

	mapper_map();
}

static void mapper133_map()
{
	mapper_map_chr(8, 0, mapper_regs[0] & 0x03);
	mapper_map_prg(32, 0, (mapper_regs[0] >> 2) & 0x01);
}

// --[ mapper 16, 153, 159 - Bandai FCG
#define mapper16_mirror		(mapper_regs[0x1f - 0])
#define mapper16_irqenable	(mapper_regs[0x1f - 1])
#define mapper16_irqcount	(mapper_regs16[0x1f - 0])
#define mapper16_irqlatch	(mapper_regs16[0x1f - 1])

static void mapper16_write(UINT16 address, UINT8 data)
{
	switch (address & 0x000f) {
		case 0x00: case 0x01: case 0x02:
		case 0x03: case 0x04: case 0x05:
		case 0x06: case 0x07: case 0x08:
			mapper_regs[address & 0xf] = data;
			break; // mixed prg, chr

		case 0x09:
			mapper16_mirror = data & 0x3;
			break;

		case 0x0a:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			mapper16_irqenable = data & 1;
			mapper16_irqcount = mapper16_irqlatch;
			break;

		case 0x0b:
			mapper16_irqlatch = (mapper16_irqlatch & 0xff00) | data;
			break;

		case 0x0c:
			mapper16_irqlatch = (mapper16_irqlatch & 0x00ff) | (data << 8);
			break;

		case 0x0d: // x24 i2c write
			return; // don't bother mapper_map(); below..
			break;
	}

	mapper_map();
}

static void mapper16_map()
{
	mapper_map_prg(16, 0, mapper_regs[8]);
	mapper_map_prg(16, 1, -1);

	mapper_map_chr( 1, 0, mapper_regs[0]);
	mapper_map_chr( 1, 1, mapper_regs[1]);
	mapper_map_chr( 1, 2, mapper_regs[2]);
	mapper_map_chr( 1, 3, mapper_regs[3]);
	mapper_map_chr( 1, 4, mapper_regs[4]);
	mapper_map_chr( 1, 5, mapper_regs[5]);
	mapper_map_chr( 1, 6, mapper_regs[6]);
	mapper_map_chr( 1, 7, mapper_regs[7]);

	switch (mapper16_mirror) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}
}

static void mapper153_map()
{
	mapper_map_prg(16, 0, (mapper_regs[8] & 0xf) | ((mapper_regs[0] & 0x1) << 4));
	mapper_map_prg(16, 1, 0xf | ((mapper_regs[0] & 0x1) << 4));

	mapper_map_chr( 8, 0, 0);

	switch (mapper16_mirror) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}
}

static void mapper16_cycle()
{
	if (mapper16_irqenable) {
		mapper16_irqcount--;

		if (mapper16_irqcount == 0xffff) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			mapper16_irqenable = 0;
		}
	}
}
#undef mapper16_mirror
#undef mapper16_irqenable
#undef mapper16_irqcount

// --[ mapper 18 - Jaleco (Magical John, Pizza Pop etc)
#define mapper18_irqenable	(mapper_regs[0x1f - 0])
#define mapper18_mirror     (mapper_regs[0x1f - 1])
#define mapper18_prg(x)     (mapper_regs[0 + (x)])
#define mapper18_chr(x)     (mapper_regs[4 + (x)])
#define mapper18_irqcount	(mapper_regs16[0x1f - 0])
#define mapper18_irqlatch	(mapper_regs16[0x1f - 1])
#define mapper18_irqmask	(mapper_regs16[0x1f - 2])

static void nib2byte(UINT8 &byte, UINT8 nib, INT32 high)
{
	if (high == 0) {
		byte = (byte & 0xf0) | (nib & 0xf);
	} else {
		byte = (byte & 0x0f) | (nib & 0xf) << 4;
	}
}

static void mapper18_write(UINT16 address, UINT8 data)
{
	INT32 highbits = address & 1;

	switch (address & 0xf003) {
		case 0x8000: case 0x8001:
			nib2byte(mapper18_prg(0), data, highbits);
			break;
		case 0x8002: case 0x8003:
			nib2byte(mapper18_prg(1), data, highbits);
			break;
		case 0x9000: case 0x9001:
			nib2byte(mapper18_prg(2), data, highbits);
			break;

		case 0xa000: case 0xa001:
			nib2byte(mapper18_chr(0), data, highbits);
			break;
		case 0xa002: case 0xa003:
			nib2byte(mapper18_chr(1), data, highbits);
			break;

		case 0xb000: case 0xb001:
			nib2byte(mapper18_chr(2), data, highbits);
			break;
		case 0xb002: case 0xb003:
			nib2byte(mapper18_chr(3), data, highbits);
			break;

		case 0xc000: case 0xc001:
			nib2byte(mapper18_chr(4), data, highbits);
			break;
		case 0xc002: case 0xc003:
			nib2byte(mapper18_chr(5), data, highbits);
			break;

		case 0xd000: case 0xd001:
			nib2byte(mapper18_chr(6), data, highbits);
			break;
		case 0xd002: case 0xd003:
			nib2byte(mapper18_chr(7), data, highbits);
			break;

		case 0xe000: mapper18_irqlatch = (mapper18_irqlatch & 0xfff0) | (data & 0xf); break;
		case 0xe001: mapper18_irqlatch = (mapper18_irqlatch & 0xff0f) | ((data & 0xf) << 4); break;
		case 0xe002: mapper18_irqlatch = (mapper18_irqlatch & 0xf0ff) | ((data & 0xf) << 8); break;
		case 0xe003: mapper18_irqlatch = (mapper18_irqlatch & 0x0fff) | ((data & 0xf) << 12); break;
		case 0xf000:
			mapper18_irqcount = mapper18_irqlatch;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xf001: {
			const UINT16 masks[4] = { (1<<16) - 1, (1<<12) - 1, (1<<8) - 1, (1<<4) - 1 };
			UINT8 maskpos = 0;
			mapper18_irqenable = data & 1;
			switch (data & (2|4|8)) {
				case 8: maskpos = 3; break;
				case 4: maskpos = 2; break;
				case 2: maskpos = 1; break;
				default: maskpos = 0; break;
			}
			mapper18_irqmask = masks[maskpos];

			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		}
		case 0xf002: mapper18_mirror = data & 3; break;
	}

	mapper_map();
}

static void mapper18_map()
{
	mapper_map_prg( 8, 0, mapper18_prg(0));
	mapper_map_prg( 8, 1, mapper18_prg(1));
	mapper_map_prg( 8, 2, mapper18_prg(2));
	mapper_map_prg( 8, 3, -1);

	for (INT32 i = 0; i < 8; i++) {
		mapper_map_chr( 1, i, mapper18_chr(i));
	}

	switch (mapper18_mirror) {
		case 0: set_mirroring(HORIZONTAL); break;
		case 1: set_mirroring(VERTICAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}
}

static void mapper18_cycle()
{
	if (mapper18_irqenable) {
		UINT16 count = mapper18_irqcount & mapper18_irqmask;
		count--;
		if (count == 0) {
			mapper_irq(2); // 2 cyc delay fixes jeebies in pizza-pop (mid-hud) and magic john (intro)
		}
		mapper18_irqcount = (mapper18_irqcount & ~mapper18_irqmask) | (count & mapper18_irqmask);
	}
}
#undef mapper18_irqenable
#undef mapper18_mirror
#undef mapper18_prg
#undef mapper18_chr
#undef mapper18_irqcount
#undef mapper18_irqlatch
#undef mapper18_irqmask

// ---[ mapper 19 Namco 163 + WSG (wave sound gen)
#define mapper19_irqcount		(mapper_regs16[0x1f - 0])
#define mapper19_irqenable  	(mapper_regs[0x1f - 0])
#define mapper19_soundaddr		(mapper_regs[0x1f - 1])
#define mapper19_soundaddrinc	(mapper_regs[0x1f - 2])
#define mapper19_soundenable	(mapper_regs[0x1f - 3])
#define mapper19_chrram_lo		(mapper_regs[0x1f - 4])
#define mapper19_chrram_mid		(mapper_regs[0x1f - 5])
#define mapper19_prg(x)     	(mapper_regs[0 + (x)]) // 0 - 2
#define mapper19_mapper210      (mapper_regs[0x1f - 6])
#define namco340_mirror			(mapper_regs[0x1f - 7])

// since chr mapping is a bit "advanced", we use a little struct
// instead of the usual mapper registers. mapper19_scan() takes care
// of the state-stuff (as well as the WSG channel regs, etc)

struct namco163_chrstuff {
	UINT8 ram;
	UINT8 data;
};

static namco163_chrstuff n163_chr[12]; // chr mapping: 8 chr banks, 4 nt banks

static UINT8 mapper19_soundram[0x80];

struct namco163_channel {
	UINT32 enabled;
	UINT32 freq;
	UINT32 phase;
	UINT32 vol;
	UINT32 len;
	UINT32 address;
	UINT32 accum;
};

static namco163_channel n163_ch[8];
static INT32 n163_channels = 0;

static void namco163_process_channel(INT16 address, UINT8 data) {
	namco163_channel *c = &n163_ch[(address >> 3) & 7];

	switch (address & 7) {
		case 0x1:
			c->phase = (c->phase & ~0x0000ff) | data;
			break;
		case 0x3:
			c->phase = (c->phase & ~0x00ff00) | (data << 8);
			break;
		case 0x5:
			c->phase = (c->phase & ~0xff0000) | (data << 16);
			break;
		case 0x0:
			c->freq = (c->freq & ~0x0000ff) | data;
			break;
		case 0x2:
			c->freq = (c->freq & ~0x00ff00) | (data << 8);
			break;
		case 0x4:
			c->freq = (c->freq & ~0xff0000) | ((data & 3) << 16);
			c->len = 0x100 - (data & 0xfc);
			c->enabled = data >> 5;
#if 0
			bprintf(0, _T("ch %d enabled%X?\n"), (address / 8) & 7, c->enabled);
			bprintf(0, _T("len: %X\n"), c->len);
			bprintf(0, _T("phase: %X\n"), c->phase);
#endif
			break;
		case 0x6:
			c->address = data;
			break;
		case 0x7:
			c->vol = (data & 0xf) * 8;
			if (address == 0x7f) {
				n163_channels = (data >> 4) & 7;
			}
#if 0
			bprintf(0, _T("ch %d vol %X data %x.     channels %d    (cycle: %d)\n"), (address / 8) & 7, c->vol, data, n163_channels, M6502TotalCycles());
#endif
			break;
	}
}

static void namco163_update_phase(namco163_channel *c, INT32 ch)
{
	ch = 0x40 + ch * 8;
	mapper19_soundram[ch + 5] = (c->phase >> 16) & 0xff;
	mapper19_soundram[ch + 3] = (c->phase >>  8) & 0xff;
	mapper19_soundram[ch + 1] = (c->phase >>  0) & 0xff;
}

static UINT32 namco163_fetch_sample(namco163_channel *c)
{
	UINT32 wave_addr = c->address + (c->phase >> 16);
	UINT32 wave_sam = mapper19_soundram[(wave_addr & 0xff) >> 1];
	wave_sam >>= (wave_addr & 1) << 2; // LE-ordered nibble

	return ((wave_sam & 0xf) - 8) * c->vol;
}

static INT16 namco163_wavesynth()
{
	if (!mapper19_soundenable) return 0;

	INT32 sample = 0;

	for (INT32 ch = 7; ch >= (7 - n163_channels); ch--) {
		namco163_channel *c = &n163_ch[ch];

		if (c->enabled && c->vol) {
			sample += namco163_fetch_sample(c);

			if (c->accum == 0) {
				c->accum = (n163_channels + 1) * 0xf;
				c->phase = (c->phase + c->freq) % (c->len << 16);
				namco163_update_phase(c, ch);
			}

			c->accum--;
		}
	}

	return sample;
}

static void mapper19_reset()
{
	memset(&n163_chr, 0, sizeof(n163_chr));
	memset(&n163_ch, 0, sizeof(n163_ch));
	memset(&mapper19_soundram, 0, sizeof(mapper19_soundram));
}

static void mapper19_scan()
{
	SCAN_VAR(n163_chr); // chr/nt mapping regs

	SCAN_VAR(mapper19_soundram); // WSG soundram

	for (INT32 i = 0x40; i < 0x80; i++) { // rebuild n163_ch from soundram
		namco163_process_channel(i, mapper19_soundram[i]);
	}
}

static INT16 mapper19_mixer()
{
	const INT32 sum = namco163_wavesynth();
	return (INT16)((sum * 0x1c00) >> 12); //(INT16)(sum * 1.75);
}

static void mapper19_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf800) {
		case 0x4800:
			if (mapper19_soundaddr >= 0x40) {
				namco163_process_channel(mapper19_soundaddr, data);
			}
			mapper19_soundram[mapper19_soundaddr] = data;
			mapper19_soundaddr = (mapper19_soundaddr + mapper19_soundaddrinc) & 0x7f;
			return; // [sound core] avoid calling mapper_map() below
		case 0x5000:
			mapper19_irqcount = (mapper19_irqcount & 0xff00) | data;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return;
		case 0x5800:
			mapper19_irqcount = (mapper19_irqcount & 0x00ff) | ((data & 0x7f) << 8);
			mapper19_irqenable = data & 0x80;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return;
		case 0xe000:
			mapper19_prg(0) = data & 0x3f;
			mapper19_soundenable = !(data & 0x40);
			namco340_mirror = (data & 0xc0) >> 6;
			break;
		case 0xe800:
			mapper19_prg(1) = data & 0x3f;
			mapper19_chrram_lo = !(data & 0x40);
			mapper19_chrram_mid = !(data & 0x80);
			break;
		case 0xf000:
			mapper19_prg(2) = data & 0x3f;
			break;
		case 0xf800:
			mapper19_soundaddr = data & 0x7f;
			mapper19_soundaddrinc = (data >> 7) & 1;
			return; // [sound core] avoid calling mapper_map() below

		case 0x8000: case 0x8800:
		case 0x9000: case 0x9800: {
			INT32 bank = (address - 0x8000) >> 11;
			n163_chr[bank].ram = mapper19_chrram_lo;
			n163_chr[bank].data = data;
			break;
		}
		case 0xa000: case 0xa800:
		case 0xb000: case 0xb800: {
			INT32 bank = (address - 0x8000) >> 11;
			n163_chr[bank].ram = mapper19_chrram_mid;
			n163_chr[bank].data = data;
			break;
		}
		case 0xc000: case 0xc800:
		case 0xd000: case 0xd800: {
			INT32 nt = (address - 0xc000) >> 11;
			n163_chr[8 + nt].data = data;
			break;
		}
	}

	mapper_map();
}

static UINT8 mapper19_read(UINT16 address)
{
	UINT8 ret = 0;

	switch (address & 0xf800) {
		case 0x4800:
			ret = mapper19_soundram[mapper19_soundaddr];
			mapper19_soundaddr = (mapper19_soundaddr + mapper19_soundaddrinc) & 0x7f;
			break;

		case 0x5000:
			ret = mapper19_irqcount & 0xff;
			break;

		case 0x5800:
			ret = (mapper19_irqcount >> 8) & 0xff;
			break;
	}

	return ret;
}

static void mapper19_map()
{
	mapper_map_prg(8, 0, mapper19_prg(0));
	mapper_map_prg(8, 1, mapper19_prg(1));
	mapper_map_prg(8, 2, mapper19_prg(2));
	mapper_map_prg(8, 3, -1);

	for (INT32 i = 0; i < 8; i++) {
		if (mapper19_mapper210 == 0 && n163_chr[i].ram && n163_chr[i].data >= 0xe0) {
			mapper_set_chrtype(MEM_RAM);
			mapper_map_chr(1, i, n163_chr[i].data & 0x1f);
		} else {
			mapper_set_chrtype(MEM_ROM);
			mapper_map_chr(1, i, n163_chr[i].data);
		}
	}

	switch (mapper19_mapper210) {
		case 0:	// mapper 19 (namco163) only!
			for (INT32 i = 8; i < 12; i++) {
				if (n163_chr[i].data >= 0xe0) {
					nametable_map(i & 3, n163_chr[i].data & 1);
				} else {
					nametable_mapraw(i & 3, Cart.CHRRom + (n163_chr[i].data << 10), MEM_ROM);
				}
			}
			break;
		case 1: // mapper 210 submapper 1 (namco 175): hardwired h/v mirroring
			break;
		case 2: // mapper 210 submapper 2 (namco 340): selectable
			switch (namco340_mirror) {
				case 0: set_mirroring(SINGLE_LOW); break;
				case 1: set_mirroring(VERTICAL); break;
				case 2: set_mirroring(HORIZONTAL); break;
				case 3: set_mirroring(SINGLE_HIGH); break;
			}
			break;
	}
}

static void mapper19_cycle()
{
	if (mapper19_irqenable) {
		mapper19_irqcount++;

		if (mapper19_irqcount == 0x7fff) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			mapper19_irqenable = 0;
		}
	}
}
#undef mapper19_irqcount
#undef mapper19_irqenable
#undef mapper19_soundaddr
#undef mapper19_soundaddrinc
#undef mapper19_soundenable
#undef mapper19_chrram_lo
#undef mapper19_chrram_mid
#undef mapper19_prg
//#undef mapper19_mapper210 -- used in mapper_init!
#undef namco340_mirror

// --[ mapper 21, 22, 23, 25 - Konami VRC2/VRC4
#define mapper23_prg(x)		(mapper_regs[0x00 + x])
#define mapper23_chr(x)		(mapper_regs[0x02 + x])
#define mapper23_chrhigh(x) (mapper_regs[0x0a + x])
#define mapper23_prgswap	(mapper_regs[0x12])
#define mapper23_irqrepeat	(mapper_regs[0x13])
#define mapper23_mirror		(mapper_regs[0x14])
#define vrc2and4_ines22		(mapper_regs[0x17])

#define mapper23_irqenable	(mapper_regs[0x18])
#define mapper23_irqlatch	(mapper_regs[0x19])
#define mapper23_irqmode	(mapper_regs[0x1a])
#define mapper23_irqcycle	(mapper_regs16[0x1f - 0])
#define mapper23_irqcount	(mapper_regs16[0x1f - 1])

static void vrc2vrc4_map()
{
	if (mapper23_prgswap & 2) {
		mapper_map_prg( 8, 0, -2);
		mapper_map_prg( 8, 1, mapper23_prg(1));
		mapper_map_prg( 8, 2, mapper23_prg(0));
		mapper_map_prg( 8, 3, -1);
	} else {
		mapper_map_prg( 8, 0, mapper23_prg(0));
		mapper_map_prg( 8, 1, mapper23_prg(1));
		mapper_map_prg( 8, 2, -2);
		mapper_map_prg( 8, 3, -1);
	}

	for (INT32 i = 0; i < 8; i++) {
		mapper_map_chr( 1, i, ((mapper23_chrhigh(i) << 4) | mapper23_chr(i)) >> vrc2and4_ines22);
	}

	switch (mapper23_mirror) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}
}

static void vrc2vrc4_write(UINT16 address, UINT8 data)
{
	address &= 0xf003;
	if (address >= 0xb000 && address <= 0xe003) {
		UINT8 reg = ((address >> 1) & 1) | ((address - 0xb000) >> 11);
		if (address & 1) { // high 5-bits
			mapper23_chrhigh(reg) = data & 0x1f;
		} else { // low 4-bits
			mapper23_chr(reg) = data & 0xf;
		}
		mapper_map();
	} else {
		switch (address & 0xf003) {
			case 0x8000:
			case 0x8001:
			case 0x8002:
			case 0x8003:
				mapper23_prg(0) = data; // usually a 0x1f mask, some pirate carts/hacks want this unmasked to address larger prg than usual
				mapper_map();
				break;
			case 0xA000:
			case 0xA001:
			case 0xA002:
			case 0xA003:
				mapper23_prg(1) = data; // comment: same as above
				mapper_map();
				break;
			case 0x9000:
			case 0x9001:
				if (data != 0xff) { // Wai Wai World, what are you doing?
					mapper23_mirror = data & 3;
				}
				mapper_map();
				break;
			case 0x9002:
			case 0x9003:
				mapper23_prgswap = data;
				mapper_map();
				break;
			case 0xf000:
				mapper23_irqlatch = (mapper23_irqlatch & 0xf0) | (data & 0xf);
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				break;
			case 0xf001:
				mapper23_irqlatch = (mapper23_irqlatch & 0x0f) | ((data & 0xf) << 4);
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				break;
			case 0xf002:
				mapper23_irqrepeat = data & 1;
				mapper23_irqenable = data & 2;
				mapper23_irqmode = data & 4; // 1 cycle, 0 scanline
				if (data & 2) {
					mapper23_irqcycle = 0;
					mapper23_irqcount = mapper23_irqlatch;
				}
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				break;
			case 0xf003:
				mapper23_irqenable = mapper23_irqrepeat;
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				break;
		}
	}
}

static void vrc2vrc4_cycle()
{
	if (mapper23_irqenable) {
		if (mapper23_irqmode) { // cycle mode
			mapper23_irqcount++;
			if (mapper23_irqcount >= 0x100) {
				mapper_irq(5);
				mapper23_irqcount = mapper23_irqlatch;
			}
		} else {
			mapper23_irqcycle += 3; // scanline mode
			if (mapper23_irqcycle >= 341) {
				mapper23_irqcycle -= 341;
				mapper23_irqcount++;
				if (mapper23_irqcount == 0x100) {
					mapper_irq(5);
					mapper23_irqcount = mapper23_irqlatch;
				}
			}
		}
	}
}

static void mapper21_write(UINT16 address, UINT8 data)
{
	address = (address & 0xf000) | ((address >> 1) & 0x3);

	vrc2vrc4_write(address, data);
}

static void mapper22_write(UINT16 address, UINT8 data)
{
	address |= ((address >> 2) & 0x3);

	vrc2vrc4_write((address & 0xf000) | ((address >> 1) & 1) | ((address << 1) & 2), data);
}

static void mapper23_write(UINT16 address, UINT8 data)
{
	address |= ((address >> 2) & 0x3) | ((address >> 4) & 0x3) | ((address >> 6) & 0x3);

	vrc2vrc4_write(address, data);
}

#define mapper25_write		mapper22_write   // same address line hookup/swapping

// --[ mapper 24, 26 (Konami VRC6)
#define mapper24_prg(x)		(mapper_regs[0x00 + x])
#define mapper24_chr(x)		(mapper_regs[0x02 + x])
#define mapper24_mirror		(mapper_regs[0x10])
#define mapper24_irqenable	(mapper_regs[0x11])
#define mapper24_irqrepeat	(mapper_regs[0x12])
#define mapper24_irqlatch	(mapper_regs[0x13])
#define mapper24_irqmode	(mapper_regs[0x14])
#define mapper24_irqcount	(mapper_regs16[0x1f - 0])
#define mapper24_irqcycle	(mapper_regs16[0x1f - 1])
#define mapper26			(mapper_regs[0x15])

struct vrc6_channel {
	INT32 dutyacc;
	INT32 phaseacc;
	INT32 volacc;
	INT16 sample;
	UINT8 regs[4];
};

struct vrc6_master {
	UINT8 reg;
	INT32 disable;
	INT32 octave_shift;
};

static vrc6_channel vrc6_ch[3];
static vrc6_master vrc6_cntrl;

static void vrc6_reset()
{
	memset(&vrc6_ch, 0, sizeof(vrc6_ch));
	memset(&vrc6_cntrl, 0, sizeof(vrc6_cntrl));
}

static void vrc6_scan()
{
	SCAN_VAR(vrc6_ch);
	SCAN_VAR(vrc6_cntrl);
}

static INT16 vrc6_pulse(INT32 num)
{
	struct vrc6_channel *ch = &vrc6_ch[num];

	if (ch->regs[2] & 0x80 && vrc6_cntrl.disable == 0) { // channel enabled?
		ch->sample = 0;
		if (ch->regs[0] & 0x80) { // ignore duty
			ch->sample = (ch->regs[0] & 0xf) << 8;
		} else {
			if (ch->dutyacc > ((ch->regs[0] >> 4) & 7))
				ch->sample = (ch->regs[0] & 0xf) << 8;
			ch->phaseacc--;
			if (ch->phaseacc <= 0) {
				ch->phaseacc = ((((ch->regs[2] & 0xf) << 8) | ch->regs[1]) + 1) >> vrc6_cntrl.octave_shift;
				ch->dutyacc = (ch->dutyacc + 1) & 0xf;
			}
		}
	}
	return ch->sample;
}

static INT16 vrc6_saw()
{
	struct vrc6_channel *ch = &vrc6_ch[2];

	if (ch->regs[2] & 0x80 && vrc6_cntrl.disable == 0) { // channel enabled?
		ch->sample = ((ch->volacc >> 3) & 0x1f) << 8;
		ch->phaseacc--;
		if (ch->phaseacc <= 0) {
			ch->phaseacc = ((((ch->regs[2] & 0xf) << 8) | ch->regs[1]) + 1)	>> vrc6_cntrl.octave_shift;
			ch->volacc += ch->regs[0] & 0x3f;
			ch->dutyacc++;
			if (ch->dutyacc == 14) {
				ch->dutyacc = 0;
				ch->volacc = 0;
			}
		}
	}
	return ch->sample;
}

static INT16 vrc6_mixer()
{
	INT32 sum = vrc6_saw();
    sum += vrc6_pulse(0);
    sum += vrc6_pulse(1);
	return (INT16)((sum * 0xc00) >> 12); //(INT16)(sum * 0.75); avoid flop in such a heated place.
}

static void vrc6_sound_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x9003:                           // master control reg
			vrc6_cntrl.reg = data;
			vrc6_cntrl.disable = data & 1;
			vrc6_cntrl.octave_shift = (data & 4) << 1;
			if (vrc6_cntrl.octave_shift == 0)
				vrc6_cntrl.octave_shift = (data & 2) << 1;
			break;
		case 0x9000: case 0x9001: case 0x9002: // pulse
		case 0xa000: case 0xa001: case 0xa002: // pulse 2
		case 0xb000: case 0xb001: case 0xb002: // saw
			vrc6_ch[((address >> 12) - 9) & 3].regs[address & 3] = data;
			break;
	}
}

static void vrc6_map_prg(UINT32 map)
{
	if (map & (1 << 0)) mapper_map_prg(16, 0, mapper24_prg(0));
	if (map & (1 << 1)) mapper_map_prg( 8, 2, mapper24_prg(1));
	if (map & (1 << 2)) mapper_map_prg( 8, 3, -1);
}

static void vrc6_map_chr(UINT32 map)
{
	for (INT32 i = 0; i < 8; i++)
		if (map & (1<<i)) mapper_map_chr( 1, i, mapper24_chr(i));

	switch (mapper24_mirror) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}
}

static void vrc6_map()
{
	vrc6_map_prg(~0);
	vrc6_map_chr(~0);
}

static void vrc6_write(UINT16 address, UINT8 data)
{
	if (mapper26) {
		// VRC6b has bits 0 & 1 swapped
		address = (address & 0xfffc) | ((address >> 1) & 1) | ((address << 1) & 2);
	}

	if (address >= 0x9000 && address <= 0xb002) {
		vrc6_sound_write(address & 0xf003, data);
		return;
	}

	switch (address & 0xf003) {
		case 0x8000: mapper24_prg(0) = data; vrc6_map_prg(1 << 0); break;
		case 0xb003: mapper24_mirror = (data & 0xc) >> 2; vrc6_map_chr(0); break;
		case 0xc000: mapper24_prg(1) = data; vrc6_map_prg(1 << 1); break;
		case 0xd000: mapper24_chr(0) = data; vrc6_map_chr(1 << 0); break;
		case 0xd001: mapper24_chr(1) = data; vrc6_map_chr(1 << 1); break;
		case 0xd002: mapper24_chr(2) = data; vrc6_map_chr(1 << 2); break;
		case 0xd003: mapper24_chr(3) = data; vrc6_map_chr(1 << 3); break;
		case 0xe000: mapper24_chr(4) = data; vrc6_map_chr(1 << 4); break;
		case 0xe001: mapper24_chr(5) = data; vrc6_map_chr(1 << 5); break;
		case 0xe002: mapper24_chr(6) = data; vrc6_map_chr(1 << 6); break;
		case 0xe003: mapper24_chr(7) = data; vrc6_map_chr(1 << 7); break;
		case 0xf000:
			mapper24_irqlatch = data;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xf001:
			mapper24_irqrepeat = data & 1;
			mapper24_irqenable = data & 2;
			mapper24_irqmode = data & 4;
			if (mapper24_irqenable) {
				mapper24_irqcycle = 0;
				mapper24_irqcount = mapper24_irqlatch;
			}
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xf002:
			mapper24_irqenable = mapper24_irqrepeat;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
	}

	//if (address >= 0x8000 && address < 0xf000) mapper_map();
}

static void vrc6_cycle()
{
	if (mapper24_irqenable) {
		if (mapper24_irqmode) { // cycle mode
			mapper24_irqcount++;
			if (mapper24_irqcount >= 0x100) {
				mapper_irq(0);
				mapper24_irqcount = mapper24_irqlatch;
			}
		} else {
			mapper24_irqcycle += 3; // scanline mode
			if (mapper24_irqcycle >= 341) {
				mapper24_irqcycle -= 341;
				mapper24_irqcount++;
				if (mapper24_irqcount == 0x100) {
					mapper_irq(0);
					mapper24_irqcount = mapper24_irqlatch;
				}
			}
		}
	}
}
#undef mapper24_prg
#undef mapper24_chr
#undef mapper24_mirror
#undef mapper24_irqenable
#undef mapper24_irqrepeat
#undef mapper24_irqlatch
#undef mapper24_irqmode
#undef mapper24_irqcount
#undef mapper24_irqcycle

// ---[ mapper 227 xxx-in-1, Waixing Bio Hazard
#define mapper227_mirror	(mapper_regs[0])
#define mapper227_S         (mapper_regs[1])
#define mapper227_L         (mapper_regs[2])
#define mapper227_P         (mapper_regs[3])
#define mapper227_O         (mapper_regs[4])

static void mapper227_write(UINT16 address, UINT8 data)
{
	mapper227_P = ((address >> 2) & 0x1f) | ((address & 0x100) >> 3);
	mapper227_S = address & 0x01;
	mapper227_L = address & (1 << 9);
	mapper227_O = address & (1 << 7);

	mapper227_mirror = address & 0x02;

	mapper_map();
}

static void mapper227_map()
{
	if (mapper227_O) {
		if (mapper227_S) {
			mapper_map_prg(32, 0, mapper227_P >> 1);
		} else {
			mapper_map_prg(16, 0, mapper227_P);
			mapper_map_prg(16, 1, mapper227_P);
		}
	} else {
		if (mapper227_S) {
			if (mapper227_L) {
				mapper_map_prg(16, 0, mapper227_P & 0x3e);
				mapper_map_prg(16, 1, mapper227_P | 0x07);
			} else {
				mapper_map_prg(16, 0, mapper227_P & 0x3e);
				mapper_map_prg(16, 1, mapper227_P & 0x38);
			}
		} else {
			if (mapper227_L) {
				mapper_map_prg(16, 0, mapper227_P);
				mapper_map_prg(16, 1, mapper227_P | 0x07);
			} else {
				mapper_map_prg(16, 0, mapper227_P);
				mapper_map_prg(16, 1, mapper227_P & 0x38);
			}
		}
	}

	mapper_map_chr( 8, 0, 0);

	set_mirroring((mapper227_mirror & 0x02) ? HORIZONTAL : VERTICAL);
}

// ---[ mapper 172: 1991 Du Ma Racing
#define jv001_register			(mapper_regs[0x1f - 0])
#define jv001_invert			(mapper_regs[0x1f - 1])
#define jv001_mode				(mapper_regs[0x1f - 2])
#define jv001_input				(mapper_regs[0x1f - 3])
#define jv001_output			(mapper_regs[0x1f - 4])

#define jv001_d0d3_mask		(0x0f)
#define jv001_d4d5_mask		(0x30)

static UINT8 jv001_read()
{
	UINT8 ret;
	ret =  (jv001_register & jv001_d0d3_mask);
	// if (invert), bits d4 and d5 are inverted
	ret |= (jv001_register & jv001_d4d5_mask) ^ (jv001_invert * jv001_d4d5_mask);

	//bprintf(0, _T("jv001_read:  %x\n"), ret);

	return ret;
}

static void jv001_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		//bprintf(0, _T("jv001_latch address (%x) - output: %x\n"), address, jv001_register);
		jv001_output = jv001_register;
	} else {
		switch (address & 0xe103) {
			case 0x4100:
				if (jv001_mode) {
					// increment d0-d3, leaving d4-d5 unchanged
					//UINT8 before = jv001_register;
					jv001_register = ((jv001_register + 1) & jv001_d0d3_mask) | (jv001_register & jv001_d4d5_mask);
					//bprintf(0, _T("jv001_inc: mode %x  before  %x  after  %x\n"), jv001_mode, before, jv001_register);
				} else {
					// load register.  if inverted invert d0-d3, leaving d4-d5 unchanged
					//UINT8 before = jv001_register;
					jv001_register = (jv001_invert) ? ((~jv001_input & jv001_d0d3_mask) | (jv001_input & jv001_d4d5_mask)) : jv001_input;
					//bprintf(0, _T("jv001_load(inc): mode %x  before  %x  after  %x   input  %x\n"), jv001_mode, before, jv001_register, jv001_input);
				}
				break;
			case 0x4101:
				//bprintf(0, _T("invert  %x\n"), data);
				jv001_invert = (data & 0x10) >> 4;
				break;
			case 0x4102:
				//bprintf(0, _T("input  %x\n"), data);
				jv001_input = data;
				break;
			case 0x4103:
				//bprintf(0, _T("mode  %x\n"), data);
				jv001_mode = (data & 0x10) >> 4;
				break;
		}
	}
}

// mapper 172: jv001 chip is mounted upside-down thus flipping d0 - d5
static UINT8 mapper172_jv001_swap(UINT8 data)
{
	return ((data & 0x01) << 5) | ((data & 0x02) << 3) | ((data & 0x04) << 1) | ((data & 0x08) >> 1) | ((data & 0x10) >> 3) | ((data & 0x20) >> 5);
}

static UINT8 mapper172_read(UINT16 address)
{
	if ((address & 0xe100) == 0x4100) {
		return mapper172_jv001_swap(jv001_read()) | (cpu_open_bus & 0xc0);
	}

	return cpu_open_bus;
}

static void mapper172_write(UINT16 address, UINT8 data)
{
	jv001_write(address, mapper172_jv001_swap(data));

	if (address & 0x8000) mapper_map();
}

static void mapper172_map()
{
	mapper_map_prg(32, 0, 0);
	mapper_map_chr( 8, 0, jv001_output & 0x03);

	set_mirroring((jv001_invert) ? VERTICAL : HORIZONTAL);
}


// --[ mapper 228: Action52
#define mapper228_mirror	(mapper_regs[0x1f - 0])
#define mapper228_prg		(mapper_regs[0x1f - 1])
#define mapper228_prgtype	(mapper_regs[0x1f - 2])
#define mapper228_chr		(mapper_regs[0x1f - 3])
#define mapper228_weird(x)	(mapper_regs[0 + (x)])

static void mapper228_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		UINT8 csel = (address >> 11) & 0x03;
		if (csel == 3) csel = 2;

		mapper228_prg = ((address >> 6) & 0x1f) | (csel << 5);
		mapper228_prgtype = address & 0x20;
		mapper228_chr = ((address & 0x0f) << 2) | (data & 0x03);
		mapper228_mirror = (~address & 0x2000) >> 13;

		mapper_map();
	}
}

static void mapper228_psg_write(UINT16 address, UINT8 data)
{
	if (address >= 0x5ff0) {
		mapper228_weird(address & 3) = data;
	}
}

static UINT8 mapper228_psg_read(UINT16 address)
{
	if (address >= 0x5ff0) {
		return mapper228_weird(address & 3);
	}

	return cpu_open_bus;
}

static void mapper228_map()
{
	if (mapper228_prgtype) {
		mapper_map_prg(16, 0, mapper228_prg);
		mapper_map_prg(16, 1, mapper228_prg);
	} else {
		mapper_map_prg(16, 0, (mapper228_prg & ~1));
		mapper_map_prg(16, 1, mapper228_prg | 1);
	}

	mapper_map_chr( 8, 0, mapper228_chr);
	set_mirroring((mapper228_mirror) ? VERTICAL : HORIZONTAL);
}
#undef mapper228_mirror
#undef mapper228_prg
#undef mapper228_prgtype
#undef mapper228_chr

// --[ mapper 90: jy company
#define mapper90_209                (mapper_regs[0x1f - 0x0])
#define mapper90_211                (mapper_regs[0x1f - 0x1])

// 5800&3: multiplier / accumulator
#define mapper90_mul0   			(mapper_regs[0x1f - 0x2])
#define mapper90_mul1   			(mapper_regs[0x1f - 0x3])
#define mapper90_accu   			(mapper_regs[0x1f - 0x4])
#define mapper90_testreg			(mapper_regs[0x1f - 0x5])

// c000 - cfff&7: IRQ
#define mapper90_irqenable			(mapper_regs[0x1f - 0x6])
#define mapper90_irqmode			(mapper_regs[0x1f - 0x7])
#define mapper90_irqcounter  		(mapper_regs[0x1f - 0x8])
#define mapper90_irqprescaler	  	(mapper_regs[0x1f - 0x9])
#define mapper90_irqxor  			(mapper_regs[0x1f - 0xa])
#define mapper90_irqprescalermask	(mapper_regs[0x1f - 0xb])
#define mapper90_irqunknown  		(mapper_regs[0x1f - 0xc])

// d000 - d7ff&3: mode
#define mapper90_mode				(mapper_regs[0x1f - 0xd])
#define mapper90_mirror				(mapper_regs[0x1f - 0xe])
#define mapper90_ppu				(mapper_regs[0x1f - 0xf])
#define mapper90_obank				(mapper_regs[0x1f - 0x10])

// 8000 - 87ff&3: PRG
#define mapper90_prg(x)				(mapper_regs[0x00 + (x)])

// 9000 - 97ff&7: CHRlo
#define mapper90_chrlo(x)			(mapper_regs[0x04 + (x)])

// a000 - a7ff&7: CHRhi (actually 8bit, ran out of 8bit regs)
#define mapper90_chrhi(x)			(mapper_regs16[0x00 + (x)])

// mmc4-like 4k chr banking latch
#define mapper90_chrlatch(x)        (mapper_regs16[0x08 + (x)])

// b000 - b7ff&3: nametable config (&4 = MSB)
#define mapper90_nt(x)              (mapper_regs16[0x0a + (x)])

// forward --
static void mapper90_ppu_clockmmc4(UINT16 address);

static void mapper90_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8000 && address <= 0x87ff) {
		mapper90_prg(address & 3) = data & 0x3f;
	}

	if (address >= 0x9000 && address <= 0x97ff) {
		mapper90_chrlo(address & 7) = data;
	}

	if (address >= 0xa000 && address <= 0xa7ff) {
		mapper90_chrhi(address & 7) = data;
	}

	if (address >= 0xb000 && address <= 0xb7ff) {
		if (~address & 4) { // LSB
			mapper90_nt(address & 3) = (mapper90_nt(address & 3) & 0xff00) | (data << 0);
		} else { // MSB
			mapper90_nt(address & 3) = (mapper90_nt(address & 3) & 0x00ff) | (data << 8);
		}
	}

	if (address >= 0xc000 && address <= 0xcfff) {
		switch (address & 0x7) {
			case 0:
				mapper90_irqenable = data & 1;
				if (!mapper90_irqenable) {
					M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				}
				break;
			case 1:
				mapper90_irqmode = data;
				mapper90_irqprescalermask = (data & 4) ? 0x7 : 0xff;
				break;
			case 2:
				mapper90_irqenable = 0;
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				break;
			case 3:
				mapper90_irqenable = 1;
				break;
			case 4:
				mapper90_irqprescaler = data ^ mapper90_irqxor;
				break;
			case 5:
				mapper90_irqcounter = data ^ mapper90_irqxor;
				break;
			case 6:
				mapper90_irqxor = data;
				break;
			case 7:
				mapper90_irqunknown = data;
				break;
		}
		return;
	}

	if (address >= 0xd000 && address <= 0xd7ff) {
		switch (address & 0x0003) {
			case 0:
				mapper90_mode = data | ((mapper90_211) ? 0x20 : 0x00);
				break;
			case 1: mapper90_mirror = data; break;
			case 2: mapper90_ppu = data; break;
			case 3:
				mapper90_obank = data;

				if (mapper90_209 && mapper90_obank & 0x80) {
					mapper_ppu_clock = mapper90_ppu_clockmmc4; // chr latching. enabled dynamically
				} else {
					mapper_ppu_clock = NULL;
				}
				break;
		}
	}

	mapper_map();
}

static void mapper90_psg_write(UINT16 address, UINT8 data)
{
	switch (address & 0xfc03) {
		case 0x5800: mapper90_mul0 = data; break;
		case 0x5801: mapper90_mul1 = data; break;
		case 0x5802: mapper90_accu += data; break;
		case 0x5803: mapper90_testreg = data; mapper90_accu = 0; break;
	}
}

static UINT8 mapper90_psg_read(UINT16 address)
{
	switch (address & 0xfc03) {
		case 0x5800: return ((mapper90_mul0 * mapper90_mul1) >> 0) & 0xff;
		case 0x5801: return ((mapper90_mul0 * mapper90_mul1) >> 8) & 0xff;
		case 0x5802: return mapper90_accu;
		case 0x5803: return mapper90_testreg;
	}

	switch (address & 0xffff) {
		case 0x5000: // jumper/DIP register
		case 0x5400:
		case 0x5c00:
			return 0x00;
	}

	return cpu_open_bus;
}

static void mapper90_clockirq()
{
	switch (mapper90_irqmode & 0xc0) {
		case 0x40:
			mapper90_irqcounter++;
			if ((mapper90_irqcounter == 0) && mapper90_irqenable) {
				//bprintf(0, _T("irq+ (mode %x) @ SL %d\n"), mapper90_irqmode, scanline);
				mapper_irq(2);
			}
			break;
		case 0x80:
			mapper90_irqcounter--;
			if ((mapper90_irqcounter == 0xff) && mapper90_irqenable) {
				//bprintf(0, _T("irq- (mode %x) @ SL %d\n"), mapper90_irqmode, scanline);
				mapper_irq(2); // 2 - "super mario world (unl)" HUD shaking
			}
			break;
	}
}

static void mapper90_clockpre()
{
	switch (mapper90_irqmode & 0xc0) {
		case 0x40:
			mapper90_irqprescaler++;
			if ((mapper90_irqprescaler & mapper90_irqprescalermask) == 0) {
				mapper90_clockirq();
			}
			break;
		case 0x80:
			mapper90_irqprescaler--;
			if ((mapper90_irqprescaler & mapper90_irqprescalermask) == mapper90_irqprescalermask) {
				mapper90_clockirq();
			}
			break;
	}
}

static void mapper90_ppu_clock(UINT16 address)
{
	// ppu clock mode
	if ((mapper90_irqmode & 3) == 2) {
		mapper90_clockpre();
		mapper90_clockpre();
	}
}

static void mapper90_ppu_clockmmc4(UINT16 address)
{
	switch (address & 0x3ff8) {
		case 0x0fd8:
			mapper90_chrlatch(0) = 0;
			mapper_map();
			break;
		case 0x0fe8:
			mapper90_chrlatch(0) = 2;
			mapper_map();
			break;
		case 0x1fd8:
			mapper90_chrlatch(1) = 4;
			mapper_map();
			break;
		case 0x1fe8:
			mapper90_chrlatch(1) = 6;
			mapper_map();
			break;
	}
}

static void mapper90_scanline()
{
	if ((mapper90_irqmode & 3) == 1 && RENDERING) {
		for (INT32 i = 0; i < 8; i++)
			mapper90_clockpre();
	}
}

static void mapper90_cycle()
{
	if ((mapper90_irqmode & 3) == 0)
		mapper90_clockpre();
}

static UINT8 mapper90_exp_read(UINT16 address)
{
	return (mapper90_mode & 0x80) ? Cart.PRGRom[PRGExpMap + (address & 0x1fff)] : Cart.WorkRAM[address & 0x1fff];
}

static void mapper90_exp_write(UINT16 address, UINT8 data) // 6000 - 7fff
{
	if (mapper90_mode & 0x80) {
		// prg-rom mode, abort write to wram
		cart_exp_write_abort = 1; // don't fall-through after callback!
	}
}

static UINT8 mapper90_ntread(UINT16 address) // this only gets mapped for 209, 211!
{
	if (mapper90_mode & 0x20) {
		INT32 nt = (address & 0xfff) / 0x400;
		if (mapper90_mode & 0x40 || ((mapper90_ppu & 0x80) ^ (mapper90_nt(nt) & 0x80))) {
			return Cart.CHRRom[(mapper90_nt(nt)) * 0x400 + (address & 0x3ff)];
		}
	}

	return read_nt_int(address); // fall back to internal
}

static UINT16 mapper90_getchr(INT32 num)
{
	UINT16 bank = 0;
	UINT16 mask = 0xffff;

	if (~mapper90_obank & 0x20) { // outer-bank mode
		bank = (mapper90_obank & 1) | ((mapper90_obank >> 2) & 6);

		switch (mapper90_mode & 0x18) {
			case 0x00: bank <<= 5; mask = 0x1f; break;
			case 0x08: bank <<= 6; mask = 0x3f; break;
			case 0x10: bank <<= 7; mask = 0x7f; break;
			case 0x18: bank <<= 8; mask = 0xff; break;
		}
	}

	return ((mapper90_chrlo(num) | (mapper90_chrhi(num) << 8)) & mask) | bank;
}

static UINT8 mapper90_bitswap06(UINT8 data)
{
	return (data & 0x40) >> 6 | (data & 0x20) >> 4 | (data & 0x10) >> 2 | (data & 0x08) | (data & 0x04) << 2 | (data & 0x02) << 4 | (data & 0x01) << 6;
}

static void mapper90_map()
{
	// prg
	INT32 prg8_obank  = (mapper90_obank & 6) << 5;
	INT32 prg16_obank = (mapper90_obank & 6) << 4;
	INT32 prg32_obank = (mapper90_obank & 6) << 3;
	switch (mapper90_mode & 3) {
		case 0x00:
			mapper_map_prg(32, 0, prg32_obank | ((mapper90_mode & 0x04) ? (mapper90_prg(3) & 0xf) : 0xf));
			if (mapper90_mode & 0x80) {
				mapper_map_exp_prg(prg8_obank | (((mapper90_prg(3) << 2) + 3) & 0x3f));
			}
			break;
		case 0x01:
			//bprintf(0, _T("Mapper: JyCompany - 16k prg mode. *untested*\n"));
			mapper_map_prg(16, 0, prg16_obank | (mapper90_prg(1) & 0x1f));
			mapper_map_prg(16, 1, prg16_obank | ((mapper90_mode & 0x04) ? (mapper90_prg(3) & 0x1f) : 0x1f));
			if (mapper90_mode & 0x80) {
				mapper_map_exp_prg(prg8_obank | (((mapper90_prg(3) << 1) + 1) & 0x3f));
			}
			break;
		case 0x02:
			mapper_map_prg(8, 0, prg8_obank | (mapper90_prg(0) & 0x3f));
			mapper_map_prg(8, 1, prg8_obank | (mapper90_prg(1) & 0x3f));
			mapper_map_prg(8, 2, prg8_obank | (mapper90_prg(2) & 0x3f));
			mapper_map_prg(8, 3, prg8_obank | ((mapper90_mode & 0x04) ? (mapper90_prg(3) & 0x3f) : 0x3f));
			if (mapper90_mode & 0x80) {
				mapper_map_exp_prg(prg8_obank | (mapper90_prg(3) & 0x3f));
			}
			break;
		case 0x03: // same as 0x02, but with inverted bits
			//bprintf(0, _T("Mapper: JyCompany - inverted bits. *untested*\n"));
			mapper_map_prg(8, 0, prg8_obank | (mapper90_bitswap06(mapper90_prg(0)) & 0x3f));
			mapper_map_prg(8, 1, prg8_obank | (mapper90_bitswap06(mapper90_prg(1)) & 0x3f));
			mapper_map_prg(8, 2, prg8_obank | (mapper90_bitswap06(mapper90_prg(2)) & 0x3f));
			mapper_map_prg(8, 3, prg8_obank | ((mapper90_mode & 0x04) ? (mapper90_bitswap06(mapper90_prg(3)) & 0x3f) : 0x3f));
			if (mapper90_mode & 0x80) {
				mapper_map_exp_prg(prg8_obank | (mapper90_bitswap06(mapper90_prg(3)) & 0x3f));
			}
			break;
	}

	// chr
	switch (mapper90_mode & 0x18) {
		case 0x00:
			mapper_map_chr( 8, 0, mapper90_getchr(0));
			break;
		case 0x08:
			if (~mapper90_obank & 0x80) { // normal 4k banking
				mapper_map_chr( 4, 0, mapper90_getchr(0));
				mapper_map_chr( 4, 1, mapper90_getchr(4));
			} else {                      // mmc4-like latch 4k banking
				mapper_map_chr( 4, 0, mapper90_getchr(mapper90_chrlatch(0)));
				mapper_map_chr( 4, 1, mapper90_getchr(mapper90_chrlatch(1)));
			}
			break;
		case 0x10:
			mapper_map_chr( 2, 0, mapper90_getchr(0));
			mapper_map_chr( 2, 1, mapper90_getchr(2));
			mapper_map_chr( 2, 2, mapper90_getchr(4));
			mapper_map_chr( 2, 3, mapper90_getchr(6));
			break;
		case 0x18:
			mapper_map_chr( 1, 0, mapper90_getchr(0));
			mapper_map_chr( 1, 1, mapper90_getchr(1));
			mapper_map_chr( 1, 2, mapper90_getchr(2));
			mapper_map_chr( 1, 3, mapper90_getchr(3));
			mapper_map_chr( 1, 4, mapper90_getchr(4));
			mapper_map_chr( 1, 5, mapper90_getchr(5));
			mapper_map_chr( 1, 6, mapper90_getchr(6));
			mapper_map_chr( 1, 7, mapper90_getchr(7));
			break;
	}

	// nametable config - if rom nt's are selected, they will be fed via mapper90_ntread()
	// a RAM nt is always selected for writing, though. (re: Tiny Toon Adv. 6 intro)
	if (mapper90_209 && mapper90_mode & 0x20) {
		nametable_map(0, mapper90_nt(0) & 1);
		nametable_map(1, mapper90_nt(1) & 1);
		nametable_map(2, mapper90_nt(2) & 1);
		nametable_map(3, mapper90_nt(3) & 1);
	} else {
		//standard nt config
		switch (mapper90_mirror & 0x3) {
			case 0: set_mirroring(VERTICAL); break;
			case 1: set_mirroring(HORIZONTAL); break;
			case 2: set_mirroring(SINGLE_LOW); break;
			case 3: set_mirroring(SINGLE_HIGH); break;
		}
	}
}
#undef mapper90_irqenable
#undef mapper90_irqmode
#undef mapper90_irqcounter
#undef mapper90_irqprescaler
#undef mapper90_irqxor
#undef mapper90_irqprescalermask
#undef mapper90_irqunknown
#undef mapper90_mode
#undef mapper90_mirror
#undef mapper90_ppu
#undef mapper90_obank
#undef mapper90_prg
#undef mapper90_chrlo
#undef mapper90_chrhi
#undef mapper90_nt

// --[ mapper 91: older JyCompany/Hummer
#define mapper91_prg(x)		(mapper_regs[0x00 + (x)])
#define mapper91_chr(x)		(mapper_regs[0x04 + (x)])
#define mapper91_irqcount   (mapper_regs[0x1f - 0x00])
#define mapper91_irqenable	(mapper_regs[0x1f - 0x01])

static void mapper91_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000) {
		case 0x6000:
			mapper91_chr(address & 3) = data;
			break;
		case 0x7000:
			switch (address & 3) {
				case 0:
				case 1:
					mapper91_prg(address & 1) = data;
					break;
				case 2:
					mapper91_irqenable = 0;
					mapper91_irqcount = 0;
					M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
					break;
				case 3:
					mapper91_irqenable = 1;
					break;
			}
			break;
	}

	mapper_map();
}

static void mapper91_scanline()
{
	if (mapper91_irqenable && RENDERING) {
		mapper91_irqcount++;
		if (mapper91_irqcount == 8) {
			mapper_irq(0x14); // 0x14 - gets rid of artefacts in "dragon ball z - super butouden 2"
			mapper91_irqenable = 0;
		}
	}
}

static void mapper91_map()
{
	mapper_map_prg( 8, 0, mapper91_prg(0));
	mapper_map_prg( 8, 1, mapper91_prg(1));
	mapper_map_prg( 8, 2, -2);
	mapper_map_prg( 8, 3, -1);

	mapper_map_chr( 2, 0, mapper91_chr(0));
	mapper_map_chr( 2, 1, mapper91_chr(1));
	mapper_map_chr( 2, 2, mapper91_chr(2));
	mapper_map_chr( 2, 3, mapper91_chr(3));
}
#undef mapper91_prg
#undef mapper91_chr
#undef mapper91_irqcount
#undef mapper91_irqenable

// --[ mapper 17: FFE / Front Far East SMC (type 17)
#define mapper17_prg(x)		(mapper_regs[0x00 + (x)])
#define mapper17_chr(x)		(mapper_regs[0x04 + (x)])
#define mapper17_irqcount   (mapper_regs16[0x00])
#define mapper17_irqenable	(mapper_regs[0x1f - 0x00])
#define mapper17_mirror		(mapper_regs[0x1f - 0x01])

static void mapper17_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x42fe:
			switch (data & 0x10) {
				case 0x00: mapper17_mirror = 2; break;
				case 0x10: mapper17_mirror = 3; break;
			}
			break;
		case 0x42ff:
			switch (data & 0x10) {
				case 0x00: mapper17_mirror = 0; break;
				case 0x10: mapper17_mirror = 1; break;
			}
			break;

		case 0x4501:
			mapper17_irqenable = 0;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0x4502:
			mapper17_irqcount = (mapper17_irqcount & 0xff00) | data;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0x4503:
			mapper17_irqcount = (mapper17_irqcount & 0x00ff) | (data << 8);
			mapper17_irqenable = 1;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;

		case 0x4504:
		case 0x4505:
		case 0x4506:
		case 0x4507:
			mapper17_prg(address & 3) = data;
			break;

		case 0x4510:
		case 0x4511:
		case 0x4512:
		case 0x4513:
		case 0x4514:
		case 0x4515:
		case 0x4516:
		case 0x4517:
			mapper17_chr(address & 7) = data;
			break;
	}

	mapper_map();
}

static void mapper17_cycle()
{
	if (mapper17_irqenable) {
		mapper17_irqcount++;
		if (mapper17_irqcount == 0x0000) {
			mapper_irq(0);
			mapper17_irqenable = 0;
		}
	}
}

static void mapper17_map()
{
	mapper_map_prg( 8, 0, mapper17_prg(0));
	mapper_map_prg( 8, 1, mapper17_prg(1));
	mapper_map_prg( 8, 2, mapper17_prg(2));
	mapper_map_prg( 8, 3, mapper17_prg(3));

	mapper_map_chr( 1, 0, mapper17_chr(0));
	mapper_map_chr( 1, 1, mapper17_chr(1));
	mapper_map_chr( 1, 2, mapper17_chr(2));
	mapper_map_chr( 1, 3, mapper17_chr(3));
	mapper_map_chr( 1, 4, mapper17_chr(4));
	mapper_map_chr( 1, 5, mapper17_chr(5));
	mapper_map_chr( 1, 6, mapper17_chr(6));
	mapper_map_chr( 1, 7, mapper17_chr(7));

	switch (mapper17_mirror & 0x3) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}
}
#undef mapper17_prg
#undef mapper17_chr
#undef mapper17_irqcount
#undef mapper17_irqenable
#undef mapper17_mirror

// ---[ mapper 400 8-Bit X-Mas 2017
#define mapper400_reg(x)		(mapper_regs[(x)]) // 0 - 3

static void mapper400_write(UINT16 address, UINT8 data)
{
	if (address >= 0x7800 && address <= 0x7fff) {
		mapper400_reg(0) = data;
	}
	if (address >= 0x8000 && address <= 0xbfff) {
		mapper400_reg(1) = data;
	}
	if (address >= 0xc000 && address <= 0xffff) {
		mapper400_reg(2) = data;
	}

	mapper_map();
}

static void mapper400_map()
{
	mapper_map_prg(16, 0, (mapper400_reg(0) & 0x78) | (mapper400_reg(2) & 0x07));
	mapper_map_prg(16, 1, (mapper400_reg(0) & 0x78) | 0x07);

	mapper_map_chr( 8, 0, (mapper400_reg(2) >> 5) & 0x03);

	if (mapper400_reg(0) != 0x80) {
		set_mirroring((mapper400_reg(0) & 0x20) ? HORIZONTAL : VERTICAL);
	}
}

#undef mapper400_reg

// ---[ mapper 413 Super Russian Roulette
#define mapper413_reg(x)		(mapper_regs[(x)]) // 0 - 3
#define mapper413_irq_latch		(mapper_regs[4])
#define mapper413_irq_count		(mapper_regs[5])
#define mapper413_irq_enable	(mapper_regs[6])
#define mapper413_ext_control   (mapper_regs[7])

static UINT32 *mapper413_ext_addr = (UINT32*)&mapper_regs16[0];

static UINT8 mapper413_read_pcm()
{
	UINT8 data = Cart.ExtData[mapper413_ext_addr[0] & (Cart.ExtDataSize - 1)];
	if (mapper413_ext_control & 0x02) mapper413_ext_addr[0]++;
	return data;
}

static UINT8 mapper413_prg_read(UINT16 address)
{
	if ((address & 0xf000) == 0xc000) {
		return mapper413_read_pcm();
	}

	return mapper_prg_read_int(address);
}

static void mapper413_prg_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000) {
		case 0x8000: mapper413_irq_latch = data; break;
		case 0x9000: mapper413_irq_count = 0; break;
		case 0xa000: mapper413_irq_enable = 0; M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
		case 0xb000: mapper413_irq_enable = 1; break;
		case 0xc000: mapper413_ext_addr[0] = (mapper413_ext_addr[0] << 1) | (data >> 7); break;
		case 0xd000: mapper413_ext_control = data; break;
		case 0xe000:
		case 0xf000: mapper413_reg((data >> 6)) = data & 0x3f; break;
	}

	mapper_map();
}

static UINT8 mapper413_psg_read(UINT16 address)
{
	if ((address & 0xf800) == 0x4800) {
		return mapper413_read_pcm();
	}
	if (address >= 0x5000) {
		return Cart.PRGRom[0x1000 | (address & 0x1fff)];
	}

	return cpu_open_bus;
}
static UINT8 mapper413_exp_read(UINT16 address)
{
	return Cart.PRGRom[PRGExpMap + (address & 0x1fff)];
}

static void mapper413_map()
{
	mapper_map_exp_prg(mapper413_reg(0)); // 6000 - 7fff
	mapper_map_prg( 8, 0, mapper413_reg(1));
	mapper_map_prg( 8, 1, mapper413_reg(2));
	mapper_map_prg( 8, 2, 3);
	mapper_map_prg( 8, 3, 4);

	mapper_map_chr( 4, 0, mapper413_reg(3));
	mapper_map_chr( 4, 1, 0x3d);
}

static void mapper413_scanline()
{
	if (mapper413_irq_count == 0) {
		mapper413_irq_count = mapper413_irq_latch;
	} else {
		mapper413_irq_count--;
	}

	if (mapper413_irq_enable && mapper413_irq_count == 0) {
		if (RENDERING) {
			mapper_irq(0);
		}
	}
}

#undef mapper413_reg
#undef mapper413_irq_latch
#undef mapper413_irq_count
#undef mapper413_irq_enable
#undef mapper413_ext_control

// --[ mapper 28: Action53 Home-brew multicart
#define mapper28_mirror		(mapper_regs[0x1f - 0])
#define mapper28_mirrorbit  (mapper_regs[0x1f - 1])
#define mapper28_cmd		(mapper_regs[0x1f - 2])
#define mapper28_regs(x)	(mapper_regs[(x)])

static void mapper28_write(UINT16 address, UINT8 data)
{
	if (address >= 0x5000 && address <= 0x5fff) {
		mapper28_cmd = ((data >> 6) & 2) | (data & 1);
	} else if (address >= 0x8000) {
		switch (mapper28_cmd) {
			case 0:
			case 1:
				mapper28_mirrorbit = (data >> 4) & 1;
				break;
			case 2:
				mapper28_mirrorbit = data & 1;
				break;
		}
		mapper28_regs(mapper28_cmd & 3) = data;

		mapper_map();
	}
}

static void mapper28_map()
{
	UINT8 slot = (mapper28_regs(2) & 0x04) >> 2;
	UINT8 outerbanksize = (mapper28_regs(2) & 0x30) >> 4;
	UINT8 outerbank = mapper28_regs(3) << 1;
	UINT8 prgbank = (mapper28_regs(1) & 0xf) << ((~mapper28_regs(2) & 0x08) >> 3);
	UINT8 page = (outerbank & ~((1 << (outerbanksize+1))-1)) | (prgbank & ((1 << (outerbanksize+1))-1));

	if (mapper28_regs(2) & 0x08) {
		mapper_map_prg(16, slot ^ 1, page);
		mapper_map_prg(16, slot    , (outerbank & 0x1fe) | slot);
	} else {
		mapper_map_prg(16, 0, page);
		mapper_map_prg(16, 1, page | 1);
	}

	mapper_map_chr( 8, 0, mapper28_regs(0) & 0x03);

	UINT8 mirror = mapper28_regs(2) & 0x03;
	if (~mirror & 0x02) {
		mirror = mapper28_mirrorbit;
	}

	switch (mirror) {
		case 0: set_mirroring(SINGLE_LOW); break;
		case 1: set_mirroring(SINGLE_HIGH); break;
		case 2: set_mirroring(VERTICAL); break;
		case 3: set_mirroring(HORIZONTAL); break;
	}
}
#undef mapper28_mirror
#undef mapper28_mirrorbit
#undef mapper28_cmd
#undef mapper28_regs

// --[ mapper 33, 48: taito
#define mapper33_prg(x)		(mapper_regs[0 + x])
#define mapper33_chr(x)		(mapper_regs[2 + x])
#define mapper33_mirror		(mapper_regs[0x1f - 0])
#define mapper33_irqenable	(mapper_regs[0x1f - 1])
#define mapper48            (mapper_regs[0x1f - 2])
#define mapper33_irqcount	(mapper_regs16[0x1f - 0])
#define mapper33_irqlatch	(mapper_regs16[0x1f - 1])
#define mapper33_irqreload  (mapper_regs16[0x1f - 2])
#define mapper48_flintstones (mapper_regs16[0x1f - 3])

static void mapper33_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf003) {
		case 0x8000:
			mapper33_prg(0) = data & 0x3f;
			if (mapper48 == 0) mapper33_mirror = data & 0x40;
			break;
		case 0x8001: mapper33_prg(1) = data & 0x3f; break;

		case 0x8002: mapper33_chr(0) = data; break;
		case 0x8003: mapper33_chr(1) = data; break;
		case 0xa000: mapper33_chr(2) = data; break;
		case 0xa001: mapper33_chr(3) = data; break;
		case 0xa002: mapper33_chr(4) = data; break;
		case 0xa003: mapper33_chr(5) = data; break;
	}

	if (mapper48) {
		switch (address & 0xf003) {
			case 0xc000: mapper33_irqlatch = (data ^ 0xff) + ((mapper48_flintstones) ? 0 : 1); M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
			case 0xc001: mapper33_irqreload = 1; mapper33_irqcount = 0; M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
			case 0xc002: mapper33_irqenable = 1; break;
			case 0xc003: mapper33_irqenable = 0; M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
			case 0xe000: if (mapper48) mapper33_mirror = data & 0x40; break;
		}
	}

	mapper_map();
}

static void mapper33_map()
{
	mapper_map_prg( 8, 0, mapper33_prg(0));
	mapper_map_prg( 8, 1, mapper33_prg(1));
	mapper_map_prg( 8, 2, -2);
	mapper_map_prg( 8, 3, -1);

	mapper_map_chr( 2, 0, mapper33_chr(0));
	mapper_map_chr( 2, 1, mapper33_chr(1));

	mapper_map_chr( 1, 4, mapper33_chr(2));
	mapper_map_chr( 1, 5, mapper33_chr(3));
	mapper_map_chr( 1, 6, mapper33_chr(4));
	mapper_map_chr( 1, 7, mapper33_chr(5));

	set_mirroring((mapper33_mirror) ? HORIZONTAL : VERTICAL);
}

static void mapper33_scanline()
{
	INT32 cnt = mapper33_irqenable;
	if (mapper33_irqcount == 0 || mapper33_irqreload) {
		mapper33_irqreload = 0;
		mapper33_irqcount = mapper33_irqlatch;
	} else {
		mapper33_irqcount--;
	}

	if (cnt && mapper33_irqenable && mapper33_irqcount == 0) {
		mapper_irq((mapper48_flintstones) ? 0x13 : 0x06);
	}
}
#undef mapper33_mirror
#undef mapper33_irqenable
#undef mapper33_prg
#undef mapper33_chr
#undef mapper33_irqcount
#undef mapper33_irqlatch
#undef mapper33_irqreload

// --[ mapper 36 - TXC (Policeman, Strike Wolf)
#define mapper36_prg		(mapper_regs[0x1f - 0])
#define mapper36_chr		(mapper_regs[0x1f - 1])
#define mapper36_RR			(mapper_regs[0x1f - 2])
#define mapper36_PP			(mapper_regs[0x1f - 3])
#define mapper36_invert		(mapper_regs[0x1f - 4])
#define mapper36_mode		(mapper_regs[0x1f - 5])

static UINT8 mapper36_read(UINT16 address)
{
	if ((address & 0xe100) == 0x4100)
		return (cpu_open_bus & 0xcf) | (mapper36_RR << 4);
	else
		return cpu_open_bus;
}

static void mapper36_write(UINT16 address, UINT8 data)
{
	if (address > 0x7fff) {
		mapper36_prg = mapper36_RR;
	} else {
		switch (address & 0xe103) {
			case 0x4100:
				if (mapper36_mode) {
					mapper36_RR++;
				} else {
					mapper36_RR = mapper36_PP;
				}
				break;
			case 0x4101:
				mapper36_invert = data & 0x10;
				break;
			case 0x4102:
				mapper36_PP = (data >> 4) & 0x3;
				break;
			case 0x4103:
				mapper36_mode = data & 0x10;
				break;
		}
		switch (address & 0xe200) {
			case 0x4200:
				mapper36_chr = data & 0xf;
				break;
		}
	}

	mapper_map();
}

static void mapper36_map()
{
	mapper_map_prg(32, 0, mapper36_prg);

	mapper_map_chr(8, 0, mapper36_chr);
}

#undef mapper36_prg
#undef mapper36_chr
#undef mapper36_RR
#undef mapper36_mode

// --[ mapper 42 Ai Senshi Nicol FDS conversion
#define mapper42_mirror		(mapper_regs[0x1f - 0])
#define mapper42_prg		(mapper_regs[0x1f - 1])
#define mapper42_chr		(mapper_regs[0x1f - 2])
#define mapper42_irqenable	(mapper_regs[0x1f - 3])
#define mapper42_irqcount   (mapper_regs16[0])

static void mapper42_write(UINT16 address, UINT8 data)
{
	switch (address & 0xe003) {
		case 0x8000: mapper42_chr = data; break;
		case 0xe000: mapper42_prg = data & 0xf; break;
		case 0xe001: mapper42_mirror = data; break;
		case 0xe002:
			mapper42_irqenable = data & 2;
			if (!mapper42_irqenable) {
				mapper42_irqcount = 0;
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			}
			break;
	}

	mapper_map();
}

static void mapper42_map()
{
	// set prg-rom @ 6000 - 7fff, fed by mapper42_exp_read()
	mapper_map_exp_prg(mapper42_prg);
	// normal prg-rom @ 8000 - ffff
	mapper_map_prg(32, 0, -1);

	mapper_map_chr(8, 0, mapper42_chr);

	set_mirroring((mapper42_mirror & 0x8) ? HORIZONTAL : VERTICAL);
}

static UINT8 mapper42_exp_read(UINT16 address)
{
	return Cart.PRGRom[PRGExpMap + (address & 0x1fff)];
}

static void mapper42_cycle()
{
	if (mapper42_irqenable) {
		mapper42_irqcount++;
		if (mapper42_irqcount >= 0x8000) {
			mapper42_irqcount -= 0x8000;
		}
		if (mapper42_irqcount >= 0x6000) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		} else {
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		}
	}
}

#undef mapper42_mirror
#undef mapper42_prg
#undef mapper42_chr
#undef mapper42_irqenable
#undef mapper42_irqcount

// --[ mapper 108 Meikyuu Jiin Dababa (Japan) FDS conversion
#define mapper108_prg		(mapper_regs[0x1f - 0])

static void mapper108_write(UINT16 address, UINT8 data)
{
	if ((address >= 0x8000 && address <= 0x8fff) || (address >= 0xf000 && address <= 0xffff)) {
		mapper108_prg = data;

		mapper_map();
	}
}

static void mapper108_map()
{
	// set prg-rom @ 6000 - 7fff, fed by mapper108_exp_read()
	mapper_map_exp_prg(mapper108_prg);
	// normal prg-rom @ 8000 - ffff
	mapper_map_prg(32, 0, -1);

	mapper_map_chr(8, 0, 0);
}

static UINT8 mapper108_exp_read(UINT16 address)
{
	return Cart.PRGRom[PRGExpMap + (address & 0x1fff)];
}

#undef mapper108_prg

// --[ mapper 111 Cheapocabra (GTROM)
#define mapper111_reg		(mapper_regs[0x1f - 0])

static void mapper111_write(UINT16 address, UINT8 data)
{
	if ((address >= 0x5000 && address <= 0x5fff) || (address >= 0x7000 && address <= 0x7fff)) {
		mapper111_reg = data;
		mapper_map();
	}
}

static void mapper111_map()
{
	mapper_map_prg(32, 0, mapper111_reg & 0xf);
	mapper_map_chr(8, 0, (mapper111_reg >> 4) & 0x1);

	INT32 nt = (mapper111_reg & 0x20) ? (0x4000 + 0x2000) : (0x4000 + 0x0000);

	for (INT32 i = 0; i < 4; i++) {
		nametable_mapraw(i & 3, Cart.CHRRam + nt + (i * 0x400), MEM_RAM);
	}
}

#undef mapper111_reg

// --[ mapper 120 Tobidase Daisakusen (Japan) FDS Conv. *game is buggy*
#define mapper120_prg		(mapper_regs[0x1f - 0])

static void mapper120_write(UINT16 address, UINT8 data)
{
	if (address == 0x41ff) {
		mapper120_prg = data & 0x7;
	}

	mapper_map();
}

static void mapper120_map()
{
	// set prg-rom @ 6000 - 7fff, fed by mapper120_exp_read()
	mapper_map_exp_prg(mapper120_prg);
	// normal prg-rom @ 8000 - ffff
	mapper_map_prg(32, 0, 2);

	mapper_map_chr(8, 0, 0);
}

static UINT8 mapper120_exp_read(UINT16 address)
{
	return Cart.PRGRom[PRGExpMap + (address & 0x1fff)];
}

#undef mapper120_prg

// --[ mapper 64 - Tengen (Atari)
#define mapper64_mirror			(mapper_regs[0x1f - 0])
#define mapper64_regnum			(mapper_regs[0x1f - 1])
#define mapper64_reload			(mapper_regs[0x1f - 2])
#define mapper64_irqmode		(mapper_regs[0x1f - 3])
#define mapper64_irqenable		(mapper_regs[0x1f - 4])
#define mapper64_irqlatch		(mapper_regs[0x1f - 5])
#define mapper64_irqcount		(mapper_regs[0x1f - 6])
#define mapper64_irqprescale	(mapper_regs[0x1f - 7])
#define mapper64_cycles         (mapper_regs16[0])

static void mapper64_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
        switch (address & 0xe001) {
			case 0xA000: mapper64_mirror = data & 1; mapper_map(); break;
            case 0x8000: mapper64_regnum = data; break;
			case 0x8001:
				mapper_regs[(mapper64_regnum & 0xf)] = data;
				mapper_map();
				break;

			case 0xC000:
				mapper64_irqlatch = data;
				break;
			case 0xC001:
				mapper64_reload = 1;
				mapper64_irqprescale = 0;
				mapper64_irqmode = data & 1;
				break;
			case 0xE000:
				mapper64_irqenable = 0;
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				break;
			case 0xE001:
				mapper64_irqenable = 1;
				M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
				break;
        }
	}
}

static void mapper64_map()
{
	if (mapper64_regnum & 0x20) {
		mapper_map_chr(1, 0, mapper_regs[0]);
		mapper_map_chr(1, 1, mapper_regs[8]);
		mapper_map_chr(1, 2, mapper_regs[1]);
		mapper_map_chr(1, 3, mapper_regs[9]);
	} else {
		mapper_map_chr(2, 0, mapper_regs[0] >> 1);
		mapper_map_chr(2, 1, mapper_regs[1] >> 1);
	}
	mapper_map_chr(1, 4, mapper_regs[2]);
	mapper_map_chr(1, 5, mapper_regs[3]);
	mapper_map_chr(1, 6, mapper_regs[4]);
	mapper_map_chr(1, 7, mapper_regs[5]);

	mapper_map_prg(8, 0, mapper_regs[6]);
	mapper_map_prg(8, 1, mapper_regs[7]);
	mapper_map_prg(8, 2, mapper_regs[0xf]);
	mapper_map_prg(8, 3, -1);
	set_mirroring(mapper64_mirror ? HORIZONTAL : VERTICAL);
}

static void mapper64_irq_reload_logic()
{
	if (mapper64_reload) {
		mapper64_irqcount = (mapper64_irqlatch) ? mapper64_irqlatch | 1 : 0;
		if (mapper64_irqcount == 0 && mapper64_cycles > 0x10)
			mapper64_irqcount = 1;
		mapper64_reload = 0;
		mapper64_cycles = 0;
	} else if (mapper64_irqcount == 0) {
		mapper64_irqcount = mapper64_irqlatch;
		if (mapper64_cycles > 0x10)
			mapper64_cycles = 0;
	} else mapper64_irqcount --;
}

static void mapper64_scanline()
{
	if (RENDERING) {
		if (mapper64_irqmode == 0) {

			mapper64_irq_reload_logic();

			if (mapper64_irqcount == 0 && mapper64_irqenable) {
				mapper_irq(1); // assert irq in 1 m2 cycle
			}
		}
	}
}

static void mapper64_cycle()
{
	if (mapper64_cycles == 0xffff) mapper64_cycles = 0x10;
	mapper64_cycles++;
	if (mapper64_irqmode == 1) {
		mapper64_irqprescale++;
		while (mapper64_irqprescale == 4) {
			mapper64_irqprescale = 0;

			mapper64_irq_reload_logic();

			if (mapper64_irqcount == 0 && mapper64_irqenable) {
				mapper_irq(4); // assert irq in 4 m2 cycles
			}
		}
	}
}

#undef mapper64_mirror
#undef mapper64_regnum
#undef mapper64_reload
#undef mapper64_irqmode
#undef mapper64_irqenable
#undef mapper64_irqlatch
#undef mapper64_irqcount
#undef mapper64_irqprescale

// --[ mapper 65 - irem h3001(?): Spartan X2, Kaiketsu Yanchamaru 3: Taiketsu! Zouringen,
#define mapper65_mirror		(mapper_regs[0x1f - 0])
#define mapper65_irqenable	(mapper_regs[0x1f - 1])
#define mapper65_prg(x)		(mapper_regs[0 + x])
#define mapper65_chr(x)		(mapper_regs[3 + x])
#define mapper65_irqcount	(mapper_regs16[0x1f - 0])
#define mapper65_irqlatch	(mapper_regs16[0x1f - 1])


static void mapper65_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0x8000: mapper65_prg(0) = data; break;
		case 0xa000: mapper65_prg(1) = data; break;
		case 0xc000: mapper65_prg(2) = data; break;
		case 0x9001: mapper65_mirror = (~data >> 7) & 1; break;
		case 0x9003: mapper65_irqenable = data & 0x80; M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
		case 0x9004: mapper65_irqcount = mapper65_irqlatch; break;
		case 0x9005: mapper65_irqlatch = (mapper65_irqlatch & 0x00ff) | (data << 8); break;
		case 0x9006: mapper65_irqlatch = (mapper65_irqlatch & 0xff00) | data; break;
		case 0xb000: mapper65_chr(0) = data; break;
		case 0xb001: mapper65_chr(1) = data; break;
		case 0xb002: mapper65_chr(2) = data; break;
		case 0xb003: mapper65_chr(3) = data; break;
		case 0xb004: mapper65_chr(4) = data; break;
		case 0xb005: mapper65_chr(5) = data; break;
		case 0xb006: mapper65_chr(6) = data; break;
		case 0xb007: mapper65_chr(7) = data; break;
	}
	mapper_map();
}

static void mapper65_map()
{
	mapper_map_prg( 8, 0, mapper65_prg(0));
	mapper_map_prg( 8, 1, mapper65_prg(1));
	mapper_map_prg( 8, 2, mapper65_prg(2));
	mapper_map_prg( 8, 3, -1);

	mapper_map_chr( 1, 0, mapper65_chr(0));
	mapper_map_chr( 1, 1, mapper65_chr(1));
	mapper_map_chr( 1, 2, mapper65_chr(2));
	mapper_map_chr( 1, 3, mapper65_chr(3));
	mapper_map_chr( 1, 4, mapper65_chr(4));
	mapper_map_chr( 1, 5, mapper65_chr(5));
	mapper_map_chr( 1, 6, mapper65_chr(6));
	mapper_map_chr( 1, 7, mapper65_chr(7));

	set_mirroring(mapper65_mirror ? VERTICAL : HORIZONTAL);
}

static void mapper65_cycle()
{
	if (mapper65_irqenable) {
		mapper65_irqcount--;

		if (mapper65_irqcount == 0) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			mapper65_irqenable = 0;
		}
	}
}
#undef mapper65_mirror
#undef mapper65_irqenable
#undef mapper65_prg
#undef mapper65_chr
#undef mapper65_irqcount
#undef mapper65_irqlatch


// --[ mapper 67 - Sunsoft-3 (Fantasy Zone II, Mito Koumon II: Sekai Manyuu Ki)
#define mapper67_mirror		(mapper_regs[0x1f - 0])
#define mapper67_irqenable	(mapper_regs[0x1f - 1])
#define mapper67_irqtoggle  (mapper_regs[0x1f - 2])
#define mapper67_prg		(mapper_regs[0])
#define mapper67_chr(x)		(mapper_regs[1 + x])
#define mapper67_irqcount	(mapper_regs16[0x1f - 0])

static void mapper67_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf800) {
		case 0x8800: mapper67_chr(0) = data; break;
		case 0x9800: mapper67_chr(1) = data; break;
		case 0xa800: mapper67_chr(2) = data; break;
		case 0xb800: mapper67_chr(3) = data; break;
		case 0xc000:
		case 0xc800:
			if (mapper67_irqtoggle == 0) {
				mapper67_irqcount = (mapper67_irqcount & 0x00ff) | (data << 8);
			} else {
				mapper67_irqcount = (mapper67_irqcount & 0xff00) | (data << 0);
			}
			mapper67_irqtoggle ^= 1;
			break;
		case 0xd800:
			mapper67_irqtoggle = 0;
			mapper67_irqenable = data & 0x10;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xe800:
			mapper67_mirror = data & 3;
			break;
		case 0xf800:
			mapper67_prg = data;
			break;
	}
	mapper_map();
}

static void mapper67_map()
{
	mapper_map_prg(16, 0, mapper67_prg);
	mapper_map_prg(16, 1, -1);

	mapper_map_chr( 2, 0, mapper67_chr(0));
	mapper_map_chr( 2, 1, mapper67_chr(1));
	mapper_map_chr( 2, 2, mapper67_chr(2));
	mapper_map_chr( 2, 3, mapper67_chr(3));

	switch (mapper67_mirror) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}
}

static void mapper67_cycle()
{
	if (mapper67_irqenable) {
		mapper67_irqcount--;

		if (mapper67_irqcount == 0xffff) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			mapper67_irqenable = 0;
		}
	}
}
#undef mapper67_mirror
#undef mapper67_irqenable
#undef mapper67_irqtoggle
#undef mapper67_prg
#undef mapper67_chr
#undef mapper67_irqcount

// --[ mapper 68 - Sunsoft-4 (After Burner 1 & 2, Nantettatte!! Baseball, Maharaja)
// Notes: Nantettatte!! Baseball ext.roms not supported yet.
#define mapper68_mirror		(mapper_regs[0x1f - 0])
#define mapper68_prg		(mapper_regs[0x1f - 1])
#define mapper68_wram_en    (mapper_regs[0x1f - 2])
#define mapper68_nt0		(mapper_regs[0x1f - 3])
#define mapper68_nt1		(mapper_regs[0x1f - 4])
#define mapper68_chr(x)		(mapper_regs[0 + (x)])

static INT32 *mapper68_timer = (INT32*)&mapper_regs16[0];

static UINT8 mapper68_exp_read(UINT16 address) // 6000-7fff
{
	if (mapper68_wram_en) {
		return Cart.WorkRAM[address & 0x1fff];
	} else {
		return cpu_open_bus;
	}
}

static void mapper68_exp_write(UINT16 address, UINT8 data) // 6000-7fff
{
	if (mapper68_wram_en) {
		mapper68_timer[0] = 107520;
	} else {
		cart_exp_write_abort = 1; // wram/sram area disabled (disable writing on return from this callback)
	}
}

static void mapper68_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000) {
		case 0x8000: mapper68_chr(0) = data; break;
		case 0x9000: mapper68_chr(1) = data; break;
		case 0xa000: mapper68_chr(2) = data; break;
		case 0xb000: mapper68_chr(3) = data; break;
		case 0xc000: mapper68_nt0 = data | 0x80; break;
		case 0xd000: mapper68_nt1 = data | 0x80; break;
		case 0xe000: mapper68_mirror = data; break;
		case 0xf000:
			mapper68_prg = data & 7;
			mapper68_wram_en = data & 0x10;
			break;
	}
	mapper_map();
}

static void mapper68_map()
{
	mapper_map_prg(16, 0, mapper68_prg & 7);
	mapper_map_prg(16, 1, -1);

	mapper_map_chr( 2, 0, mapper68_chr(0));
	mapper_map_chr( 2, 1, mapper68_chr(1));
	mapper_map_chr( 2, 2, mapper68_chr(2));
	mapper_map_chr( 2, 3, mapper68_chr(3));

	if (mapper68_mirror & 0x10) {
		switch (mapper68_mirror & 3) {
			case 0:
				nametable_mapraw(0, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				nametable_mapraw(2, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				nametable_mapraw(1, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				nametable_mapraw(3, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				break;
			case 1:
				nametable_mapraw(0, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				nametable_mapraw(1, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				nametable_mapraw(2, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				nametable_mapraw(3, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				break;
			case 2:
				nametable_mapraw(0, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				nametable_mapraw(1, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				nametable_mapraw(2, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				nametable_mapraw(3, Cart.CHRRom + (mapper68_nt0 << 10), MEM_ROM);
				break;
			case 3:
				nametable_mapraw(0, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				nametable_mapraw(1, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				nametable_mapraw(2, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				nametable_mapraw(3, Cart.CHRRom + (mapper68_nt1 << 10), MEM_ROM);
				break;
		}

	} else {
		switch (mapper68_mirror & 3) {
			case 0: set_mirroring(VERTICAL); break;
			case 1: set_mirroring(HORIZONTAL); break;
			case 2: set_mirroring(SINGLE_LOW); break;
			case 3: set_mirroring(SINGLE_HIGH); break;
		}
	}
}

static void mapper68_cycle()
{
	if (mapper68_timer[0] > 0) {
		mapper68_timer[0]--;

		if (mapper68_timer[0] == 0) {
		}
	}
}

#undef mapper68_mirror
#undef mapper68_prg
#undef mapper68_chr
#undef mapper68_wram_en
#undef mapper68_nt0
#undef mapper68_nt1

// --[ mapper 69 (sunsoft fme-7: Gimmick!, Hebereke, Pyokotan)
#define mapper69_mirror		(mapper_regs[0x1f - 0])
#define mapper69_regnum		(mapper_regs[0x1f - 1])
#define mapper69_irqenable	(mapper_regs[0x1f - 2])
#define mapper69_irqcount   (mapper_regs16[0])

static void mapper69_write(UINT16 address, UINT8 data)
{
	switch (address & 0xe000) {
		case 0x8000: mapper69_regnum = data & 0xf; break;
		case 0xa000:
			switch (mapper69_regnum) {
				case 0x0:
				case 0x1:
				case 0x2:
				case 0x3:
				case 0x4:
				case 0x5:
				case 0x6:
				case 0x7: mapper_regs[(mapper69_regnum & 0xf)] = data; break;
				case 0x8: mapper_regs[0x8 + 3] = data; break;
				case 0x9: mapper_regs[0x8 + 0] = data; break;
				case 0xa: mapper_regs[0x8 + 1] = data; break;
				case 0xb: mapper_regs[0x8 + 2] = data; break;
				case 0xc: mapper69_mirror = data & 3; break;
				case 0xd: mapper69_irqenable = data; M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
				case 0xe: mapper69_irqcount = (mapper69_irqcount & 0xff00) | data; break;
				case 0xf: mapper69_irqcount = (mapper69_irqcount & 0x00ff) | (data << 8); break;
			}
			if (mapper69_regnum < 0xd)
				mapper_map();
			break;

		case 0xc000:
			AY8910Write(0, 0, data);
			break;
		case 0xe000:
			AY8910Write(0, 1, data);
			break;
	}
}

static void mapper69_map()
{
	if ((mapper_regs[8 + 3] & 0xc0) == 0xc0) {
		// set prg-ram @ 6000 - 7fff
		// handled in mapper69_exp_write/read
	} else {
		// set prg-rom @ 6000 - 7fff
		mapper_map_exp_prg(mapper_regs[8 + 3] & 0x3f);
	}

	mapper_map_prg( 8, 0, mapper_regs[8 + 0]);
	mapper_map_prg( 8, 1, mapper_regs[8 + 1]);
	mapper_map_prg( 8, 2, mapper_regs[8 + 2]);
	mapper_map_prg( 8, 3, -1);

	mapper_map_chr(1, 0, mapper_regs[0]);
	mapper_map_chr(1, 1, mapper_regs[1]);
	mapper_map_chr(1, 2, mapper_regs[2]);
	mapper_map_chr(1, 3, mapper_regs[3]);
	mapper_map_chr(1, 4, mapper_regs[4]);
	mapper_map_chr(1, 5, mapper_regs[5]);
	mapper_map_chr(1, 6, mapper_regs[6]);
	mapper_map_chr(1, 7, mapper_regs[7]);

	switch (mapper69_mirror) {
        case 0: set_mirroring(VERTICAL); break;
        case 1: set_mirroring(HORIZONTAL); break;
        case 2: set_mirroring(SINGLE_LOW); break;
        case 3: set_mirroring(SINGLE_HIGH); break;
	}
}

static void mapper69_exp_write(UINT16 address, UINT8 data)
{
	if ((mapper_regs[8 + 3] & 0xc0) == 0xc0) {
		Cart.WorkRAM[address & Cart.WorkRAMMask] = data;
	}
	cart_exp_write_abort = 1;
}

static UINT8 mapper69_exp_read(UINT16 address)
{
	if ((mapper_regs[8 + 3] & 0xc0) == 0x40) { // ram selected, but disabled
		return cpu_open_bus;
	} if ((mapper_regs[8 + 3] & 0xc0) == 0xc0) { // ram selected and enabled
		return Cart.WorkRAM[address & Cart.WorkRAMMask];
	} else { // rom selected
		return Cart.PRGRom[PRGExpMap + (address & 0x1fff)];
	}
}

static void mapper69_cycle()
{
	if (mapper69_irqenable) {
		mapper69_irqcount--;
		if (mapper69_irqcount == 0xffff) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
			mapper69_irqenable = 0;
		}
	}
}

#undef mapper69_mirror
#undef mapper69_regnum
#undef mapper69_irqenable

// --[ mapper 70: (Kamen Rider Club)
static void mapper70_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper70_map()
{
	mapper_map_prg(16, 0, mapper_regs[0] >> 4);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, mapper_regs[0] & 0xf);
}

// --[ mapper 71: Camerica (Bee 52, Big Nose, Fire Hawk, Micro Machines, ...)
static void mapper71_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x9000) {
		mapper_regs[1] = ((data & 0x10) ? SINGLE_HIGH : SINGLE_LOW);
	} else {
		mapper_regs[0] = data;
	}

	mapper_map();
}

static void mapper71_map()
{
	mapper_map_prg(16, 0, mapper_regs[0]);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, 0);

	if (mapper_regs[1]) {
		// only fire hawk sets this, everything else uses hard-mirroring
		set_mirroring(mapper_regs[1]);
	}
}

// --[ mapper 73 - Konami vrc3 (Salamander)
// note: the flickery pixels on the top-right of HUD also happens on FC HW -dink
#define mapper73_irqenable	(mapper_regs[0x1f - 1])
#define mapper73_irqmode	(mapper_regs[0x1f - 2])
#define mapper73_irqreload  (mapper_regs[0x1f - 3])
#define mapper73_prg		(mapper_regs[0])
#define mapper73_irqcount	(mapper_regs16[0x1f - 0])
#define mapper73_irqlatch	(mapper_regs16[0x1f - 1])

static void mapper73_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000) {
		case 0x8000: mapper73_irqlatch = (mapper73_irqlatch & 0xfff0) | (data & 0xf); break;
		case 0x9000: mapper73_irqlatch = (mapper73_irqlatch & 0xff0f) | ((data & 0xf) << 4); break;
		case 0xa000: mapper73_irqlatch = (mapper73_irqlatch & 0xf0ff) | ((data & 0xf) << 8); break;
		case 0xb000: mapper73_irqlatch = (mapper73_irqlatch & 0x0fff) | ((data & 0xf) << 12); break;
		case 0xc000:
			mapper73_irqreload = data & 1;
			mapper73_irqenable = data & 2;
			mapper73_irqmode = data & 4;

			if (mapper73_irqenable) {
				if (mapper73_irqmode) {
					// when is this mode used? I can't make it happen -dink
					mapper73_irqcount = mapper73_irqlatch & 0xff;
				} else {
					mapper73_irqcount = mapper73_irqlatch;
				}
			}
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xd000:
			mapper73_irqenable = mapper73_irqreload;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xf000:
			mapper73_prg = data;
			break;
	}
	mapper_map();
}

static void mapper73_map()
{
	mapper_map_prg(16, 0, mapper73_prg);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, 0);
}

static void mapper73_cycle()
{
	if (mapper73_irqenable) {
		if ( (mapper73_irqmode && (mapper73_irqcount & 0xff) == 0xff) ||
			((mapper73_irqmode == 0) && mapper73_irqcount == 0xffff) ) {
			mapper73_irqcount = mapper73_irqlatch;
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		} else {
			mapper73_irqcount++;
		}
	}
}
#undef mapper73_irqenable
#undef mapper73_irqmode
#undef mapper73_irqreload
#undef mapper73_prg
#undef mapper73_irqcount
#undef mapper73_irqlatch

// --[ mapper 75: Konami VRC1 (King Kong 2, Tetsuwan Atom, ...)
#define mapper75_prg(x)     (mapper_regs[0 + x]) // 0 - 2
#define mapper75_chr(x)     (mapper_regs[3 + x]) // 3 - 4
#define mapper75_ext        (mapper_regs[8])
static void mapper75_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000) {
		case 0x8000: mapper75_prg(0) = data; break;
		case 0x9000: mapper75_ext = data; break;
		case 0xa000: mapper75_prg(1) = data; break;
		case 0xc000: mapper75_prg(2) = data; break;
		case 0xe000: mapper75_chr(0) = data; break;
		case 0xf000: mapper75_chr(1) = data; break;
	}

	mapper_map();
}

static void mapper75_map()
{
	mapper_map_prg( 8, 0, mapper75_prg(0));
	mapper_map_prg( 8, 1, mapper75_prg(1));
	mapper_map_prg( 8, 2, mapper75_prg(2));
	mapper_map_prg( 8, 3, -1);

	mapper_map_chr( 4, 0, (mapper75_chr(0) & 0xf) | ((mapper75_ext & 2) << 3));
	mapper_map_chr( 4, 1, (mapper75_chr(1) & 0xf) | ((mapper75_ext & 4) << 2));

	if (Cart.Mirroring != 4) {
		set_mirroring((mapper75_ext & 1) ? HORIZONTAL : VERTICAL);
	}
}
#undef mapper75_prg
#undef mapper75_chr
#undef mapper75_ext

// --[ mapper 85 (VRC7) Lagrange Point, Tiny Toon Adventures 2 (Japan)
#define mapper85_prg(x)		(mapper_regs[0x00 + x])
#define mapper85_chr(x)		(mapper_regs[0x03 + x])
#define mapper85_mirror		(mapper_regs[0x10])
#define mapper85_wramen     (mapper_regs[0x11])
#define mapper85_irqenable	(mapper_regs[0x12])
#define mapper85_irqrepeat	(mapper_regs[0x13])
#define mapper85_irqlatch	(mapper_regs[0x14])
#define mapper85_irqmode	(mapper_regs[0x15])
#define mapper85_irqcount	(mapper_regs16[0x1f - 0])
#define mapper85_irqcycle	(mapper_regs16[0x1f - 1])
#define vrc7_mute           (mapper_regs[0x16])

static void vrc7_map()
{
	mapper_map_prg( 8, 0, mapper85_prg(0));
	mapper_map_prg( 8, 1, mapper85_prg(1));
	mapper_map_prg( 8, 2, mapper85_prg(2));
	mapper_map_prg( 8, 3, -1);

	for (INT32 i = 0; i < 8; i++)
		mapper_map_chr( 1, i, mapper85_chr(i));

	switch (mapper85_mirror) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SINGLE_LOW); break;
		case 3: set_mirroring(SINGLE_HIGH); break;
	}

	if (mapper85_wramen) {
		NESMode &= ~NO_WORKRAM;
	} else {
		NESMode |= NO_WORKRAM;
	}
}

static void vrc7_write(UINT16 address, UINT8 data)
{
	if (address & 0x18) {
		// VRC7a uses xx10, VRC7b uses xxx8
		address = (address & ~0x0008) | 0x0010;
	}

	switch (address & 0xf030) {
		case 0x8000: mapper85_prg(0) = data & 0x3f; break;
		case 0x8010: mapper85_prg(1) = data & 0x3f; break;
		case 0x9000: mapper85_prg(2) = data & 0x3f; break;

		case 0x9010: BurnYM2413Write(0, data); break;
		case 0x9030: BurnYM2413Write(1, data); break;

		case 0xa000: mapper85_chr(0) = data; break;
		case 0xa010: mapper85_chr(1) = data; break;
		case 0xb000: mapper85_chr(2) = data; break;
		case 0xb010: mapper85_chr(3) = data; break;
		case 0xc000: mapper85_chr(4) = data; break;
		case 0xc010: mapper85_chr(5) = data; break;
		case 0xd000: mapper85_chr(6) = data; break;
		case 0xd010: mapper85_chr(7) = data; break;

		case 0xe000:
			mapper85_mirror = data & 0x3;
			mapper85_wramen = data & 0x80;
			vrc7_mute = data & 0x40;
			break;

		case 0xe010:
			mapper85_irqlatch = data;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xf000:
			mapper85_irqrepeat = data & 1;
			mapper85_irqenable = data & 2;
			mapper85_irqmode = data & 4;
			if (mapper85_irqenable) {
				mapper85_irqcycle = 0;
				mapper85_irqcount = mapper85_irqlatch;
			}
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
		case 0xf010:
			mapper85_irqenable = mapper85_irqrepeat;
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
	}

	mapper_map();
}

static void vrc7_cycle()
{
	if (mapper85_irqenable) {
		if (mapper85_irqmode) { // cycle mode
			mapper85_irqcount++;
			if (mapper85_irqcount >= 0x100) {
				M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
				mapper_irq(4);
				mapper85_irqcount = mapper85_irqlatch;
			}
		} else {
			mapper85_irqcycle += 3; // scanline mode
			if (mapper85_irqcycle >= 341) {
				mapper85_irqcycle -= 341;
				mapper85_irqcount++;
				if (mapper85_irqcount == 0x100) {
					M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
					mapper_irq(4);
					mapper85_irqcount = mapper85_irqlatch;
				}
			}
		}
	}
}
#undef mapper85_prg
#undef mapper85_chr
#undef mapper85_mirror
#undef mapper85_wramen
#undef mapper85_irqenable
#undef mapper85_irqrepeat
#undef mapper85_irqlatch
#undef mapper85_irqmode
#undef mapper85_irqcount
#undef mapper85_irqcycle

// --[ mapper 116: Somari (AV Girl Fighting)
// it's a frankensteinian mess-mapper -dink
#define mapper116_vrc2_prg(x)		(mapper_regs[0x00 + (x)])
#define mapper116_vrc2_chr(x)		(mapper_regs[0x02 + (x)])
#define mapper116_vrc2_mirror		(mapper_regs[0x0a])
#define mapper116_mode				(mapper_regs[0x0b])

#define mapper116_mmc3_banksel      (mapper_regs[0x0c])
#define mapper116_mmc3_mirror		(mapper_regs[0x0d])
#define mapper116_mmc3_irqlatch 	(mapper_regs[0x0e])
#define mapper116_mmc3_irqcount		(mapper_regs[0x0f])
#define mapper116_mmc3_irqenable	(mapper_regs[0x10])
#define mapper116_mmc3_irqreload	(mapper_regs[0x11])
#define mapper116_mmc3_regs(x)		(mapper_regs16[0x12 + (x)]) // must be regs16!

#define mapper116_mmc1_regs(x)		(mapper_regs[0x1b + (x)])
#define mapper116_mmc1_serialbyte	(mapper_regs16[0])
#define mapper116_mmc1_bitcount		(mapper_regs16[1])

static void mapper116_defaults()
{
	mapper116_vrc2_prg(0) = 0;
	mapper116_vrc2_prg(1) = 1;
	mapper116_vrc2_chr(0) = 0xff;
	mapper116_vrc2_chr(1) = 0xff;
	mapper116_vrc2_chr(2) = 0xff;
	mapper116_vrc2_chr(3) = 0xff;
	mapper116_vrc2_chr(4) = 4;
	mapper116_vrc2_chr(5) = 5;
	mapper116_vrc2_chr(6) = 6;
	mapper116_vrc2_chr(7) = 7;

	mapper116_mmc1_regs(0) = 0x0c;

	mapper116_mmc3_regs(0) = 0;
	mapper116_mmc3_regs(1) = 2;
	mapper116_mmc3_regs(2) = 4;
	mapper116_mmc3_regs(3) = 5;
	mapper116_mmc3_regs(4) = 6;
	mapper116_mmc3_regs(5) = 7;
	mapper116_mmc3_regs(6) = -4;
	mapper116_mmc3_regs(7) = -3;
	mapper116_mmc3_regs(8) = -2;
	mapper116_mmc3_regs(9) = -1;
}

static void mapper116_write(UINT16 address, UINT8 data)
{
	if (address < 0x8000) {
		if ((address & 0x4100) == 0x4100) {
			// Someri
			mapper116_mode = data;
			if (address & 1) {
				mapper116_mmc1_bitcount = 0;
				mapper116_mmc1_serialbyte = 0;
				mapper116_mmc1_regs(0) = 0x0c;
				mapper116_mmc1_regs(3) = 0x00;
			}
			mapper_map();
		}
	} else {
		if (address == 0xa131) {
			// Gouder SL-1632, mode & 2 == mmc3, ~mode & 2 == vrc2
			mapper116_mode = (data & ~3) | ((data & 2) >> 1);
		}
		switch (mapper116_mode & 3) {
			case 0:	{
				if (address >= 0xb000 && address <= 0xe003) {
					INT32 reg = ((((address & 2) | (address >> 10)) >> 1) + 2) & 7;

					if (~address & 1) {
						mapper116_vrc2_chr(reg) = (mapper116_vrc2_chr(reg) & 0xf0) | (data & 0x0f);
					} else {
						mapper116_vrc2_chr(reg) = (mapper116_vrc2_chr(reg) & 0x0f) | ((data & 0x0f) << 4);
					}
				} else {
					switch (address & 0xf000) {
						case 0x8000: mapper116_vrc2_prg(0) = data; break;
						case 0x9000: mapper116_vrc2_mirror = data & 1; break;
						case 0xa000: mapper116_vrc2_prg(1) = data; break;
					}
				}
				mapper_map();
			}
			break;
			case 1: {
				switch (address & 0xe001) {
					case 0x8000: mapper116_mmc3_banksel = data; break;
					case 0x8001: mapper116_mmc3_regs(mapper116_mmc3_banksel & 0x7) = data; break;
					case 0xa000: mapper116_mmc3_mirror = data & 1; break;
					case 0xc000: mapper116_mmc3_irqlatch = data; break;
					case 0xc001: mapper116_mmc3_irqreload = 1; break;
					case 0xe000: mapper116_mmc3_irqenable = 0; M6502SetIRQLine(0, CPU_IRQSTATUS_NONE); break;
					case 0xe001: mapper116_mmc3_irqenable = 1; break;
				}
				mapper_map();
			}
			break;
			case 2:
			case 3: {
				if (address & 0x8000) {
					if (data & 0x80) { // bit 7, mapper reset
						mapper116_mmc1_bitcount = 0;
						mapper116_mmc1_serialbyte = 0;
						mapper116_mmc1_regs(0) |= 0x0c;
						if (mapper_map) mapper_map();
					} else {
						mapper116_mmc1_serialbyte |= (data & 1) << mapper116_mmc1_bitcount;
						mapper116_mmc1_bitcount++;
						if (mapper116_mmc1_bitcount == 5) {
							UINT8 reg = (address >> 13) & 0x3;
							mapper116_mmc1_regs(reg) = mapper116_mmc1_serialbyte;
							mapper116_mmc1_bitcount = 0;
							mapper116_mmc1_serialbyte = 0;
							if (mapper_map) mapper_map();
						}
					}
				}
			}
			break;
		}
	}
}

static void mapper116_map()
{
	INT32 bank = (mapper116_mode & 4) << 6;
	switch (mapper116_mode & 3) {
		case 0: {
			mapper_map_chr( 1, 0, mapper116_vrc2_chr(0) | bank);
			mapper_map_chr( 1, 1, mapper116_vrc2_chr(1) | bank);
			mapper_map_chr( 1, 2, mapper116_vrc2_chr(2) | bank);
			mapper_map_chr( 1, 3, mapper116_vrc2_chr(3) | bank);
			mapper_map_chr( 1, 4, mapper116_vrc2_chr(4) | bank);
			mapper_map_chr( 1, 5, mapper116_vrc2_chr(5) | bank);
			mapper_map_chr( 1, 6, mapper116_vrc2_chr(6) | bank);
			mapper_map_chr( 1, 7, mapper116_vrc2_chr(7) | bank);

			mapper_map_prg( 8, 0, mapper116_vrc2_prg(0));
			mapper_map_prg( 8, 1, mapper116_vrc2_prg(1));
			mapper_map_prg( 8, 2, -2);
			mapper_map_prg( 8, 3, -1);

			set_mirroring((mapper116_vrc2_mirror) ? HORIZONTAL : VERTICAL);
		}
		break;
		case 1: {
			mapper_map_prg(8, ((mapper116_mmc3_banksel & 0x40) >> 5), mapper116_mmc3_regs(6));
			mapper_map_prg(8, 1, mapper116_mmc3_regs(7));
			mapper_map_prg(8, 2 ^ ((mapper116_mmc3_banksel & 0x40) >> 5), mapper116_mmc3_regs(8));
			mapper_map_prg(8, 3, mapper116_mmc3_regs(9));

			INT32 swap = (mapper116_mmc3_banksel & 0x80) >> 5;
			mapper_map_chr( 1, 0 ^ swap, (mapper116_mmc3_regs(0) & 0xfe) | bank);
			mapper_map_chr( 1, 1 ^ swap, (mapper116_mmc3_regs(0) | 0x01) | bank);
			mapper_map_chr( 1, 2 ^ swap, (mapper116_mmc3_regs(1) & 0xfe) | bank);
			mapper_map_chr( 1, 3 ^ swap, (mapper116_mmc3_regs(1) | 0x01) | bank);
			mapper_map_chr( 1, 4 ^ swap, mapper116_mmc3_regs(2) | bank);
			mapper_map_chr( 1, 5 ^ swap, mapper116_mmc3_regs(3) | bank);
			mapper_map_chr( 1, 6 ^ swap, mapper116_mmc3_regs(4) | bank);
			mapper_map_chr( 1, 7 ^ swap, mapper116_mmc3_regs(5) | bank);

			set_mirroring((mapper116_mmc3_mirror) ? HORIZONTAL : VERTICAL);
		}
		break;
		case 2:
		case 3: {
			if (mapper116_mmc1_regs(0) & 0x8) {
				if (mapper116_mmc1_regs(0) & 0x4) {
					mapper_map_prg(16, 0, mapper116_mmc1_regs(3) & 0xf);
					mapper_map_prg(16, 1, 0x0f);
				} else {
					mapper_map_prg(16, 0, 0);
					mapper_map_prg(16, 1, mapper116_mmc1_regs(3) & 0xf);
				}
			} else {
				mapper_map_prg(32, 0, (mapper116_mmc1_regs(3) & 0xf) >> 1);
			}

			if (mapper116_mmc1_regs(0) & 0x10) {
				mapper_map_chr( 4, 0, mapper116_mmc1_regs(1));
				mapper_map_chr( 4, 1, mapper116_mmc1_regs(2));
			} else {
				mapper_map_chr( 8, 0, mapper116_mmc1_regs(1) >> 1);
			}

			switch (mapper116_mmc1_regs(0) & 3) {
				case 0: set_mirroring(SINGLE_LOW); break;
				case 1: set_mirroring(SINGLE_HIGH); break;
				case 2: set_mirroring(VERTICAL); break;
				case 3: set_mirroring(HORIZONTAL); break;
			}
		}
		break;
	}
}

static void mapper14_map()
{
	switch (mapper116_mode & 3) {
		case 0: {
			mapper_map_chr( 1, 0, mapper116_vrc2_chr(0));
			mapper_map_chr( 1, 1, mapper116_vrc2_chr(1));
			mapper_map_chr( 1, 2, mapper116_vrc2_chr(2));
			mapper_map_chr( 1, 3, mapper116_vrc2_chr(3));
			mapper_map_chr( 1, 4, mapper116_vrc2_chr(4));
			mapper_map_chr( 1, 5, mapper116_vrc2_chr(5));
			mapper_map_chr( 1, 6, mapper116_vrc2_chr(6));
			mapper_map_chr( 1, 7, mapper116_vrc2_chr(7));

			mapper_map_prg( 8, 0, mapper116_vrc2_prg(0));
			mapper_map_prg( 8, 1, mapper116_vrc2_prg(1));
			mapper_map_prg( 8, 2, -2);
			mapper_map_prg( 8, 3, -1);

			set_mirroring((mapper116_vrc2_mirror) ? HORIZONTAL : VERTICAL);
		}
		break;
		case 1: {
			mapper_map_prg(8, ((mapper116_mmc3_banksel & 0x40) >> 5), mapper116_mmc3_regs(6));
			mapper_map_prg(8, 1, mapper116_mmc3_regs(7));
			mapper_map_prg(8, 2 ^ ((mapper116_mmc3_banksel & 0x40) >> 5), mapper116_mmc3_regs(8));
			mapper_map_prg(8, 3, mapper116_mmc3_regs(9));

			INT32 swap = (mapper116_mmc3_banksel & 0x80) >> 5;
			INT32 bank0 = (mapper116_mode & 0x08) << 5;
			INT32 bank1 = (mapper116_mode & 0x20) << 3;
			INT32 bank2 = (mapper116_mode & 0x80) << 1;
			mapper_map_chr( 1, 0 ^ swap, (mapper116_mmc3_regs(0) & 0xfe) | bank0);
			mapper_map_chr( 1, 1 ^ swap, (mapper116_mmc3_regs(0) | 0x01) | bank0);
			mapper_map_chr( 1, 2 ^ swap, (mapper116_mmc3_regs(1) & 0xfe) | bank0);
			mapper_map_chr( 1, 3 ^ swap, (mapper116_mmc3_regs(1) | 0x01) | bank0);
			mapper_map_chr( 1, 4 ^ swap, mapper116_mmc3_regs(2) | bank1);
			mapper_map_chr( 1, 5 ^ swap, mapper116_mmc3_regs(3) | bank1);
			mapper_map_chr( 1, 6 ^ swap, mapper116_mmc3_regs(4) | bank2);
			mapper_map_chr( 1, 7 ^ swap, mapper116_mmc3_regs(5) | bank2);

			set_mirroring((mapper116_mmc3_mirror) ? HORIZONTAL : VERTICAL);
		}
		break;
	}
}

static void mapper116_mmc3_scanline()
{
	if ((mapper116_mode & 0x03) != 1) return;

	if (mapper116_mmc3_irqcount == 0 || mapper116_mmc3_irqreload) {
		mapper116_mmc3_irqreload = 0;
		mapper116_mmc3_irqcount = mapper116_mmc3_irqlatch;
	} else {
		mapper116_mmc3_irqcount--;
	}

	if (mapper116_mmc3_irqcount == 0 && mapper116_mmc3_irqenable && RENDERING) {
		mapper_irq(0);
	}
}
#undef mapper116_vrc2_prg
#undef mapper116_vrc2_chr
#undef mapper116_vrc2_mirror
#undef mapper116_mode

#undef mapper116_mmc3_banksel
#undef mapper116_mmc3_mirror
#undef mapper116_mmc3_irqlatch
#undef mapper116_mmc3_irqcount
#undef mapper116_mmc3_irqenable
#undef mapper116_mmc3_irqreload
#undef mapper116_mmc3_regs

#undef mapper116_mmc1_regs
#undef mapper116_mmc1_serialbyte
#undef mapper116_mmc1_bitcount

// --[ mapper 80: Taito X1-005
// --[ mapper 207: with advanced mirroring
#define mapper207_prg(x)     (mapper_regs[0 + x]) // 0 - 2
#define mapper207_chr(x)     (mapper_regs[3 + x]) // 3 - 9
#define mapper207_mirror     (mapper_regs[0x1f - 0])
#define mapper207_ram_en     (mapper_regs[0x1f - 1])
#define mapper207_adv_mirror (mapper_regs[0x1f - 2]) // mapper 207 yes, 80 no

static UINT8 mapper80_ram[0x80];

static void mapper207_scan()
{
	SCAN_VAR(mapper80_ram);
}

static UINT8 mapper207_read(UINT16 address)
{
	if (address >= 0x7f00 && address <= 0x7fff) {
		if (mapper207_ram_en == 0xa3) {
			return mapper80_ram[address & 0x7f];
		}
	}
	return 0xff;
}

static void mapper207_write(UINT16 address, UINT8 data)
{
	if (address >= 0x7f00 && address <= 0x7fff) {
		if (mapper207_ram_en == 0xa3) {
			mapper80_ram[address & 0x7f] = data;
		}
	}

	switch (address) {
		case 0x7ef0: mapper207_chr(0) = data; break;
		case 0x7ef1: mapper207_chr(1) = data; break;
		case 0x7ef2: mapper207_chr(2) = data; break;
		case 0x7ef3: mapper207_chr(3) = data; break;
		case 0x7ef4: mapper207_chr(4) = data; break;
		case 0x7ef5: mapper207_chr(5) = data; break;
		case 0x7ef6:
		case 0x7ef7: mapper207_mirror = data & 1; break; // for mapper80
		case 0x7ef8:
		case 0x7ef9: mapper207_ram_en = data; break; // for mapper80
		case 0x7efa:
		case 0x7efb: mapper207_prg(0) = data; break;
		case 0x7efc:
		case 0x7efd: mapper207_prg(1) = data; break;
		case 0x7efe:
		case 0x7eff: mapper207_prg(2) = data; break;
	}

	mapper_map();
}

static void mapper207_map()
{
	mapper_map_prg( 8, 0, mapper207_prg(0));
	mapper_map_prg( 8, 1, mapper207_prg(1));
	mapper_map_prg( 8, 2, mapper207_prg(2));
	mapper_map_prg( 8, 3, -1);

	mapper_map_chr( 2, 0, (mapper207_chr(0) >> 1) & 0x3f);
	mapper_map_chr( 2, 1, (mapper207_chr(1) >> 1) & 0x3f);
	mapper_map_chr( 1, 4, mapper207_chr(2));
	mapper_map_chr( 1, 5, mapper207_chr(3));
	mapper_map_chr( 1, 6, mapper207_chr(4));
	mapper_map_chr( 1, 7, mapper207_chr(5));

	if (mapper207_adv_mirror == 0) {
		set_mirroring((mapper207_mirror & 1) ? VERTICAL : HORIZONTAL);
	} else {
		nametable_mapall((mapper207_chr(0) >> 7) & 1, (mapper207_chr(0) >> 7) & 1, (mapper207_chr(1) >> 7) & 1, (mapper207_chr(1) >> 7) & 1);
	}
}
#undef mapper207_prg
#undef mapper207_chr
#undef mapper207_mirror
#undef mapper207_ram_en

// ---[ mapper 81 NTDEC Super Gun (Caltron)
static void mapper81_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		mapper_regs[0] = data;
		mapper_map();
	}
}

static void mapper81_map()
{
    mapper_map_prg(16, 0, (mapper_regs[0] >> 2) & 0x03);
    mapper_map_prg(16, 1, -1);
    mapper_map_chr( 8, 0, mapper_regs[0] & 0x03);
}

// --[ mapper 82: Taito X1-017
#define mapper82_prg(x)     (mapper_regs[0 + (x)]) // 0 - 2
#define mapper82_chr(x)     (mapper_regs[3 + (x)]) // 3 - 8
#define mapper82_mirror     (mapper_regs[0x1f - 0])
#define mapper82_a12inv     (mapper_regs[0x1f - 1])
#define mapper82_ram_en(x)  (mapper_regs[9 + (x)])

static UINT8 mapper82_read(UINT16 address)
{
	if ( ((address >= 0x6000 && address <= 0x67ff) && mapper82_ram_en(0)) ||
		 ((address >= 0x6800 && address <= 0x6fff) && mapper82_ram_en(1)) ||
		 ((address >= 0x7000 && address <= 0x73ff) && mapper82_ram_en(2))
	   )
		return Cart.WorkRAM[(address & 0x1fff)];

	return cpu_open_bus;
}


static void mapper82_write(UINT16 address, UINT8 data)
{
	cart_exp_write_abort = 1; // don't fall-through after callback!

	if ( ((address >= 0x6000 && address <= 0x67ff) && mapper82_ram_en(0)) ||
		 ((address >= 0x6800 && address <= 0x6fff) && mapper82_ram_en(1)) ||
		 ((address >= 0x7000 && address <= 0x73ff) && mapper82_ram_en(2))
	   )
		Cart.WorkRAM[(address & 0x1fff)] = data;

	switch (address) {
		case 0x7ef0: mapper82_chr(0) = data; break;
		case 0x7ef1: mapper82_chr(1) = data; break;
		case 0x7ef2: mapper82_chr(2) = data; break;
		case 0x7ef3: mapper82_chr(3) = data; break;
		case 0x7ef4: mapper82_chr(4) = data; break;
		case 0x7ef5: mapper82_chr(5) = data; break;
		case 0x7ef6: mapper82_mirror = data & 1; mapper82_a12inv = data & 2; break;
		case 0x7ef7: mapper82_ram_en(0) = (data == 0xca); break;
		case 0x7ef8: mapper82_ram_en(1) = (data == 0x69); break;
		case 0x7ef9: mapper82_ram_en(2) = (data == 0x84); break;
		case 0x7efa: mapper82_prg(0) = data >> 2; break;
		case 0x7efb: mapper82_prg(1) = data >> 2; break;
		case 0x7efc: mapper82_prg(2) = data >> 2; break;
		case 0x7efd: // unused irq latch
		case 0x7efe: // unused irq control
		case 0x7eff: // unused irq ack
			break;
	}

	mapper_map();
}

static void mapper82_map()
{
	mapper_map_prg( 8, 0, mapper82_prg(0));
	mapper_map_prg( 8, 1, mapper82_prg(1));
	mapper_map_prg( 8, 2, mapper82_prg(2));
	mapper_map_prg( 8, 3, -1);

	if (mapper82_a12inv == 0) {
		mapper_map_chr( 2, 0, (mapper82_chr(0) >> 1) & 0x7f);
		mapper_map_chr( 2, 1, (mapper82_chr(1) >> 1) & 0x7f);
		mapper_map_chr( 1, 4, mapper82_chr(2));
		mapper_map_chr( 1, 5, mapper82_chr(3));
		mapper_map_chr( 1, 6, mapper82_chr(4));
		mapper_map_chr( 1, 7, mapper82_chr(5));
	} else {
		mapper_map_chr( 2, 2, (mapper82_chr(0) >> 1) & 0x7f);
		mapper_map_chr( 2, 3, (mapper82_chr(1) >> 1) & 0x7f);
		mapper_map_chr( 1, 0, mapper82_chr(2));
		mapper_map_chr( 1, 1, mapper82_chr(3));
		mapper_map_chr( 1, 2, mapper82_chr(4));
		mapper_map_chr( 1, 3, mapper82_chr(5));
	}

	set_mirroring((mapper82_mirror) ? VERTICAL : HORIZONTAL);
}
#undef mapper82_prg
#undef mapper82_chr
#undef mapper82_mirror
#undef mapper82_a12inv
#undef mapper82_ram_en


// --[ mapper 232: Camerica (Quattro Adventure, Arcade, Sports)
static void mapper232_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8000 && address <= 0xbfff) {
		mapper_regs[0] = data;
	}

	if (address >= 0xc000 && address <= 0xffff) {
		mapper_regs[1] = data;
	}

	mapper_map();
}

static void mapper232_map()
{
	UINT32 block = (mapper_regs[0] & 0x18) >> 1;
	mapper_map_prg(16, 0, block | (mapper_regs[1] & 3));
	mapper_map_prg(16, 1, block | 3);
	mapper_map_chr( 8, 0, 0);
}

// --[ mapper 77: Irem? Napoleon Senki
static void mapper77_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper77_map()
{
	mapper_map_prg(32, 0, mapper_regs[0] & 0x0f);
	mapper_map_chr_ramrom( 2, 0, (mapper_regs[0] >> 4) & 0x0f, MEM_ROM);
	mapper_map_chr_ramrom( 2, 1, 1, MEM_RAM);
	mapper_map_chr_ramrom( 2, 2, 2, MEM_RAM);
	mapper_map_chr_ramrom( 2, 3, 3, MEM_RAM);
}

// --[ mapper 78: Irem? (Holy Diver, Uchuusen: Cosmo Carrier)
static void mapper78_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper78_map()
{
	mapper_map_prg(16, 0, mapper_regs[0] & 7);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, mapper_regs[0] >> 4);
	if (Cart.Mirroring == 4) // Holy Diver
		set_mirroring(((mapper_regs[0] >> 3) & 1) ? VERTICAL : HORIZONTAL);
	else
		set_mirroring(((mapper_regs[0] >> 3) & 1) ? SINGLE_HIGH : SINGLE_LOW);
}

// --[ mapper 32: Irem G101 (Image Fight, Kaiketsu Yanchamaru 2, Major League, Meikyuu Jima, ...)
#define mapper32_mirror  (mapper_regs[0x1f - 0])
#define mapper32_prg(x)  (mapper_regs[0 + x])
#define mapper32_chr(x)  (mapper_regs[2 + (x)])

static void mapper32_write(UINT16 address, UINT8 data)
{
	switch (address & 0xf000) {
		case 0x8000:
			mapper32_prg(0) = data;
			break;
		case 0x9000:
			mapper32_mirror = data;
			break;
		case 0xa000:
			mapper32_prg(1) = data;
			break;
		case 0xb000:
			mapper32_chr((address & 7)) = data;
			break;
	}
	mapper_map();
}

static void mapper32_map()
{
	mapper_map_prg( 8, 0 ^ (mapper32_mirror & 2), mapper32_prg(0));
	mapper_map_prg( 8, 1, mapper32_prg(1));
	mapper_map_prg( 8, 2 ^ (mapper32_mirror & 2), -2);
	mapper_map_prg( 8, 3, -1);

	for (INT32 i = 0; i < 8; i++)
		mapper_map_chr( 1, i, mapper32_chr(i));

	if (Cart.Crc == 0xd8dfd3d1) {
		// Major League (Japan) hardwired to SINGLE_LOW
		set_mirroring(SINGLE_LOW);
	} else {
		set_mirroring((mapper32_mirror & 1) ? HORIZONTAL : VERTICAL);
	}
}

#undef mapper32_mirror
#undef mapper32_prg
#undef mapper32_chr

// --[ mapper 34: BNROM, NINA-001 (Deadly Towers, Darkseed) (Impossible Mission 2)
static void mapper34_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8000) { // BNROM
		mapper_regs[0] = data;
	} else {
		switch (address) {   // NINA-001
			case 0x7ffd:
			case 0x7ffe:
			case 0x7fff:
				mapper_regs[(address & 0x3) - 1] = data;
				break;
		}
	}
	mapper_map();
}

static void mapper34_map()
{
	mapper_map_prg(32, 0, mapper_regs[0]);

	mapper_map_chr( 4, 0, mapper_regs[1]);
	mapper_map_chr( 4, 1, mapper_regs[2]);
}

// --[ mapper 87: Jaleco (Argus, Ninja JajaMaru Kun, City Connection)
static void mapper87_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = ((data >> 1) & 1) | ((data << 1) & 2);
	mapper_map();
}

static void mapper87_map()
{
	mapper_map_prg(32, 0, 0);
	mapper_map_chr( 8, 0, mapper_regs[0]);
}

// --[ mapper 88,154: Namcot (Quinty, Dragon Spirit..)
#define mapper88_cmd	(mapper_regs[0x1f - 0])
#define mapper88_mirror	(mapper_regs[0x1f - 1])
#define mapper154       (mapper_regs[0x1f - 2])

static void mapper88_write(UINT16 address, UINT8 data)
{
	switch (address & 0x8001) {
		case 0x8000:
			mapper88_cmd = data & 7;
			mapper88_mirror = (data >> 6) & 1;
			break;
		case 0x8001:
			mapper_regs[mapper88_cmd] = data;
			break;
	}

	mapper_map();
}

static void mapper88_map()
{
	mapper_map_chr( 2, 0, (mapper_regs[0] >> 1) & 0x1f);
	mapper_map_chr( 2, 1, (mapper_regs[1] >> 1) & 0x1f);

	mapper_map_chr( 1, 4, mapper_regs[2] | 0x40);
	mapper_map_chr( 1, 5, mapper_regs[3] | 0x40);
	mapper_map_chr( 1, 6, mapper_regs[4] | 0x40);
	mapper_map_chr( 1, 7, mapper_regs[5] | 0x40);

	mapper_map_prg( 8, 0, mapper_regs[6]);
	mapper_map_prg( 8, 1, mapper_regs[7]);
	mapper_map_prg( 8, 2, -2);
	mapper_map_prg( 8, 3, -1);

	if (mapper154) {
		switch (mapper88_mirror) {
			case 0: set_mirroring(SINGLE_LOW); break;
			case 1: set_mirroring(SINGLE_HIGH); break;
		}
	}
}
#undef mapper88_cmd
#undef mapper88_mirror

// --[ mapper 89: (Tenka no Goikenban: Mito Koumon)
static void mapper89_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper89_map()
{
	mapper_map_prg(16, 0, (mapper_regs[0] >> 4) & 7);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, (mapper_regs[0] & 7) | ((mapper_regs[0] >> 4) & 8));
	set_mirroring((mapper_regs[0] & 8) ? SINGLE_HIGH : SINGLE_LOW);
}

// --[ mapper 93: Sunsoft-1/2 PCB: Fantasy Zone, Shanghai
static void mapper93_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper93_map()
{
	mapper_map_prg(16, 0, mapper_regs[0] >> 4);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, 0);
}

// --[ mapper 94: Senjou no Ookami (Commando)
static void mapper94_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper94_map()
{
	mapper_map_prg(16, 0, mapper_regs[0] >> 2);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, 0);
}

// --[ mapper 97: Kaiketsu Yanchamaru
static void mapper97_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper97_map()
{
	mapper_map_prg(16, 0, -1);
	mapper_map_prg(16, 1, mapper_regs[0] & 0xf);
	mapper_map_chr( 8, 0, ((mapper_regs[0] >> 1) & 1) | ((mapper_regs[0] << 1) & 2));
	set_mirroring((mapper_regs[0] & 0x80) ? VERTICAL : HORIZONTAL);
}

// --[ mapper 107: (Magic Dragon)
static void mapper107_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper107_map()
{
	mapper_map_prg(32, 0, (mapper_regs[0] >> 1) & 3);
	mapper_map_chr( 8, 0, mapper_regs[0] & 7);
}

// --[ mapper 101: (Urusei Yatsura - Lum no Wedding Bell (Japan))
static void mapper101_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper101_map()
{
	mapper_map_prg(32, 0, 0);
	mapper_map_chr( 8, 0, mapper_regs[0]);
}

// --[ mapper 112: ASDER (Huang Di, ...)
#define mapper112_mirror		(mapper_regs[0x1f - 0])
#define mapper112_regnum		(mapper_regs[0x1f - 1])
#define mapper112_chrbank		(mapper_regs[0x1f - 2])

static void mapper112_write(UINT16 address, UINT8 data)
{
	switch (address & 0xe001) {
		case 0x8000: mapper112_regnum = data & 7; break;
		case 0xa000: mapper_regs[mapper112_regnum] = data; break;
		case 0xc000: mapper112_chrbank = data; break;
		case 0xe000: mapper112_mirror = data & 1; break;
	}

	mapper_map();
}

static void mapper112_map()
{
	mapper_map_prg( 8, 0, mapper_regs[0]);
	mapper_map_prg( 8, 1, mapper_regs[1]);

	mapper_map_chr( 2, 0, mapper_regs[2] >> 1);
	mapper_map_chr( 2, 1, mapper_regs[3] >> 1);

	mapper_map_chr( 1, 4, ((mapper112_chrbank & 0x10) << 4) | mapper_regs[4]);
	mapper_map_chr( 1, 5, ((mapper112_chrbank & 0x20) << 3) | mapper_regs[5]);
	mapper_map_chr( 1, 6, ((mapper112_chrbank & 0x40) << 2) | mapper_regs[6]);
	mapper_map_chr( 1, 7, ((mapper112_chrbank & 0x80) << 1) | mapper_regs[7]);

	set_mirroring(mapper112_mirror ? HORIZONTAL : VERTICAL);
}

// --[ mapper 113 NINA-03/NINA-06 ext. (Papillon Gals) / Mojon Twins Multicart
static void mapper113_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper113_map()
{
	mapper_map_prg(32, 0, (mapper_regs[0] >> 3) & 7);
	mapper_map_chr( 8, 0, ((mapper_regs[0] >> 3) & 8) | (mapper_regs[0] & 7));
	set_mirroring((mapper_regs[0] & 0x80) ? VERTICAL : HORIZONTAL);
}

// --[ mapper 79, 146: NINA-03/NINA-06 (Death Bots), Sachen (146 / Metal Fighter)
static void mapper79_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper79_map()
{
	mapper_map_prg(32, 0, (mapper_regs[0] >> 3) & 1);
	mapper_map_chr( 8, 0, (mapper_regs[0] & 7));
}

// --[ mapper 137, 138, 139, 141: Sachen 8259x (Super Pang, etc)
static void mapper8259_write(UINT16 address, UINT8 data)
{
	if (address < 0x4100) return;

	if ((address & 0x4101) == 0x4100) {
		mapper_regs[8] = data; // command
	} else {
		mapper_regs[mapper_regs[8] & 7] = data;
		mapper_map();
	}
}

static void mapper8259_map()
{
	mapper_map_prg(32, 0, mapper_regs[5] & 7);

	if (Cart.CHRRomSize != 0) {
		for (INT32 i = 0; i < 4; i++) {
			INT32 bank = 0;

			if (mapper_regs[7] & 1)
				bank = (mapper_regs[0] & 7) | ((mapper_regs[4] & 7) << 3);
			else
				bank = (mapper_regs[i] & 7) | ((mapper_regs[4] & 7) << 3);

			switch (mapper_regs[0x1f]) {
				case 137:                         // 137 8259D
					bank = mapper_regs[i] & 7;
					switch (i & 3) {
						case 1:
							bank |= (mapper_regs[4] & 1) << 4;
							break;
						case 2:
							bank |= (mapper_regs[4] & 2) << 3;
							break;
						case 3:
							bank |= ((mapper_regs[4] & 4) << 2) | ((mapper_regs[6] & 1) << 3);
							break;
					}
					mapper_map_chr( 1, i, bank);
					mapper_map_chr( 4, 1, -1);
					break;
				case 138:                         // 138 8259B
					mapper_map_chr( 2, i, bank);
					break;
				case 139:
					bank = (bank << 2) | (i & 3); // 139 8259C
					mapper_map_chr( 2, i, bank);
					break;
				case 141:
					bank = (bank << 1) | (i & 1); // 141 8259A
					mapper_map_chr( 2, i, bank);
					break;
			}
		}
	}

	if (mapper_regs[7] & 1) {
		set_mirroring(VERTICAL);
	} else {
		switch ((mapper_regs[7] >> 1) & 3) {
			case 0: set_mirroring(VERTICAL); break;
			case 1: set_mirroring(HORIZONTAL); break;
			case 2: set_mirroring(SACHEN); break;
			case 3: set_mirroring(SINGLE_LOW); break;
		}
	}
}

// --[ mapper 150: Sachen 74LS374XX (Tasac)
#define mapper150_cmd       (mapper_regs[0x1f - 0])
#define mapper150_prg       (mapper_regs[0x1f - 1])

static void mapper150_write(UINT16 address, UINT8 data)
{
	switch (address & 0xc101) {
		case 0x4100:
			mapper150_cmd = data & 0x7;
			break;
		case 0x4101:
			switch (mapper150_cmd) {
				case 2:
					mapper150_prg = data & 1;
					break;
				case 5:
					mapper150_prg = data & 7;
					break;
				default:
					mapper_regs[mapper150_cmd] = data;
					break;
			}
			mapper_map();
			break;
	}
}

static UINT8 mapper150_read(UINT16 address)
{
	if ((address & 0xc101) == 0x4000) {
		return (~mapper150_cmd) ^ 1; // '1' (or 0) is switch for Sachen / Sachen Hacker
	}

	return 0;
}

static void mapper150_map()
{
	mapper_map_prg(32, 0, mapper150_prg);

	mapper_map_chr( 8, 0, ((mapper_regs[2] & 0x1) << 3) | ((mapper_regs[4] & 0x1) << 2) | (mapper_regs[6] & 0x03));

	switch ((mapper_regs[7] >> 1) & 3) {
		case 0: set_mirroring(VERTICAL); break;
		case 1: set_mirroring(HORIZONTAL); break;
		case 2: set_mirroring(SACHEN); break;
		case 3: set_mirroring(SINGLE_LOW); break;
	}
}

// ---[ mapper 162 Zelda Triforce of the Gods
#define mapper162_reg(x)			(mapper_regs[0x00 + (x)])

static void mapper162_write(UINT16 address, UINT8 data)
{
	if (address >= 0x5000 && address <= 0x5fff) {
		mapper162_reg((address >> 8) & 0x03) = data;

		mapper_map();
	}
}

static void mapper162_map()
{
	switch (mapper162_reg(3) & 0x05) {
		case 0:
			mapper_map_prg(32, 0, (mapper162_reg(1) & 0x02) | ((mapper162_reg(2) & 0x0f) << 4) | (mapper162_reg(0) & 0x0c));
			break;
		case 1:
			mapper_map_prg(32, 0, ((mapper162_reg(2) & 0x0f) << 4) | (mapper162_reg(0) & 0x0c));
			break;
		case 4:
			mapper_map_prg(32, 0, ((mapper162_reg(1) & 0x02) >> 1) | ((mapper162_reg(2) & 0x0f) << 4) | (mapper162_reg(0) & 0x0e));
			break;
		case 5:
			mapper_map_prg(32, 0, ((mapper162_reg(2) & 0x0f) << 4) | (mapper162_reg(0) & 0x0f));
			break;
	}
}

// ---[ mapper 163 Final Fantasy VII (NJ063) + Advent Children ver. (&etc.)
#define mapper163_reg(x)			(mapper_regs[0x00 + (x)])
#define mapper163_chr(x)			(mapper_regs[0x08 + (x)])
#define mapper163_prg				(mapper_regs[0x1f - 0])
#define mapper163_prot_flip			(mapper_regs[0x1f - 1])
#define mapper163_raster_split		(mapper_regs[0x1f - 2])

static UINT8 mapper163_read(UINT16 address)
{
	switch (address & 0x7700) {
		case 0x5100:
			return mapper163_reg(3) | mapper163_reg(1) | mapper163_reg(0) | (mapper163_reg(2) ^ 0xff);
		case 0x5500:
			return (mapper163_prot_flip == 0) ? (mapper163_reg(3) | mapper163_reg(0)) : 0;
	}

	return 0x4;
}

static void mapper163_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0x5000) {
		if (address == 0x5101) {
			if (mapper163_reg(4) != 0 && data == 0) {
				mapper163_prot_flip ^= 1;
			}
			mapper163_reg(4) = data;
		} else if (address == 0x5100 && data == 0x6) {
			mapper163_prg = 0x3;
		} else {
			switch (address & 0x7300) {
				case 0x5000:
					mapper163_reg(0) = data;
					if (!(data & 0x80) && scanline < 128) {
						mapper163_chr(0) = 0;
						mapper163_chr(1) = 1;
					}
					mapper163_raster_split = data & 0x80;
					mapper163_prg = (mapper163_reg(0) & 0xf) | ((mapper163_reg(2) & 0xf) << 4);
					break;
				case 0x5100:
					mapper163_reg(1) = data;
					if (data == 0x6) {
						mapper163_prg = 3;
					}
					break;
				case 0x5200:
					mapper163_reg(2) = data;
					mapper163_prg = (mapper163_reg(0) & 0xf) | ((mapper163_reg(2) & 0xf) << 4);
					break;
				case 0x5300:
					mapper163_reg(3) = data;
					break;
			}
		}
		mapper_map();
	}
}

static void mapper163_map()
{
	mapper_map_prg(32, 0, mapper163_prg);
	mapper_map_chr(4, 0, mapper163_chr(0));
	mapper_map_chr(4, 1, mapper163_chr(1));
}

static void mapper163_cycle(UINT16 address)
{
	static INT32 last_127 = 0;
	static INT32 last_239 = 0;

	if (mapper163_raster_split && pixel > 257) {
		switch (scanline) {
			case 127:
				if (nCurrentFrame != last_127) {
					mapper_map_chr(4, 0, 1);
					mapper_map_chr(4, 1, 1);
					mapper163_chr(0) = 1;
					mapper163_chr(1) = 1;
				}
				last_127 = nCurrentFrame;
				break;
			case 239:
				if (nCurrentFrame != last_239) {
					mapper_map_chr(4, 0, 0);
					mapper_map_chr(4, 1, 0);
					mapper163_chr(0) = 0;
					mapper163_chr(1) = 0;
				}
				last_239 = nCurrentFrame;
				break;
		}
	}
}

#undef mapper163_reg
#undef mapper163_prg
#undef mapper163_chr
#undef mapper163_prot_flip
#undef mapper163_raster_split

// --[ mapper 38: (Crime Busters)
static void mapper38_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper38_map()
{
	mapper_map_prg(32, 0, mapper_regs[0] & 0x3);
	mapper_map_chr( 8, 0, (mapper_regs[0] >> 2) & 0x3);
}

// --[ mapper 66,140: (Bio Senshi Dan: Increaser tono Tatakai, Youkai Club, ..)
static void mapper140_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper140_map()
{
	mapper_map_prg(32, 0, mapper_regs[0] >> 4);
	mapper_map_chr( 8, 0, mapper_regs[0] & 0xf);
}

// --[ mapper 11, 144: ColorDreams, Death Race(144)
static void mapper11_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper144_write(UINT16 address, UINT8 data)
{
	if (address >= 0x8001) {
		mapper_regs[0] = data;
		mapper_map();
	}
}

static void mapper11_map()
{
	mapper_map_prg(32, 0, mapper_regs[0] & 0xf);
	mapper_map_chr( 8, 0, mapper_regs[0] >> 4);
}

// --[ mapper 471: haratyler, haraforce
static void mapper471_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		mapper_regs[0] = address & 1;

		M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);

		mapper_map();
	}
}

static void mapper471_map()
{
	mapper_map_prg(32, 0, mapper_regs[0] & 0x1);
	mapper_map_chr( 8, 0, mapper_regs[0] & 0x1);
}

static void mapper471_scanline()
{
	mapper_irq(0);
}

// --[ mapper 152: (Arkanoid II, Pocket Zaurus: Juu Ouken no Nazo, ...)
static void mapper152_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper152_map()
{
	mapper_map_prg(16, 0, (mapper_regs[0] >> 4) & 7);
	mapper_map_prg(16, 1, -1);
	mapper_map_chr( 8, 0, mapper_regs[0] & 0xf);
	set_mirroring((mapper_regs[0] & 0x80) ? SINGLE_HIGH : SINGLE_LOW);
}

// --[ mapper 156: Open (Metal Force, Buzz & Waldog, Koko Adv., Janggun-ui Adeul)
#define mapper156_chr_lo(x)     (mapper_regs[0 + (x)])  // x = 0 - 7
#define mapper156_chr_hi(x)     (mapper_regs[8 + (x)])  // x = 0 - 7
#define mapper156_prg           (mapper_regs[0x1f - 0])
#define mapper156_mirror        (mapper_regs[0x1f - 1])

static void mapper156_write(UINT16 address, UINT8 data)
{
	switch (address) {
		case 0xc000:
		case 0xc001:
		case 0xc002:
		case 0xc003:
		case 0xc008:
		case 0xc009:
		case 0xc00a:
		case 0xc00b:
			mapper156_chr_lo((address & 0x3) | ((address & 0x8) >> 1)) = data;
			break;
		case 0xc004:
		case 0xc005:
		case 0xc006:
		case 0xc007:
		case 0xc00c:
		case 0xc00d:
		case 0xc00e:
		case 0xc00f:
			mapper156_chr_hi((address & 0x3) | ((address & 0x8) >> 1)) = data;
			break;
		case 0xc010:
			mapper156_prg = data;
			break;
		case 0xc014:
			mapper156_mirror = 0x10 | (data & 1);
			break;
	}

	mapper_map();
}

static void mapper156_map()
{
	mapper_map_prg(16, 0, mapper156_prg);
	mapper_map_prg(16, 1, -1);

	for (INT32 i = 0; i < 8; i++) {
		mapper_map_chr( 1, i, (mapper156_chr_hi(i) << 8) | mapper156_chr_lo(i));
	}

	switch (mapper156_mirror) {
		case 0: set_mirroring(SINGLE_LOW); break;
		case 0x10: set_mirroring(VERTICAL); break;
		case 0x11: set_mirroring(HORIZONTAL); break;
	}
}

// --[ mapper 180: Crazy Climber
static void mapper180_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper180_map()
{
	mapper_map_prg(16, 0, 0);
	mapper_map_prg(16, 1, mapper_regs[0] & 0x7);
	mapper_map_chr( 8, 0, 0);
}

// --[ mapper 184: Atlantis no Nazo, Kanshakudama Nage Kantarou no Toukaidou Gojuusan Tsugi, Wing of Madoola
static void mapper184_write(UINT16 address, UINT8 data)
{
	mapper_regs[0] = data;
	mapper_map();
}

static void mapper184_map()
{
	mapper_map_prg(32, 0, 0);
	mapper_map_chr( 4, 0, mapper_regs[0] & 0xf);
	mapper_map_chr( 4, 1, mapper_regs[0] >> 4);
}

// --[ mapper 185 CNROM+prot
static void mapper185_write(UINT16 address, UINT8 data)
{
	if (address & 0x8000) {
		mapper_regs[0] = data;
		mapper_map();
	}
}

static void mapper185_map()
{
	if ( (mapper_regs[0] & 3) && (mapper_regs[0] != 0x13) ) { // map rom
		mapper_map_chr_ramrom( 8, 0, 0, MEM_ROM);
	} else { // map ram
		mapper_map_chr_ramrom( 8, 0, 0, MEM_RAM);
	}
}

// --[ end mappers

static void mapper_reset()
{
	mapper_init(Cart.Mapper);
}

static void mapper_scan(INT32 nAction, INT32 *pnMin)
{
	SCAN_VAR(PRGMap);
	SCAN_VAR(CHRMap);
	SCAN_VAR(CHRType);
	SCAN_VAR(PRGExpMap);

	SCAN_VAR(mapper_regs);
	SCAN_VAR(mapper_regs16);
	SCAN_VAR(mapper_irq_exec);

	if (mapper_scan_cb) {
		mapper_scan_cb();
	}

	if (nAction & ACB_WRITE) {
		if (mapper_map) mapper_map();
	}
}

static INT32 mapper_init(INT32 mappernum)
{
	INT32 retval = -1;

	if (NESMode & MAPPER_NOCLEAR && RESETMode == RESET_BUTTON) {
		// do nothing! certain mappers need regs preserved on RESET_BUTTON
	} else {
		memset(mapper_regs, 0, sizeof(mapper_regs));
		memset(mapper_regs16, 0, sizeof(mapper_regs16));
	}

	mapper_write = NULL; // 8000 - ffff
	mapper_map = NULL;
	mapper_scanline = NULL; // scanline cb
	mapper_cycle = NULL; // cycle cb (1x per cycle)
	mapper_ppu_clock = NULL; // called after busaddress change (see mapper09) (only in visible & prerender!)
	mapper_ppu_clockall = NULL; // called every ppu clock
	mapper_scan_cb = NULL; // savestate cb (see vrc6)
	mapper_scan_cb_nvram = NULL; // savestate cb (nvram)

	mapper_prg_read = mapper_prg_read_int; // 8000-ffff (read)

	PRGExpMap = 0;
	cart_exp_write = NULL; // 6000 - 7fff
	cart_exp_read = NULL;

	psg_area_write = NULL; // 4020 - 5fff
	psg_area_read = NULL;

	bprintf(0, _T("Mapper #%d init/reset!\n"), mappernum);

	switch (mappernum) {
		case 0x808: { // Famicom Disk System
			NESMode |= MAPPER_NOCLEAR; // preserve mapper regs on soft-reset
			mapper_map_prg(32, 0, 0);
			mapper_map_chr( 8, 0, 0);
			psg_area_write = mapperFDS_write; // 4000 - 5fff
			psg_area_read = mapperFDS_read;
			mapper_write = mapperFDS_prg_write; // 8000 - ffff
			mapper_cycle = mapperFDS_cycle;
			mapper_map   = mapperFDS_map;
			nes_ext_sound_cb = mapperFDS_mixer;
			mapper_scan_cb  = mapperFDS_scan;

			BurnLEDInit(1, LED_POSITION_BOTTOM_RIGHT, LED_SIZE_4x4, LED_COLOR_GREEN, 80);

			mapperFDS_reset();
			mapper_map();
			retval = 0;
			break;
		}

		case 400: { // 8 Bit X-Mas 2017 (RET-X7-GBL)
			cart_exp_write  = mapper400_write;  // 6000 - 7fff
			mapper_write    = mapper400_write;  // 8000 - ffff
			mapper_map      = mapper400_map;

			mapper_regs[0] = 0x80; // default

			mapper_map();
			retval = 0;
			break;
		}

		case 413: { // Super Russian Roulette (BATLAB-SRR-X PCB)
			psg_area_read   = mapper413_psg_read;  // 4000 - 5fff
			cart_exp_read   = mapper413_exp_read;  // 6000 - 7fff
			mapper_prg_read = mapper413_prg_read;  // 8000 - ffff
			mapper_write    = mapper413_prg_write; // 8000 - ffff
			mapper_scanline = mapper413_scanline;
			mapper_map      = mapper413_map;

			mapper_map();
			retval = 0;
			break;
		}

		case 0: { // no mapper
			mapper_map_prg(32, 0, 0);
			mapper_map_chr( 8, 0, 0);
			retval = 0;
			break;
		}

		case 155: // 155 is mmc1 w/wram always mapped @ 6000-7fff
		case 1: { // mmc1
			if (mappernum != 155) {
				cart_exp_write = mapper01_exp_write; // 6000 - 7fff
				cart_exp_read = mapper01_exp_read;
			}
			mapper_write = mapper01_write;
			mapper_map   = mapper01_map;
			mapper_regs[0] = 0xc;
			mapper_map();
			retval = 0;
			break;
		}

		case 105: { // mmc1 + nwc (Nintendo World Championship 1990)
			mapper_write = mapper01_write;
			mapper_map   = mapper105_map;
			mapper_cycle = mapper105_cycle; // for timer
			mapper_regs[0] = 0xc;
			mapper_map();
			retval = 0;
			break;
		}

		case 2: { // UxROM
			mapper_write = mapper02_write;
			mapper_map   = mapper02_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 15: { // Contra 168-in-1 Multicart
			mapper_write = mapper15_write;
			mapper_map   = mapper15_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 190: { // Magic Kid Googoo
			mapper_write = mapper190_write;
			mapper_map   = mapper190_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 193: { // NTDEC TC-112 (War in the Gulf, Fighting Hero, ..)
			cart_exp_write = mapper193_write;
			mapper_map   = mapper193_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 72: { // Jaleco JF-17, Pinball Quest Japan
			mapper_write = mapper72_write;
			mapper_map   = mapper72_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 41: { // Caltron 6-in-1
			cart_exp_write = mapper41_write;
			mapper_write = mapper41_write;
			mapper_map   = mapper41_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 53: { // SuperVision 16-in-1
			NESMode |= MAPPER_NOCLEAR; // preserve mapper regs on soft-reset
			cart_exp_read = mapper53_exp_read;
			cart_exp_write = mapper53_write;
			mapper_write = mapper53_write;
			mapper_map   = mapper53_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 104: { // Golden Five / Pegasus 5-in-1
			mapper_write = mapper104_write;
			mapper_map   = mapper104_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 389: { // Caltron 9-in-1
			mapper_write = mapper389_write;
			mapper_map   = mapper389_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 216: { // Magic Jewelry 2
			mapper_write = mapper216_write;
			mapper_map   = mapper216_map;
			psg_area_read = mapper216_read;
			mapper_map();
			retval = 0;
			break;
		}

		case 132: { // Qi Wang
			mapper_write = mapper132_write;
			mapper_map   = mapper132_map;
			psg_area_read = mapper132_read;
			psg_area_write = mapper132_write;
			mapper_map();
			retval = 0;
			break;
		}

		case 3: { // CNROM
			mapper_write = mapper03_write;
			mapper_map   = mapper03_map;
			if (Cart.Crc == 0xab29ab28) // dropzone
				mapper_cycle = mapper03_cycle;
			mapper_map_prg(32, 0, 0);
			mapper_map();
			retval = 0;
			break;
		}

		case 5: { // MMC5
			cart_exp_write = mapper5_exp_write; // 6000 - 7fff
			cart_exp_read = mapper5_exp_read;
			psg_area_write = mapper5_write; // 4000 - 5fff
			psg_area_read = mapper5_read;
			mapper_write = mapper5_prg_write; // 8000 - ffff
			mapper_map   = mapper5_map;
			mapper_scan_cb  = mapper5_scan;
			mapper_ppu_clockall = mapper5_ppu_clk;
			nes_ext_sound_cb = mapper5_mixer;

			read_nt = &mapper5_ntread;
			write_nt = &mapper5_ntwrite;

			mapper5_reset();

			mapper_map();
			retval = 0;
			break;
		}

		case 7: { // AxROM: Battle Toads, Marble Madness, RC Pro-Am
			mapper_write = mapper07_write;
			mapper_map   = mapper07_map;
		    mapper_map_chr( 8, 0, 0);
			mapper_map();
			retval = 0;
			break;
		}

		case 9: { // mmc2: punch out!
			mapper_write = mapper09_write;
			mapper_map   = mapper09_map;
			mapper_ppu_clock = mapper09_ppu_clk;
			mapper_map_prg( 8, 1, -3);
			mapper_map_prg( 8, 2, -2);
			mapper_map_prg( 8, 3, -1);
			mapper_map();
			retval = 0;
			break;
		}

		case 10: { // mmc4: fire emblem (mmc2 + sram + different prg mapping)
			mapper_write = mapper09_write;
			mapper_map   = mapper10_map;
			mapper_ppu_clock = mapper10_ppu_clk;
			mapper_map();
			retval = 0;
			break;
		}

		case 99: {
			mapper_map = mapper99_map;
			mapper_map();
			if (Cart.Crc == 0x84ad422e) {
				bprintf(0, _T("Patching: VS. Raid on Bungling Bay...\n"));
				// VS. Raid on Bungling Bay patch
				bprintf(0, _T("%02x %02x %02x\n"), Cart.PRGRom[0x16df], Cart.PRGRom[0x16e0], Cart.PRGRom[0x16e1]);
				Cart.PRGRom[0x16df] = 0xea; // game stuck in loop @ 96df - possibly waiting for second cpu?
				Cart.PRGRom[0x16e0] = 0xea;
				Cart.PRGRom[0x16e1] = 0xea;

				nesapuSetMode4017(0x00); // enable frame-irq
			}

			retval = 0;
			break;
		}

		case 13: {
			mapper_write = mapper13_write;
			mapper_map = mapper13_map;
			mapper_map_prg(32, 0, 0);
			mapper_map();
			retval = 0;
			break;
		}

		case 133: {
			psg_area_write = mapper133_write;
			mapper_map = mapper133_map;
			mapper_map_prg(32, 0, 0);
			mapper_map();
			retval = 0;
			break;
		}

		case 159: // 128byte eeprom
		case 16: // 256byte eeprom
			{ // Bandai fcg
			mapper_write = mapper16_write;
			cart_exp_write = mapper16_write;
			mapper_map   = mapper16_map;
			mapper_cycle = mapper16_cycle;
			mapper_map();
			retval = 0;
			break;
		}

		case 153: { // Bandai fcg (Famicom Jump II)
			mapper_write = mapper16_write;
			mapper_map   = mapper153_map;
			mapper_cycle = mapper16_cycle;
			mapper_map();
			retval = 0;
			break;
		}

		case 18: { // Jaleco (Magical John, Pizza Pop, etc)
			mapper_write = mapper18_write;
			mapper_map   = mapper18_map;
			mapper_cycle = mapper18_cycle;
			mapper_map();
			retval = 0;
			break;
		}

		case 210:
		case 19: { // Namco n106/163
			psg_area_read  = mapper19_read;
			psg_area_write = mapper19_write; // 4020 - 5fff
			mapper_write = mapper19_write; // 8000 - ffff
			mapper_map   = mapper19_map;
			mapper_cycle = mapper19_cycle;
			nes_ext_sound_cb = mapper19_mixer;
			mapper_scan_cb = mapper19_scan;
			mapper19_mapper210 = (mappernum == 210);

			//waygan 2  0x8aa76b0b   wagyan 3 0x7aff2388  splatterh 0xb7eeb48b
			if (Cart.Crc == 0x8aa76b0b || Cart.Crc == 0x7aff2388 || Cart.Crc == 0xb7eeb48b) {
				mapper19_mapper210 = 2; // namco 340 (210 submapper 2)
				bprintf(0, _T("Namco 210 submapper 2 (Namco 340)\n"));
			}

			mapper19_reset();
			mapper_map();
			retval = 0;
			break;
		}

		case 21: { // vrc2
			mapper_write = mapper21_write;
			mapper_map   = vrc2vrc4_map;
			mapper_cycle = vrc2vrc4_cycle;
			mapper_map();
			retval = 0;
			break;
		}

		case 22: { // vrc2
			mapper_write = mapper22_write;
			mapper_map   = vrc2vrc4_map;
			vrc2and4_ines22 = 1;
			mapper_map();
			retval = 0;
			break;
		}

		case 23: { // vrc4
			mapper_write = mapper23_write;
			mapper_map   = vrc2vrc4_map;
			mapper_cycle = vrc2vrc4_cycle;
			mapper_map();
			retval = 0;
			break;
		}

		case 25: { // vrc4
			mapper_write = mapper22_write;
			mapper_map   = vrc2vrc4_map;
			mapper_cycle = vrc2vrc4_cycle;
			mapper_map();
			retval = 0;
			break;
		}

		case 24: { // vrc6
			mapper_write = vrc6_write;
			mapper_map   = vrc6_map;
			mapper_cycle = vrc6_cycle;
			vrc6_reset();
			mapper_map();
			nes_ext_sound_cb = vrc6_mixer; // vrc6 sound
			mapper_scan_cb = vrc6_scan;
			retval = 0;
			break;
		}

		case 26: { // vrc6
			mapper_write = vrc6_write;
			mapper_map   = vrc6_map;
			mapper_cycle = vrc6_cycle;
			mapper26 = 1;
			vrc6_reset();
			mapper_map();
			nes_ext_sound_cb = vrc6_mixer; // vrc6 sound
			mapper_scan_cb = vrc6_scan;
			retval = 0;
			break;
		}

		case 227: { // xxx-in-1, Waixing Bio Hazard
			mapper_write = mapper227_write;
			mapper_map   = mapper227_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 172: { // 1991 Du Ma Racing
			psg_area_write = mapper172_write;	// 4020 - 5fff
			psg_area_read = mapper172_read;		// 4020 - 5fff
			mapper_write = mapper172_write;
			mapper_map   = mapper172_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 228: { // Action 52, Cheetah Men II
			mapper_write = mapper228_write;
			psg_area_write = mapper228_psg_write;
			psg_area_read = mapper228_psg_read;
			mapper_map   = mapper228_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 91: { // Older JY Company/Hummer
			cart_exp_write = mapper91_write; // 6000 - 7fff
			mapper_map = mapper91_map;
			mapper_scanline = mapper91_scanline;
			mapper_map();
			retval = 0;
			break;
		}

		case 17: { // Front Far East FFE SMC (Type 17)
			psg_area_write = mapper17_write; // 4020 - 5fff
			mapper_map = mapper17_map;
			mapper_cycle = mapper17_cycle;
			mapper_regs[0] = ~3;
			mapper_regs[1] = ~2;
			mapper_regs[2] = ~1;
			mapper_regs[3] = ~0;
			mapper_map();
			retval = 0;
			break;
		}

		case 211: mapper90_211 = 1;
		case 209: mapper90_209 = 1;
		case 90: { // JY Company
			mapper_write = mapper90_write;       // 8000 - ffff
			psg_area_write = mapper90_psg_write; // 4020 - 5fff
			psg_area_read = mapper90_psg_read;
			cart_exp_write = mapper90_exp_write; // 6000 - 7fff
			cart_exp_read = mapper90_exp_read;

			mapper_scanline = mapper90_scanline;
			mapper_cycle = mapper90_cycle;
			mapper_ppu_clockall = mapper90_ppu_clock; // irq
			if (mapper90_209) {
				read_nt = &mapper90_ntread;
			}

			mapper_map   = mapper90_map;

			// mapper defaults
			mapper90_mul0 = 0xff;
			mapper90_mul1 = 0xff;
			mapper90_accu = 0xff;
			mapper90_testreg = 0xff;
			mapper90_chrlatch(0) = 0;
			mapper90_chrlatch(1) = 4;

			mapper_map();
			retval = 0;
			break;
		}

		case 28: { // Action53 / Home-brew multicart
			mapper_write   = mapper28_write;
			psg_area_write = mapper28_write;
			mapper_map     = mapper28_map;
			//mapper_map()
			NESMode |= MAPPER_NOCLEAR; // preserve mapper regs on soft-reset
			if (RESETMode == RESET_POWER)
				mapper_map_prg(16, 1, -1);
			retval = 0;
			break;
		}

		case 30: { // UNROM-512
			flashrom_chiptype = MC_SST;

			mapper_prg_read = flashrom_read;
			mapper_write = mapper30_write;
			mapper_map   = mapper30_map;
			switch (rom[6] & (1|8)) {
				case 0: set_mirroring(HORIZONTAL); break;
				case 1: set_mirroring(VERTICAL); break;
				case 8: set_mirroring(SINGLE_LOW); mapper30_mirroring_en = 1; break;
				case 9: set_mirroring(FOUR_SCREEN); break;
			}
			mapper_map();
			retval = 0;
			break;
		}

		case 33: { // Taito TC0190
			mapper_write = mapper33_write;
			mapper_map   = mapper33_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 36: { // TXC (Policeman, Strike Wolf)
			psg_area_read  = mapper36_read;
			psg_area_write = mapper36_write;
			mapper_write   = mapper36_write;
			mapper_map     = mapper36_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 42: { // Ai Senshi Nicol FDS Conversion
			mapper_write = mapper42_write;
			mapper_map   = mapper42_map;
			mapper_cycle = mapper42_cycle;
			cart_exp_read = mapper42_exp_read;
			mapper_map();
			retval = 0;
			break;
		}

		case 108: { // Meikyuu Jiin Dababa FDS conversion
			mapper_write = mapper108_write;
			psg_area_write = mapper108_write; // just to silent debug msgs
			mapper_map   = mapper108_map;
			cart_exp_read = mapper108_exp_read;
			mapper_map();
			retval = 0;
			break;
		}

		case 111: { // Cheapocabra (GTROM)
			cart_exp_write = mapper111_write;
			psg_area_write = mapper111_write;
			mapper_map   = mapper111_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 120: { // Tobidase Daisakusen FDS Conversion - garbage! (game crashes after lv1)
			psg_area_write = mapper120_write;
			mapper_map   = mapper120_map;
			cart_exp_read = mapper120_exp_read;
			mapper_map();
			retval = 0;
			break;
		}

		case 48: { // Taito TC0690
			mapper_write = mapper33_write;
			mapper_map   = mapper33_map;
			mapper_scanline = mapper33_scanline;
			mapper48 = 1;
			mapper48_flintstones = (Cart.Crc == 0x12f38740);
			bprintf(0, _T("mapper48 - flintstones? %x\n"), mapper48_flintstones);
			mapper_map();
			retval = 0;
			break;
		}

		case 185: { // CNROM + prot
			mapper_write = mapper185_write;
			mapper_map   = mapper185_map;
			memset(Cart.CHRRam, 0xff, Cart.CHRRamSize);
			mapper_map_prg(32, 0, 0);
			mapper_map();
			retval = 0;
			break;
		}

		case 32: { // Irem G101
			mapper_write = mapper32_write;
			mapper_map = mapper32_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 34: { // BNROM/NINA-001
			mapper_write = mapper34_write;
			cart_exp_write = mapper34_write;
			mapper_map = mapper34_map;
			mapper_regs[1] = 0; // defaults to retain compatibility with BNROM
			mapper_regs[2] = 1;
			mapper_map();
			retval = 0;
			break;
		}

		case 65: { // Irem h3001
			mapper_write = mapper65_write;
			mapper_map = mapper65_map;
			mapper_cycle = mapper65_cycle;
			mapper_regs[2] = -2;
			mapper_map_prg( 8, 3, -1);
			mapper_map();
			retval = 0;
			break;
		}

		case 67: { // Sunsoft-3
			mapper_write = mapper67_write;
			mapper_map = mapper67_map;
			mapper_cycle = mapper67_cycle;
			mapper_map();
			retval = 0;
			break;
		}

		case 68: { // Sunsoft-4
			mapper_write = mapper68_write;
			mapper_map = mapper68_map;
			mapper_cycle = mapper68_cycle;
			cart_exp_read = mapper68_exp_read;
			cart_exp_write = mapper68_exp_write;
			mapper_map();
			retval = 0;
			break;
		}

		case 64: { // Tengen RAMBO-1
			mapper_write = mapper64_write;
			mapper_map   = mapper64_map;
			mapper_scanline = mapper64_scanline;
			mapper_cycle = mapper64_cycle;
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 69: { // sunsoft
			mapper_write = mapper69_write;
			mapper_map   = mapper69_map;
			mapper_cycle = mapper69_cycle;
			cart_exp_read = mapper69_exp_read;
			cart_exp_write = mapper69_exp_write;
			mapper_map();
			AY8910Reset(0);
			retval = 0;
			break;
		}

		case 70: {
			mapper_write = mapper70_write; // 8000 - ffff
			mapper_map   = mapper70_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 71: { // camerica (codemasters) misc
			mapper_write = mapper71_write; // 8000 - ffff
			mapper_map   = mapper71_map;
		    mapper_map();
			if (Cart.Crc == 0xa64e926a && Cart.PRGRom[0xc4c1] == 0xa5) {
				bprintf(0, _T("Applied Hot-Patch: Quattro Sports / BMX Simulator start fix..\n"));
				Cart.PRGRom[0xc4c1] = 0xa9; // make ctrlr-debounce read happy
			}
			retval = 0;
			break;
		}

		case 73: { // Konami vrc3 (Salamander)
			mapper_write = mapper73_write;
			mapper_map   = mapper73_map;
			mapper_cycle = mapper73_cycle;
		    mapper_map();
			retval = 0;
			break;
		}

		case 75: { // Konami vrc1
			mapper_write = mapper75_write; // 8000 - ffff
			mapper_map   = mapper75_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 85: { // Konami vrc7 (Lagrange Point, Tiny Toons Adventures 2 (Japan))
			mapper_write = vrc7_write;
			mapper_map   = vrc7_map;
			mapper_cycle = vrc7_cycle;
			BurnYM2413VRC7Reset();
		    mapper_map();
			retval = 0;
			break;
		}

		case 232: { // camerica (codemasters) quattro
			mapper_write = mapper232_write;
			mapper_map   = mapper232_map;
			mapper_map();
			if (Cart.Crc == 0x9adc4130 && Cart.PRGRom[0x2c4c1] == 0xa5) {
				bprintf(0, _T("Applied Hot-Patch: Quattro Sports / BMX Simulator start fix..\n"));
				Cart.PRGRom[0x2c4c1] = 0xa9; // make ctrlr-debounce read happy
			}
			retval = 0;
			break;
		}

		case 77: { // Irem? Napoleon Senki
			mapper_write = mapper77_write; // 8000 - ffff
			mapper_map   = mapper77_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 78: { // Jaleco JF16
			mapper_write = mapper78_write; // 8000 - ffff
			mapper_map   = mapper78_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 80: { // Taito x1005
			cart_exp_write = mapper207_write;
			cart_exp_read  = mapper207_read;
			mapper_map     = mapper207_map;
			mapper_scan_cb = mapper207_scan;
			memset(mapper80_ram, 0, sizeof(mapper80_ram));
			mapper207_adv_mirror = 0;
			mapper_map();
			retval = 0;
			break;
		}

		case 82: { // Taito x1017
			cart_exp_write = mapper82_write;
			cart_exp_read  = mapper82_read;
			mapper_map     = mapper82_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 81: { // NTDEC Super Gun (Caltron)
			mapper_write = mapper81_write;
			mapper_map   = mapper81_map;
			mapper_map();
			retval = 0;
			break;
		}

		case 207: { // Taito x1005 w/advanced mirroring
			cart_exp_write = mapper207_write;
			cart_exp_read  = mapper207_read;
			mapper_map     = mapper207_map;
			mapper_scan_cb = mapper207_scan;
			memset(mapper80_ram, 0, sizeof(mapper80_ram));
			mapper207_adv_mirror = 1;
			mapper_map();
			retval = 0;
			break;
		}

		case 87: {
			cart_exp_write = mapper87_write; // 6000 - 7fff!
			mapper_map   = mapper87_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 88: {
			mapper_write = mapper88_write;
			mapper_map   = mapper88_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 154: {
			mapper_write = mapper88_write;
			mapper_map   = mapper88_map;
			mapper154 = 1;
		    mapper_map();
			retval = 0;
			break;
		}

		case 101: {
			cart_exp_write = mapper101_write; // 6000 - 7fff!
			mapper_map   = mapper101_map;
			mapper_regs[0] = 0xff;
		    mapper_map();
			retval = 0;
			break;
		}

		case 107: {
			mapper_write = mapper107_write;
			mapper_map   = mapper107_map;
			mapper_regs[0] = 0xff;
		    mapper_map();
			retval = 0;
			break;
		}

		case 38: {
			cart_exp_write = mapper38_write; // 6000 - 7fff
			mapper_map   = mapper38_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 66: {
			mapper_write = mapper140_write; // 8000 - ffff
			mapper_map   = mapper140_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 11: {
			mapper_write = mapper11_write; // 8000 - ffff
			mapper_map   = mapper11_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 144: { // similar to 11
			mapper_write = mapper144_write; // 8000 - ffff
			mapper_map   = mapper11_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 112: {
			mapper_write = mapper112_write;
			psg_area_write = mapper112_write;
			mapper_map   = mapper112_map;
			mapper_map_prg(8, 2, -2);
			mapper_map_prg(8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 471: {
			mapper_write = mapper471_write; // 8000 - ffff
			psg_area_write = mapper471_write; // 4020 - 5fff (need this to get rid of unmapped debug spam)
			mapper_map   = mapper471_map;
			mapper_scanline = mapper471_scanline;
		    mapper_map();
			retval = 0;
			break;
		}

		case 146: // Sachen Metal Fighter
		case 79: { // NINA-03 / NINA-06
			psg_area_write = mapper79_write; // 4020 - 5fff
			mapper_map     = mapper79_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 113: { // NINA-03 / NINA-06 extended / Mojon Twins Multicart
			psg_area_write = mapper113_write; // 4020 - 5fff
			mapper_map     = mapper113_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 137:
		case 138:
		case 139:
		case 141: { // Sachen 8259A/B/C/D
			psg_area_write = mapper8259_write; // 4020 - 5fff -and-
			cart_exp_write = mapper8259_write; // 6000 - 7fff
			mapper_map     = mapper8259_map;
			mapper_regs[0x1f] = mappernum;
			mapper_map_chr( 8, 0, 0);
		    mapper_map();
			retval = 0;
			break;
		}

		case 150: { // Sachen LS
			psg_area_write = mapper150_write; // 4020 - 5fff -and-
			psg_area_read  = mapper150_read;
			cart_exp_write = mapper150_write; // 6000 - 7fff
			cart_exp_read  = mapper150_read;
			mapper_map     = mapper150_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 162: { // (Waixing) Zelda - Triforce of the Gods
			psg_area_write = mapper162_write; // 4020 - 5fff
			mapper_map     = mapper162_map;
			mapper162_reg(0) = 0x03;
			mapper162_reg(3) = 0x07;
			mapper_map_chr( 8, 0, 0);
		    mapper_map();
			retval = 0;
			break;
		}

		case 163: { // Final Fantasy VII (NJ063)
			psg_area_write = mapper163_write; // 4020 - 5fff
			psg_area_read  = mapper163_read;
			mapper_map     = mapper163_map;
			mapper_ppu_clock = mapper163_cycle;
		    mapper_map();
			retval = 0;
			break;
		}

		case 140: {
			cart_exp_write = mapper140_write; // 6000 - 7fff!
			mapper_map     = mapper140_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 180: { // crazy climber
			mapper_write = mapper180_write;
			mapper_map   = mapper180_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 184: {
			cart_exp_write = mapper184_write; // 6000 - 7fff!
			mapper_map     = mapper184_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 152: {
			mapper_write = mapper152_write;
			mapper_map   = mapper152_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 156: {
			mapper_write = mapper156_write;
			mapper_map   = mapper156_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 89: {
			mapper_write = mapper89_write;
			mapper_map   = mapper89_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 93: { // Sunsoft-1/2: Fantasy Zone, Shanghai
			mapper_write = mapper93_write;
			mapper_map   = mapper93_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 94: { // Senjou no Ookami (Commando)
			mapper_write = mapper94_write;
			mapper_map   = mapper94_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 97: { // Kaiketsu Yanchamaru (Kid Niki)
			mapper_write = mapper97_write;
			mapper_map   = mapper97_map;
		    mapper_map();
			retval = 0;
			break;
		}

		case 206: // mmc3 w/no irq (Tengen mimic-1, Namcot 118, 106)
		case 4: { // mmc3
			mapper_write = mapper04_write;
			mapper_map   = mapper04_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring; // wagyan land doesn't set the mapper bit!

			{
				// VS. Mapper 4 / 206 Protection
				mapper04_vs_prottype = 0;
				switch (Cart.Crc) {
					default: break;
					case 0x9f2251e4: // super xevious
						mapper04_vs_prottype++;         // fallthrough!
					case 0xd81d33da: // tko boxing
						mapper04_vs_prottype++;         // fallthrough!
					case 0xac95e2c9: // rbi baseball
						psg_area_read = mapper04_vs_rbi_tko_prot;
						break;
				}
			}

			// default mmc3 regs:
			// chr
			mapper_regs[0] = 0;
			mapper_regs[1] = 2;
			mapper_regs[2] = 4;
			mapper_regs[3] = 5;
			mapper_regs[4] = 6;
			mapper_regs[5] = 7;
			// prg
			mapper_regs[6] = 0;
			mapper_regs[7] = 1;

			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 555: { // NCC
			// mmc3 stuff
			mapper_write = mapper04_write;        // 8000-ffff, write only
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring;// inherit mirror from cart header

			mapper_map   = mapper555_map;

			psg_area_write = mapper555_write;     // 4000-5fff
			psg_area_read = mapper555_read;       // 4000-5fff

			cart_exp_write = mapper555_prgwrite;  // 6000-6fff
			cart_exp_read = mapper555_prgread;    // 6000-6fff

			mapper_cycle = mapper555_cycle; // for timer

			// default mmc3 regs:
			// chr
			mapper555_prgmask = 0x07;

			mapper_regs[0] = 0;
			mapper_regs[1] = 2;
			mapper_regs[2] = 4;
			mapper_regs[3] = 5;
			mapper_regs[4] = 6;
			mapper_regs[5] = 7;
			// prg
			mapper_regs[6] = 0;
			mapper_regs[7] = 1;

		    mapper_map();
			retval = 0;
			break;
		}

		case 406: { // Haradius Zero (mmc3 + 2xMXIC flash)
			flashrom_chiptype = MXIC;

			mapper_prg_read = flashrom_read;
			mapper_write    = mapper406_write;

			mapper_map      = mapper04_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror  = Cart.Mirroring;

			mapper_map_prg( 8, 3, -1);
			mapper_map();
			retval = 0;
			break;
		}

		case 451: { // Haratyler pseudo-mmc3 + AMIC flash
			flashrom_chiptype = AMIC;

			mapper_prg_read = flashrom_read;
			mapper_write    = mapper451_write;

			mapper_map      = mapper451_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror  = Cart.Mirroring;

			mapper_map();
			retval = 0;
			break;
		}

		case 12: { // mmc3 variant (dbz 5, mk4, umk3)
			psg_area_write = mapper12_write;
			psg_area_read = mapper12_read;
			mapper_write = mapper04_write;
			mapper_map   = mapper04_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring; // wagyan land doesn't set the mapper bit!
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 76: { // mmc3 variant (digital devil m.)
			mapper_write = mapper04_write;
			mapper_map   = mapper76_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring;
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 268: { // mmc3 variant (COOLBOY / MINDKIDS)
			mapper_write = mapper268_write;

			if (Cart.SubMapper == 1) psg_area_write = mapper268_write; // 5000-5fff
			else cart_exp_write = mapper268_write;                     // 6000-6fff

			mapper_map   = mapper268_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring;

			mapper_map();
			retval = 0;
			break;
		}

		case 95: { // mmc3 variant (dragon buster)
			mapper_write = mapper95_write;
			mapper_map   = mapper95_map;
			mapper_scanline = mapper04_scanline;
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 115: { // mmc3 variant (thunderbolt 2)
			mapper_write = mapper04_write;

			psg_area_write = mapper115_write;
			psg_area_read = mapper115_read;
			cart_exp_write = mapper115_write;
			cart_exp_read = mapper115_read;

			mapper_map   = mapper115_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring; // wagyan land doesn't set the mapper bit!
		    mapper_map();
			retval = 0;
			break;
		}

		case 116: { // Somari (AV Girl Fighter)
			mapper_write = mapper116_write;
			psg_area_write = mapper116_write;
			cart_exp_write = mapper116_write;
			mapper_map   = mapper116_map;
			mapper_scanline = mapper116_mmc3_scanline;
			mapper116_defaults();
		    mapper_map();
			retval = 0;
			break;
		}

		case 14: { // Gouder SL-1632 (Samurai Spirits)
			mapper_write = mapper116_write;
			psg_area_write = mapper116_write;
			cart_exp_write = mapper116_write;
			mapper_map   = mapper14_map; // difference from 116
			mapper_scanline = mapper116_mmc3_scanline;
			mapper116_defaults();
		    mapper_map();
			retval = 0;
			break;
		}

		case 118: { // mmc3-derivative TKSROM/TLSROM (Alien Syndrome, Armadillo)
			mapper_write = mapper04_write;
			mapper_map   = mapper118_map;
			mapper_scanline = mapper04_scanline;
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 119: { // mmc3-derivative w/char ram+rom & ability to map both into diff. banks! (pin-bot, high speed)
			mapper_write = mapper04_write;
			mapper_map   = mapper119_map;
			mapper_scanline = mapper04_scanline;
			mapper_set_chrtype(MEM_RAM);
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 165: { // mmc3-derivative w/mmc4-style char ram(bank0)+rom(others)
			mapper_write = mapper04_write;
			mapper_map   = mapper165_map;
			mapper_ppu_clock = mapper165_ppu_clock;
			mapper_scanline = mapper04_scanline;
			mapper_set_chrtype(MEM_RAM);
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 192: { // mmc3-derivative w/char ram+rom, ram mapped to chr banks 8, 9, a, b
			mapper_write = mapper04_write;
			mapper_map   = mapper192_map;
			mapper_scanline = mapper04_scanline;
			mapper_set_chrtype(MEM_RAM);
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 194: { // mmc3-derivative w/char ram+rom, ram mapped to chr banks 0, 1
			mapper_write = mapper04_write;
			mapper_map   = mapper194_map;
			mapper_scanline = mapper04_scanline;
			mapper_set_chrtype(MEM_RAM);
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 195: { // mmc3-derivative w/char ram+rom, ram mapped to chr banks 0-3, cpu accessable via 5000-5fff
			mapper_write = mapper04_write;
			mapper_map   = mapper195_map;
			psg_area_write  = mapper195_write;	// 5000 - 5fff r/w chr ram
			psg_area_read   = mapper195_read;	// 5000 - 5fff
			mapper_scanline = mapper04_scanline;
			mapper_set_chrtype(MEM_RAM);
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 189: { // mmc3-derivative
			psg_area_write = mapper189_write; // 4020 - 5fff
			cart_exp_write = mapper189_write; // 6000 - 7fff
			mapper_write = mapper04_write;    // 8000 - ffff
			mapper_map   = mapper189_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring; // wagyan land doesn't set the mapper bit!
		    mapper_map();
			retval = 0;
			break;
		}

		case 258: { // mmc3-derivative (blood of jurassic)
			psg_area_write = mapper258_write;	// 4020 - 5fff
			psg_area_read = mapper258_read;		// 4020 - 5fff
			mapper_write = mapper04_write;		// 8000 - ffff
			mapper_map   = mapper258_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring; // wagyan land doesn't set the mapper bit!

			// default mmc3 regs:
			// chr
			mapper_regs[0] = 0;
			mapper_regs[1] = 2;
			mapper_regs[2] = 4;
			mapper_regs[3] = 5;
			mapper_regs[4] = 6;
			mapper_regs[5] = 7;
			// prg
			mapper_regs[6] = 0;
			mapper_regs[7] = 1;

			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}

		case 262: { // mmc3-derivative (Street Heroes)
			psg_area_write = mapper262_write;
			psg_area_read = mapper262_read;
			mapper_write = mapper04_write;
			mapper_map   = mapper262_map;
			mapper_scanline = mapper04_scanline;
			mapper4_mirror = Cart.Mirroring; // wagyan land doesn't set the mapper bit!
			mapper_map_prg( 8, 3, -1);
		    mapper_map();
			retval = 0;
			break;
		}
	}
	return retval;
}

static void mapper_irq(INT32 delay_cyc)
{
	if (delay_cyc == 0) { // irq now
		M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
	} else { // irq later (after 'cyc' m2 cycles)
		mapper_irq_exec = delay_cyc;
	}
}

static void mapper_run()
{
	if (mapper_irq_exec) {
		mapper_irq_exec--;
		if (mapper_irq_exec == 0) {
			M6502SetIRQLine(0, CPU_IRQSTATUS_ACK);
		}
	}

	if (mapper_cycle) mapper_cycle();
}

// --------[ end Mappers

static UINT32 nes_palette[3][0x40] = {
{ // dink-fami
	0x5c5c5c, 0x00237e, 0x100e9e, 0x33009b, 0x520074, 0x630037, 0x610500, 0x4c1700, 0x2b2c00, 0x093e00, 0x004700, 0x004505, 0x003744, 0x000000, 0x000000, 0x000000,
	0xa7a7a7, 0x1157d6, 0x3b38ff, 0x6d21fe, 0x9916c8, 0xb11973, 0xae2a1a, 0x904500, 0x626400, 0x307d00, 0x0a8a00, 0x00862a, 0x007385, 0x000000, 0x000000, 0x000000,
	0xfeffff, 0x5ba9ff, 0x8a88ff, 0xc16eff, 0xef61ff, 0xff65c7, 0xff7866, 0xe6961b, 0xb4b700, 0x7fd200, 0x53e027, 0x3cdc79, 0x3fc7da, 0x454545, 0x000000, 0x000000,
	0xfeffff, 0xbadbff, 0xcecdff, 0xe5c2ff, 0xf8bcff, 0xffbee7, 0xffc6be, 0xf4d39c, 0xe0e18a, 0xc9ec8c, 0xb6f2a2, 0xacf0c6, 0xade8ef, 0xb0b0b0, 0x000000, 0x000000,
},
{ // rgb
	0x7c7c7c, 0x0000fc, 0x0000bc, 0x4428bc, 0x940084, 0xa80020, 0xa81000, 0x881400,	0x503000, 0x007800, 0x006800, 0x005800, 0x004058, 0x000000, 0x000000, 0x000000,
	0xbcbcbc, 0x0078f8, 0x0058f8, 0x6844fc, 0xd800cc, 0xe40058, 0xf83800, 0xe45c10,	0xac7c00, 0x00b800, 0x00a800, 0x00a844, 0x008888, 0x000000, 0x000000, 0x000000,
	0xf8f8f8, 0x3cbcfc, 0x6888fc, 0x9878f8, 0xf878f8, 0xf85898, 0xf87858, 0xfca044,	0xf8b800, 0xb8f818, 0x58d854, 0x58f898, 0x00e8d8, 0x787878, 0x000000, 0x000000,
	0xfcfcfc, 0xa4e4fc, 0xb8b8f8, 0xd8b8f8, 0xf8b8f8, 0xf8a4c0, 0xf0d0b0, 0xfce0a8,	0xf8d878, 0xd8f878, 0xb8f8b8, 0xb8f8d8, 0x00fcfc, 0xf8d8f8, 0x000000, 0x000000
},
{ // Royaltea (Rupert Carmichael)
	0x5a6165, 0x0023a8, 0x0f17b0, 0x28129f,	0x550b61, 0x6b0a11, 0x6e0d00, 0x5e1900,	0x3c2402, 0x003104, 0x003508, 0x00341f,	0x002c55, 0x000000, 0x000000, 0x000000,
	0xa7b5bc, 0x0059ff, 0x2a44ff, 0x523cf1,	0x9f34ba, 0xb32846, 0xbb2d09, 0x9e4100,	0x865a00, 0x246d02, 0x007312, 0x007156,	0x0066a6, 0x000000, 0x000000, 0x000000,
	0xffffff, 0x4b9fff, 0x5a91ff, 0x867eff,	0xd97dff, 0xff95cf, 0xff8e76, 0xf7a247,	0xefb412, 0x8cc51c, 0x48d04a, 0x10d197,	0x00c9f0, 0x43484b, 0x000000, 0x000000,
	0xffffff, 0xb1d9ff, 0xb1cfff, 0xbcc8ff,	0xe3c8ff, 0xffd3f7, 0xffd5cb, 0xffdeb9,	0xffe5ad, 0xdbf6af, 0xb7fbc4, 0x9cfbe6,	0x96f7ff, 0xb1c0c7, 0x000000, 0x000000
} };

static UINT32 vs_palette[5][0x40] = {
{ // 2C03, 2C05
	0x6d6d6d, 0x002492, 0x0000db, 0x6d49db, 0x92006d, 0xb6006d, 0xb62400, 0x924900, 0x6d4900, 0x244900, 0x006d24, 0x009200, 0x004949, 0x000000, 0x000000, 0x000000,
	0xb6b6b6, 0x006ddb, 0x0049ff, 0x9200ff, 0xb600ff, 0xff0092, 0xff0000, 0xdb6d00, 0x926d00, 0x249200, 0x009200, 0x00b66d, 0x009292, 0x000000, 0x000000, 0x000000,
	0xffffff, 0x6db6ff, 0x9292ff, 0xdb6dff, 0xff00ff, 0xff6dff, 0xff9200, 0xffb600, 0xdbdb00, 0x6ddb00, 0x00ff00, 0x49ffdb, 0x00ffff, 0x000000, 0x000000, 0x000000,
	0xffffff, 0xb6dbff, 0xdbb6ff, 0xffb6ff, 0xff92ff, 0xffb6b6, 0xffdb92, 0xffff49, 0xffff6d, 0xb6ff49, 0x92ff6d, 0x49ffdb, 0x92dbff, 0x000000, 0x000000, 0x000000
},
{ // 2C04A
	0xffb6b6, 0xdb6dff, 0xff0000, 0x9292ff, 0x009292, 0x244900, 0x494949, 0xff0092, 0xffffff, 0x6d6d6d, 0xffb600, 0xb6006d, 0x92006d, 0xdbdb00, 0x6d4900, 0xffffff,
	0x6db6ff, 0xdbb66d, 0x6d2400, 0x6ddb00, 0x92dbff, 0xdbb6ff, 0xffdb92, 0x0049ff, 0xffdb00, 0x49ffdb, 0x000000, 0x490000, 0xdbdbdb, 0x929292, 0xff00ff, 0x002492,
	0x00006d, 0xb6dbff, 0xffb6ff, 0x00ff00, 0x00ffff, 0x004949, 0x00b66d, 0xb600ff, 0x000000, 0x924900, 0xff92ff, 0xb62400, 0x9200ff, 0x0000db, 0xff9200, 0x000000,
	0x000000, 0x249200, 0xb6b6b6, 0x006d24, 0xb6ff49, 0x6d49db, 0xffff00, 0xdb6d00, 0x004900, 0x006ddb, 0x009200, 0x242424, 0xffff6d, 0xff6dff, 0x926d00, 0x92ff6d
},
{ // 2C04B
	0x000000, 0xffb600, 0x926d00, 0xb6ff49, 0x92ff6d, 0xff6dff, 0x009292, 0xb6dbff, 0xff0000, 0x9200ff, 0xffff6d, 0xff92ff, 0xffffff, 0xdb6dff, 0x92dbff, 0x009200,
	0x004900, 0x6db6ff, 0xb62400, 0xdbdbdb, 0x00b66d, 0x6ddb00, 0x490000, 0x9292ff, 0x494949, 0xff00ff, 0x00006d, 0x49ffdb, 0xdbb6ff, 0x6d4900, 0x000000, 0x6d49db,
	0x92006d, 0xffdb92, 0xff9200, 0xffb6ff, 0x006ddb, 0x6d2400, 0xb6b6b6, 0x0000db, 0xb600ff, 0xffdb00, 0x6d6d6d, 0x244900, 0x0049ff, 0x000000, 0xdbdb00, 0xffffff,
	0xdbb66d, 0x242424, 0x00ff00, 0xdb6d00, 0x004949, 0x002492, 0xff0092, 0x249200, 0x000000, 0x00ffff, 0x924900, 0xffff00, 0xffb6b6, 0xb6006d, 0x006d24, 0x929292
},
{ // 2C04C
	0xb600ff, 0xff6dff, 0x92ff6d, 0xb6b6b6, 0x009200, 0xffffff, 0xb6dbff, 0x244900, 0x002492, 0x000000, 0xffdb92, 0x6d4900, 0xff0092, 0xdbdbdb, 0xdbb66d, 0x92dbff,
	0x9292ff, 0x009292, 0xb6006d, 0x0049ff, 0x249200, 0x926d00, 0xdb6d00, 0x00b66d, 0x6d6d6d, 0x6d49db, 0x000000, 0x0000db, 0xff0000, 0xb62400, 0xff92ff, 0xffb6b6,
	0xdb6dff, 0x004900, 0x00006d, 0xffff00, 0x242424, 0xffb600, 0xff9200, 0xffffff, 0x6ddb00, 0x92006d, 0x6db6ff, 0xff00ff, 0x006ddb, 0x929292, 0x000000, 0x6d2400,
	0x00ffff, 0x490000, 0xb6ff49, 0xffb6ff, 0x924900, 0x00ff00, 0xdbdb00, 0x494949, 0x006d24, 0x000000, 0xdbb6ff, 0xffff6d, 0x9200ff, 0x49ffdb, 0xffdb00, 0x004949
},
{ // 2C04D
	0x926d00, 0x6d49db, 0x009292, 0xdbdb00, 0x000000, 0xffb6b6, 0x002492, 0xdb6d00, 0xb6b6b6, 0x6d2400, 0x00ff00, 0x00006d, 0xffdb92, 0xffff00, 0x009200, 0xb6ff49,
	0xff6dff, 0x490000, 0x0049ff, 0xff92ff, 0x000000, 0x494949, 0xb62400, 0xff9200, 0xdbb66d, 0x00b66d, 0x9292ff, 0x249200, 0x92006d, 0x000000, 0x92ff6d, 0x6db6ff,
	0xb6006d, 0x006d24, 0x924900, 0x0000db, 0x9200ff, 0xb600ff, 0x6d6d6d, 0xff0092, 0x004949, 0xdbdbdb, 0x006ddb, 0x004900, 0x242424, 0xffff6d, 0x929292, 0xff00ff,
	0xffb6ff, 0xffffff, 0x6d4900, 0xff0000, 0xffdb00, 0x49ffdb, 0xffffff, 0x92dbff, 0x000000, 0xffb600, 0xdb6dff, 0xb6dbff, 0x6ddb00, 0xdbb6ff, 0x00ffff, 0x244900
} };

#define DIP_PALETTE (NESDips[2] & 3)
static INT32 last_palette;
static UINT32 *our_palette = NULL;

static void UpdatePalettePointer()
{
	switch (PPUType) {
		default:
		case RP2C02: // NES/FC/Royaltea palette (dip-selectable)
			our_palette = nes_palette[DIP_PALETTE];
			break;
		case RP2C03:	// VS. palettes (... and below!)
		case RP2C05A:
		case RP2C05B:
		case RP2C05C:
		case RP2C05D:
		case RP2C05E:
			our_palette = vs_palette[0];
			break;
		case RP2C04A:
		case RP2C04B:
		case RP2C04C:
		case RP2C04D:
			our_palette = vs_palette[1 + (PPUType - RP2C04A)];
			break;
	}
}

static INT32 nes_frame_cycles;
static UINT32 nes_ppu_cyc_mult;
static UINT32 prerender_line; // ntsc 261, pal 311

static UINT32 ppu_frame;
static UINT32 ppu_framecycles;
static UINT32 mirroring;
static UINT8 nt_ram[0x400 * 4]; // nt (2k std. / 4k four-screen [2k expansion is usually on cart])
static UINT8 pal_ram[0x20];   // palette ram
static UINT8 oam_ram[0x100];  // oamram (sprite)
static OAMBUF oam[0x40];   // "to be drawn" oam
static INT32 oam_cnt;
static OAMBUF oam2[0x40];  // secondary oam (after eval)
static INT32 oam2_cnt;
static UINT8 oamAddr;

static UINT16 v_addr, t_addr; // vram addr (loopy-v), temp vram addr (loopy-t)
static UINT16 v_addr_update;
static UINT32 v_addr_update_delay;
static UINT8 fine_x; // fine-x (scroll)

#if 0 // moved above the mapper section
static UINT8 ppu_ctrl;
static UINT8 ppu_mask;
static UINT8 ppu_status;
#define RENDERING (ppu_mask & (mask_bg | mask_spr)) // show bg (0x08) + show sprites (0x10)
#endif
static UINT32 sprite_height; // 8, 16. set on PPUCTRL
static UINT32 v_incr; // 1, 32. set on PPUCTRL
static UINT32 bgtable_start; // 0, 0x1000. set on PPUCTRL
static UINT32 sprtable_start; // 0, 0x1000. set on PPUCTRL
static UINT32 ppu_pal_mask; // 0x3f (color) 0x30 (gray). set on PPUMASK
static UINT32 ppu_pal_emphasis; // emphasis palette index

static UINT32 ppu_no_nmi_this_frame;
static UINT32 ppu_odd;
static UINT16 ppu_bus_address;
static INT32 ppu_over; // #cycles we've run over/under to be compensated for on next frame
static UINT8 ppu_dbus; // ppu data-bus
static UINT8 ppu_buffer; // VRAM read buffer.

static UINT8 write_latch;
static UINT8 nt_byte;

static UINT8 bgL, bgH;
static UINT16 bg_shiftL, bg_shiftH;

static UINT8 at_byte;
static UINT8 at_shiftL, at_shiftH;
static UINT8 at_latchL, at_latchH;

#define get_bit(x, n) (((x) >> (n)) & 1)
static UINT8 bitrev_table[0x100];
static UINT8 sprite_cache[256+8];
static UINT32 spritemasklimit;
static UINT32 bgmasklimit;

static UINT16 *screen;

// Nametable mirroring/mapping
static UINT8 *NTMap[4];
static UINT32 NTType[4];

static void nametable_map(INT32 nt, INT32 ntbank)
{
	NTMap[nt & 3] = &nt_ram[0x400 * (ntbank & 3)];
	NTType[nt & 3] = MEM_RAM;
}

static void nametable_mapraw(INT32 nt, UINT8 *ntraw, UINT8 type)
{
	NTMap[nt & 3] = ntraw;
	NTType[nt & 3] = type;
}

static void nametable_mapall(INT32 ntbank0, INT32 ntbank1, INT32 ntbank2, INT32 ntbank3)
{
	nametable_map(0, ntbank0);
	nametable_map(1, ntbank1);
	nametable_map(2, ntbank2);
	nametable_map(3, ntbank3);
}

static void set_mirroring(INT32 mode)
{
	switch (mode)
    {
        case HORIZONTAL:  nametable_mapall(0, 0, 1, 1); break;
        case VERTICAL:	  nametable_mapall(0, 1, 0, 1); break;
		case SINGLE_LOW:  nametable_mapall(0, 0, 0, 0); break;
		case SINGLE_HIGH: nametable_mapall(1, 1, 1, 1); break;
		case FOUR_SCREEN: nametable_mapall(0, 1, 2, 3); break;
		case SACHEN:	  nametable_mapall(0, 1, 1, 1); break;
	}

	mirroring = mode;
}

static UINT8 read_nt_int(UINT16 address)
{
	return NTMap[(address & 0xfff) >> 10][address & 0x3ff];
}

static void write_nt_int(UINT16 address, UINT8 data)
{
	if (NTType[(address & 0xfff) >> 10] == MEM_RAM)
		NTMap[(address & 0xfff) >> 10][address & 0x3ff] = data;
}


// The internal PPU-Bus
static UINT8 ppu_bus_read(UINT16 address)
{
	if (address >= 0x0000 && address <= 0x1fff)
		return mapper_chr_read(address);

	if (address >= 0x2000 && address <= 0x3eff)
		return read_nt(address);

	if (address >= 0x3f00 && address <= 0x3fff) {
		return pal_ram[address & 0x1f] & ppu_pal_mask;
	}

	return 0;
}

static void ppu_bus_write(UINT16 address, UINT8 data)
{
	if (address >= 0x0000 && address <= 0x1fff)
		mapper_chr_write(address, data);

	if (address >= 0x2000 && address <= 0x3eff)
		write_nt(address, data);

	if (address >= 0x3f00 && address <= 0x3fff) {
		pal_ram[address & 0x1f] = data;
		if ((address & 0x03) == 0) {
			// mirrors: 0x10, 0x14, 0x18, 0x1c <-> 0x00, 0x04, 0x08, 0x0c
			pal_ram[(address & 0x1f) ^ 0x10] = data;
		}
	}
}

static void h_scroll();
static void v_scroll();

static void ppu_inc_v_addr()
{
	if (RENDERING && (scanline < 241 || scanline == prerender_line)) {
		// no updates while rendering!
		//bprintf(0, _T("no updates while rendering! frame: %X  scanline: %d    pixel: %d\n"), nCurrentFrame, scanline, pixel);

		// Burai Fighter (U) statusbar fix
		h_scroll();
		v_scroll();
		return;
	}

	v_addr = (v_addr + v_incr) & 0x7fff;
	ppu_bus_address = v_addr & 0x3fff;
}

static void get_vsystem_prot(UINT8 &dbus, UINT8 status_reg)
{	// 2C05A-E(D) returns a special ID in the usually-unused PPU status bits
	// as a form of EEPROM-Swap/Copy protection.
	const UINT8 _2c05_ids[5] = { 0x1b, 0x3d, 0x1c, 0x1b, 0x00 };
	if (PPUType >= RP2C05A && PPUType <= RP2C05E) {
		dbus = _2c05_ids[PPUType - RP2C05A] | status_reg;
	}
}

static UINT8 ppu_read(UINT16 reg)
{
	reg &= 7;

	switch (reg)
	{
		case 2: // PPUSTATUS
			if (scanline == 241 && pixel < 3) {
				// re: https://wiki.nesdev.com/w/index.php/PPU_frame_timing
				// PPUSTATUS read: 1 cycle before vbl (pixel == 0) skips status & nmi
				// on or 1 pixel after (pixel == 1 or 2) returns vblank status, but skips nmi
				//bprintf(0, _T("PPUSTATUS: CANCEL NMI.  scanline %d  pixel %d\n"), scanline, pixel);
				ppu_no_nmi_this_frame = 1;
				if (pixel == 0) ppu_status &= ~status_vblank;
			}
			ppu_dbus = (ppu_dbus & 0x1f) | ppu_status;
			get_vsystem_prot(ppu_dbus, ppu_status);
			ppu_status &= ~status_vblank;
			write_latch = 0;
			//bprintf(0, _T("PPUSTATUS - frame: %d   scanline: %d     pixel: %d    res: %X   PC: %X\n"), nCurrentFrame, scanline, pixel, ppu_dbus, M6502GetPrevPC(-1));
			break;

		case 4: // OAMDATA
			if (RENDERING && scanline < 241) {
				// quirks to keep micromachines happy
				if (pixel >= 1 && pixel <= 64) {
					ppu_dbus = 0xff;
					break;
				}
				if (pixel >= 65 && pixel <= 256) {
					ppu_dbus = 0x00;
					break;
				}

				if (pixel >= 257 && pixel <= 320) {
					ppu_dbus = oam2[((pixel & 0xff) >> 3)].x;
					break;
				}

				if ((pixel >= 321 && pixel <= 340) || pixel == 0) {
					ppu_dbus = oam2[0].y;
					break;
				}
			}
			ppu_dbus = oam_ram[oamAddr];
			break;

		case 7: // PPUDATA
			if ((ppu_bus_address & 0x3fff) < 0x3f00) {
				// vram buffer delay
				ppu_dbus = ppu_buffer;
				ppu_buffer = ppu_bus_read(ppu_bus_address);
				//mapper_ppu_clock(ppu_bus_address);
			} else {
				// palette has no buffer delay, buffer gets stuffed with vram though (ppu quirk)
				ppu_dbus = ppu_bus_read(ppu_bus_address);
				ppu_buffer = ppu_bus_read(ppu_bus_address - 0x1000);
			}

			ppu_inc_v_addr();
			break;

	}

	return ppu_dbus;
}

static void ppu_write(UINT16 reg, UINT8 data)
{
	reg &= 7;

	if (PPUType >= RP2C05A && reg < 2) {
		// PPUCTRL / PPUMASK swapped on RP2C05x
		reg ^= 1;
	}

	ppu_dbus = data;

	switch (reg)
	{
		case 0: // PPUCTRL
			if (data & ctrl_nmi) {
				if ((~ppu_ctrl & ctrl_nmi) && (ppu_status & status_vblank)) {
					//--Note: If NMI is fired here, it will break:
					//Bram Stokers Dracula, Galaxy 5000, GLUK The Thunder aka Thunder Warrior
					//Animal Clipper (HB) - nmi clobbers A register during bootup.
					//Dragon Power scrolling goes bad
					//Solution: Delay NMI by 1(?) opcode
					//bprintf(0, _T("PPUCTRL: toggle-nmi-arm! scanline %d  pixel %d    frame: %d   PPC %X\n"), scanline, pixel, nCurrentFrame, M6502GetPC(-1));
					m6502_set_nmi_hold2();
				}
			} else {
				//bprintf(0, _T("PPUCTRL: %X  cancel-nmi?  scanline %d  pixel %d   frame %d\n"), data, scanline, pixel, nCurrentFrame);
			}

			ppu_ctrl = data;
			//bprintf(0, _T("PPUCTRL reg: %X   scanline %d  pixel %d   frame %d  PC  %X\n"), ctrl.reg, scanline, pixel, nCurrentFrame, M6502GetPC(-1));
			t_addr = (t_addr & 0x73ff) | ((data & ctrl_ntbase) << 10);

			sprite_height = (ppu_ctrl & ctrl_sprsize) ? 16 : 8;
			v_incr = (ppu_ctrl & ctrl_incr) ? 32 : 1;
			bgtable_start = (ppu_ctrl & ctrl_bgtab) ? 0x1000 : 0x0000;
			sprtable_start = (ppu_ctrl & ctrl_sprtab) ? 0x1000 : 0x0000;
			break;

		case 1: // PPUMASK
			if ((data & (mask_bg | mask_spr))==0 && RENDERING) { // rendering ON -> OFF assert ppu_bus_address
				ppu_bus_address = v_addr & 0x3fff;
			}
			ppu_mask = data;
			ppu_pal_emphasis = (data >> 5) * 0x40;
			if (NESMode & IS_PAL) {
				ppu_pal_emphasis = (((data & 0x80) | ((data & 0x40) >> 1) | ((data & 0x20) << 1)) >> 5) * 0x40;
			}
			//bprintf(0, _T("mask   %x\n"), data);
			ppu_pal_mask = ((ppu_mask & mask_greyscale) ? 0x30 : 0x3f);

			bgmasklimit = ~0;
			spritemasklimit = ~0;
			if (ppu_mask & mask_bg) {
				bgmasklimit = (ppu_mask & mask_bgleft) ? 0 : 8;
			}
			if (ppu_mask & mask_spr) {
				spritemasklimit = (ppu_mask & mask_sprleft) ? 0 : 8;
			}

			break;

		case 3: // OAMADDR
			oamAddr = data;
			break;

		case 4: // OAMDATA
			//bprintf(0, _T("Frame %04x:  OAMDATA[%X]  %X      scanline %d  pixel %d\n"), nCurrentFrame, oamAddr, data, scanline, pixel);
			if (RENDERING && (scanline < 241 || scanline == prerender_line) && (~NESMode & BAD_HOMEBREW)) {
				bprintf(0, _T("write OAM prohibited.  scanline %d\n"), scanline);
				return;
			}
			if ((oamAddr & 3) == 2) data &= 0xe3; // attr mask
			oam_ram[oamAddr] = data;
			oamAddr = (oamAddr + 1) & 0xff;
			break;

		case 5: // PPUSCROLL
			if (!write_latch) {      // First write.
				fine_x = data & 7;
				t_addr = (t_addr & 0x7fe0) | ((data & 0xf8) >> 3);
			} else  {                // Second write.
				t_addr = (t_addr & 0x0c1f) | ((data & 0xf8) << 2) | ((data & 0x07) << 12);
			}
			write_latch = !write_latch;
			break;
		case 6: // PPUADDR
			if (!write_latch) {		// First write.
				t_addr = (t_addr & 0x00ff) | ((data & 0x3f) << 8);
			} else {                // Second write.
				t_addr = (t_addr & 0x7f00) | data;
				v_addr_update = t_addr;
				v_addr_update_delay = 2;
			}
			write_latch = !write_latch;
			break;
		case 7: // PPUDATA
			ppu_bus_write(ppu_bus_address, data);
			//mapper_ppu_clock(ppu_bus_address);

			ppu_inc_v_addr();
			break;
	}
}

static void h_scroll()
{
	if (RENDERING) {
		if ((v_addr & 0x1f) == 0x1f) {
			v_addr ^= 0x041f;
		} else {
			v_addr++;
		}
	}
}

static void v_scroll()
{
	if (RENDERING) {
		if ((v_addr & 0x7000) == 0x7000) {
			switch (v_addr & 0x03e0) {
				case 0x03a0: v_addr ^= 0x7ba0; break;
				case 0x03e0: v_addr &= ~0x73e0; break;
				default: v_addr = (v_addr & ~0x7000) + 0x0020;
			}
		} else {
			v_addr += 0x1000;
		}
	}
}

static void reload_shifters()
{
	at_latchL = at_byte & 1;
    at_latchH = (at_byte & 2) >> 1;

	bg_shiftL = (bg_shiftL & 0xff00) | bgL;
    bg_shiftH = (bg_shiftH & 0xff00) | bgH;
}

static void evaluate_sprites()
{
	INT32 cur_line = (scanline == prerender_line) ? -1 : scanline;
	oam2_cnt = 0;

	memset(sprite_cache, 0, sizeof(sprite_cache));

    for (INT32 i = oamAddr; i < 0x40; i++) {
        INT32 line = cur_line - oam_ram[(i << 2) + 0];
        if (line >= 0 && line < sprite_height) {
			if (oam2_cnt == 8) {
				ppu_status |= status_spovrf;
				if (~NESDips[0] & 1) // DIP option: disable sprite limit
					break;
			}
			oam2[oam2_cnt].idx  = i;
            oam2[oam2_cnt].y    = oam_ram[(i << 2) + 0];
            oam2[oam2_cnt].tile = oam_ram[(i << 2) + 1];
            oam2[oam2_cnt].attr = oam_ram[(i << 2) + 2];
			oam2[oam2_cnt++].x  = oam_ram[(i << 2) + 3];
		}
	}
}

static void load_sprites()
{
	oam_cnt = 0;
	for (INT32 i = 0; i < oam2_cnt; i++) {
		oam[oam_cnt++] = oam2[i];

		UINT32 sprY = scanline - oam[i].y;
		sprY = (oam[i].attr & 0x80) ? ~sprY : sprY; // invert y-bits if y-flipped

		if (sprite_height == 16) {
			ppu_bus_address = ((oam[i].tile & 1) * 0x1000) + ((oam[i].tile & 0xfe) * 16)
				+ ((sprY & 8) << 1) + (sprY & 7);
		} else {
			ppu_bus_address = sprtable_start + (oam[i].tile * 16) + (sprY & 7);
		}

		// sprite_cache[x + (0..7)] = 1, the next 8px have a sprite
		*(UINT32*)(&sprite_cache[oam[i].x + 0]) = 0x01010101;
		*(UINT32*)(&sprite_cache[oam[i].x + 4]) = 0x01010101;

		oam[i].tileL = mapper_chr_read(ppu_bus_address);
		ppu_bus_address += 8;
		oam[i].tileH = mapper_chr_read(ppu_bus_address);

		if (mapper_ppu_clock) mapper_ppu_clock(ppu_bus_address); // Punch-Out (mapper 9) latches on high-byte read (+8)

		if (oam[i].attr & 0x40) { // reverse bits if x-flipped
			oam[i].tileL = bitrev_table[oam[i].tileL];
			oam[i].tileH = bitrev_table[oam[i].tileH];
		}
    }
}

static void draw_and_shift()
{
	if (scanline < 240 && pixel >= (0 + 2) && pixel < (256 + 2)) {
		UINT8 x = pixel - 2; // drawn pixel is 2 cycles behind ppu pixel(cycle)
		UINT8 pix = 0;
		UINT8 sprPal = 0;
		UINT8 sprPri = 0;
		UINT8 spr = 0;
		UINT8 eff_x = 0;

		if (!RENDERING && (v_addr & 0x3f00) == 0x3f00) {
			// https://wiki.nesdev.com/w/index.php/PPU_palettes "The background palette hack"
			pix = v_addr & 0x1f;
		}

		if (x >= bgmasklimit) {
			pix = (get_bit(bg_shiftH, 15 ^ fine_x) << 1) |
				  (get_bit(bg_shiftL, 15 ^ fine_x) << 0);

			if (pix) {
				pix |= ((get_bit(at_shiftH, 7 ^ fine_x) << 1) |
						(get_bit(at_shiftL, 7 ^ fine_x) << 0)) << 2;
			}
        }

		if (sprite_cache[x] && x >= spritemasklimit) {
			for (INT32 i = oam_cnt - 1; i >= 0; i--) {
				if (oam[i].idx == 0xff) // no sprite
					continue;

				eff_x = x - oam[i].x;
				if (eff_x >= 8) // sprite out of view
					continue;

				spr = (get_bit(oam[i].tileH, 7 ^ eff_x) << 1) |
					  (get_bit(oam[i].tileL, 7 ^ eff_x) << 0);

				if (spr == 0) // transparent sprite, ignore
					continue;

				if (oam[i].idx == 0 && pix && x != 0xff) {
					ppu_status |= status_sp0hit;
				}

                spr |= (oam[i].attr & 3) << 2; // add color (attr), 2bpp shift
                sprPal = spr + 0x10; // add sprite color palette-offset
                sprPri = ~oam[i].attr & 0x20; // sprite over bg?
            }
		}

		if (~nBurnLayer & 1) pix = 0; // if tile layer disabled, clear pixel.
        if (sprPal && (pix == 0 || sprPri) && nSpriteEnable & 1) pix = sprPal;

		screen[scanline_row + x] = (pal_ram[pix & 0x1f] & ppu_pal_mask) | ppu_pal_emphasis;
    }

	bg_shiftL <<= 1;
	bg_shiftH <<= 1;
    at_shiftL = (at_shiftL << 1) | at_latchL;
    at_shiftH = (at_shiftH << 1) | at_latchH;
}

static inline void scanlinestate(INT32 state)
{
	if (state == VBLANK) {

		switch (pixel) {
			case 1: // enter VBlank
				ppu_bus_address = v_addr & 0x3fff;
				ppu_status |= status_vblank;

				if (NESMode & ALT_TIMING2) {
					if ((ppu_ctrl & ctrl_nmi) && ppu_no_nmi_this_frame == 0) {
						//bprintf(0, _T("nmi @ frame %d  scanline %d  pixel %d  PPUCTRL %x\n"), nCurrentFrame, scanline, pixel, ctrl.reg);
						M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_AUTO);
					}
					ppu_no_nmi_this_frame = 0;
				}
				break;

			case (6 * 3):
				// 6 CPU-cycles later, do nmi.  fixes boot w/b-wings, bad dudes, gunnac
				// crap on screen with marble madness. (Also passes blargg timing tests)
				if (~NESMode & ALT_TIMING2) {
					if ((ppu_ctrl & ctrl_nmi) && ppu_no_nmi_this_frame == 0) {
						//bprintf(0, _T("nmi @ frame %d  scanline %d  pixel %d  PPUCTRL %x\n"), nCurrentFrame, scanline, pixel, ctrl.reg);
						M6502SetIRQLine(CPU_IRQLINE_NMI, CPU_IRQSTATUS_AUTO);
					}
					ppu_no_nmi_this_frame = 0;
				}
				break;
		}
	}
	else if (state == VISIBLE || state == PRERENDER) {

		// Sprites
		switch (pixel) {
			case 1:
				memset(oam2, 0xff, sizeof(oam2)); // clear secondary oam

				if (state == PRERENDER) {
					ppu_status &= status_openbus; // clear vbl, spr0 hit & overflow bits

					if (oamAddr > 7) { // 2c02 oamram corruption (Huge Insect, Tatakai no Banka pre-revA)
						for (INT32 i = 0; i < 8; i++) {
							oam_ram[i] = oam_ram[(oamAddr & 0xf8) + i];
						}
					}
				}
				break;
			case 257:
				evaluate_sprites();
				if (RENDERING) {
					oamAddr = 0;
				}
				break;
			case 321:
				if (RENDERING) {
					load_sprites();
				} else {
					oam_cnt = 0;
				}
				break;
		}

		// Tiles
		if ( (pixel >= 1 && pixel <= 257) || (pixel >= 321 && pixel <= 337) ) {
			if (pixel != 1) draw_and_shift();
			if (RENDERING) {
				switch (pixel & 7) {
					case 1:
						ppu_bus_address = 0x2000 | (v_addr & 0x0fff); // nametable address
						reload_shifters();
						if (pixel == 257 && RENDERING) {
							// copy horizontal bits from loopy-T to loopy-V
							v_addr = (v_addr & ~0x041f) | (t_addr & 0x041f);
						}
						break;
					case 2:
						nt_byte = read_nt(ppu_bus_address);
						break;
					case 3:
						ppu_bus_address = 0x23c0 | (v_addr & 0x0c00) | ((v_addr >> 4) & 0x38) | ((v_addr >> 2) & 7); // attribute address
						break;
					case 4:
						at_byte = read_nt(ppu_bus_address);
						at_byte >>= ((v_addr & (1 << 6)) >> 4) | (v_addr & (1 << 1));
						break;
					case 5:
						ppu_bus_address = bgtable_start + (nt_byte * 16) + (v_addr >> 12); // background address
						break;
					case 6:
						bgL = mapper_chr_read(ppu_bus_address & 0x1fff);
						break;
					case 7:
						ppu_bus_address = 8 + bgtable_start + (nt_byte * 16) + (v_addr >> 12); // background address
						break;
					case 0:
						bgH = mapper_chr_read(ppu_bus_address & 0x1fff); // needs masking, tc: solstice manages to change bus address to 0x2340 between this and the prev. cycle.
						if (pixel == 256)
							v_scroll();
						else
							h_scroll();
						break;
				}
			}
		}

		if (state == PRERENDER && pixel >= 280 && pixel <= 304 && RENDERING) {
			// copy vertical bits from loopy-T to loopy-V
			v_addr = (v_addr & ~0x7be0) | (t_addr & 0x7be0);
		}

		if (pixel >= 337 && pixel <= 340 && RENDERING) {
			// dummy nt fetches
			ppu_bus_address = 0x2000 | (v_addr & 0x0fff);
		}

		// Signal scanline to mapper: (+18 gets rid of jitter in Kirby, Yume Penguin Monogatari, Klax, ...)
		// Note: was 20, but caused occasional flickering in the rasterized water in "Kira Kira Star Night GOLD" (maybe the others in the series, too?)
		if (pixel == (260+18) /*&& RENDERING*/ && mapper_scanline) mapper_scanline();
		// Mapper callback w/ppu_bus_address - used by mmc2/mmc4 (mapper9/mapper10)
		if (mapper_ppu_clock) mapper_ppu_clock(ppu_bus_address); // ppu callback (visible lines)
    }
}

static void set_scanline(INT32 sl)
{
	scanline = sl;
	scanline_row = scanline * 256;
}

void ppu_cycle()
{
	ppu_framecycles++;	// keep track of cycles/frame since reset
	pixel++; 			// cycle (pixel)/line

	if (pixel > 340) {
		pixel = 0;
		set_scanline(scanline + 1);
		if (scanline > prerender_line) {
			set_scanline(0);
			ppu_frame++;
			ppu_odd ^= 1;
			if (RENDERING && ppu_odd) {
				// this real hw hack was added to the ppu back in the day for better
				// rf/composite output.  since it messes with current cpu:ppu
				// sync. and isn't really necessary w/emu, let's disable it. -dink
				//pixel++;
				//ppu_framecycles++;
			}
        }
	}

	if (scanline >= 0 && scanline <= 239)
		scanlinestate(VISIBLE);
	else if (scanline == 241)
		scanlinestate(VBLANK);
	else if (scanline == prerender_line) {
		scanlinestate(PRERENDER);
	}

	if (mapper_ppu_clockall) mapper_ppu_clockall(ppu_bus_address); // ppu callback (every line)

	if (v_addr_update_delay > 0) {
		v_addr_update_delay--;
		if (v_addr_update_delay == 0) {
			v_addr = v_addr_update;
			if ((scanline >= 240 && scanline < prerender_line) || !RENDERING) {
				ppu_bus_address = v_addr & 0x3fff;
			}
		}
	}
}

static void ppu_run(INT32 cyc)
{
	while (ppu_over < 0) { // we didn't run enough cycles last frame, catch-up!
		cyc++;
		ppu_over++;
	}

	if ((NESMode & IS_PAL) && (mega_cyc_counter % 5) == 0) {
		cyc++;
	}

	while (cyc) {
		if (ppu_over > 0) { // if we're over some cycles on the start of next:
			ppu_over--;     // frame - idle them away
		} else {
			ppu_cycle();
		}
		cyc--;
	}
}

static void ppu_scan(INT32 nAction)
{
	SCAN_VAR(mirroring);
	SCAN_VAR(scanline);
	SCAN_VAR(pixel);
	SCAN_VAR(ppu_frame);

	SCAN_VAR(v_addr);
	SCAN_VAR(t_addr);
	SCAN_VAR(v_addr_update);
	SCAN_VAR(v_addr_update_delay);

	SCAN_VAR(fine_x);
	SCAN_VAR(oamAddr);

	SCAN_VAR(ppu_ctrl);
	SCAN_VAR(ppu_mask);
	SCAN_VAR(ppu_status);

	SCAN_VAR(sprite_height);
	SCAN_VAR(v_incr);
	SCAN_VAR(bgtable_start);
	SCAN_VAR(sprtable_start);
	SCAN_VAR(ppu_pal_mask);
	SCAN_VAR(ppu_pal_emphasis);
	SCAN_VAR(spritemasklimit);
	SCAN_VAR(bgmasklimit);

	SCAN_VAR(ppu_no_nmi_this_frame);
	SCAN_VAR(ppu_odd);
	SCAN_VAR(ppu_bus_address);
	SCAN_VAR(ppu_over);
	SCAN_VAR(ppu_dbus);
	SCAN_VAR(ppu_buffer);
	SCAN_VAR(write_latch);

	SCAN_VAR(nt_byte);
	SCAN_VAR(bgL);
	SCAN_VAR(bgH);
	SCAN_VAR(bg_shiftL);
	SCAN_VAR(bg_shiftH);

	SCAN_VAR(at_byte);
	SCAN_VAR(at_shiftL);
	SCAN_VAR(at_shiftH);
	SCAN_VAR(at_latchL);
	SCAN_VAR(at_latchH);

	// ppu-ram
	SCAN_VAR(nt_ram);
	SCAN_VAR(pal_ram);
	SCAN_VAR(oam_ram);

	if (nAction & ACB_WRITE) {
		set_mirroring(mirroring);
	}
}

static void ppu_reset()
{
	mmc5_nt_ram = &nt_ram[0];

	// start at (around) vblank to remove 1 full frame of input lag
	set_scanline(239); // line on titlescreen of micromachines if starts on 240
	if (NESMode & BAD_HOMEBREW) {
		set_scanline(0); // animal clipper, enables nmi via ppuctrl, if happens during vblank, game will go nuts
	}
	pixel = 0;
	ppu_frame = 0;

	v_addr = 0;
	t_addr = 0;
	fine_x = 0;
	oamAddr = 0;

	ppu_ctrl = 0;
	ppu_mask = 0;
	ppu_status = 0;

	sprite_height = 8;
	v_incr = 1;
	bgtable_start = 0;
	sprtable_start = 0;
	ppu_pal_mask = 0x3f;
	ppu_pal_emphasis = 0;
	spritemasklimit = 0;
	bgmasklimit = 0;

	ppu_odd = 0;
	ppu_no_nmi_this_frame = 0;
	ppu_bus_address = 0;
	ppu_over = 0; // cycles overrun this frame
	ppu_dbus = 0;
	ppu_buffer = 0;

	write_latch = 0;

	ppu_framecycles = 0; // total ran cycles this frame

    memset(nt_ram, 0xff, sizeof(nt_ram));
	memset(pal_ram, 0x00, sizeof(pal_ram));
    memset(oam_ram, 0xff, sizeof(oam_ram));

	memset(oam, 0xff, sizeof(oam));
	memset(oam2, 0xff, sizeof(oam2));

	nt_byte = 0;
	bgL = bgH = 0;
	bg_shiftL = bg_shiftH = 0;

	at_byte = 0;
	at_shiftL = at_shiftH = 0;
	at_latchL = at_latchH = 0;

	oam_cnt = 0;
	oam2_cnt = 0;

	last_palette = DIP_PALETTE;

	UpdatePalettePointer();
}

static void ppu_init(INT32 is_pal)
{
	read_nt = &read_nt_int;
	write_nt = &write_nt_int;
	for (INT32 i = 0; i < 0x100; i++)
		bitrev_table[i] = BITSWAP08(i, 0, 1, 2, 3, 4, 5, 6, 7);

	if (is_pal) {
		nes_frame_cycles = 33248; // pal
		prerender_line = 311;
		nes_ppu_cyc_mult = (UINT32)((float)3.2 * (1 << 12));
	} else {
		nes_frame_cycles = 29781; // ntsc (default)
		nes_ppu_cyc_mult = (UINT32)((float)3.0 * (1 << 12));
		prerender_line = 261;
	}

	screen = (UINT16*)BurnMalloc((256+8) * (256+8) * sizeof(UINT16));

	ppu_reset();
}

static UINT8 GetAvgBrightness(INT32 x, INT32 y)
{
	// Zapper Detection
	const UINT32 rgbcolor = our_palette[screen[(y) * 256 + x] & 0x3f];

	return ((rgbcolor & 0xff) + ((rgbcolor >> 8) & 0xff) + ((rgbcolor >> 16) & 0xff)) / 3;
}

enum {
	ZAP_SENSE		= 0x00,
	ZAP_NONSENSE	= 0x08,
	ZAP_TRIGGER		= 0x10
};

static UINT8 nes_read_zapper()
{
	if (RENDERING == 0 || scanline < 8 || scanline > 240)
		return ZAP_NONSENSE;

	INT32 in_y = ((BurnGunReturnY(0) * 224) / 255);
	INT32 in_x = BurnGunReturnX(0);
	INT32 real_sl = scanline - 8;

	// offscreen check
	if (in_y == 0 || in_y == 224 || in_x == 0 || in_x == 255) {
		return ZAP_NONSENSE;
	}

	for (INT32 yy = in_y - 2; yy < in_y + 2; yy++) {
		if (yy < real_sl-8 || yy > real_sl || yy < 0 || yy > 224) continue;

		for (INT32 xx = in_x - 2; xx < in_x + 2; xx++) {
			if (xx < 0 || xx > 255) continue;
			if (yy == real_sl && xx >= pixel) break; // <- timing is everything, here.
			if (GetAvgBrightness(xx, yy) > 0x88) { // + flux capacitor makes time travel possible
				return ZAP_SENSE;
			}
		}
	}

	return ZAP_NONSENSE;
}

static UINT8 nes_read_joy(INT32 port)
{
	UINT8 ret = 0;

	if ((NESMode & USE_ZAPPER) && port == 1) {
		// NES Zapper on second port (0x4017)
		ret = nes_read_zapper(); // light sensor
		ret |= (ZapperFire) ? ZAP_TRIGGER : 0x00; // trigger

		if (ZapperReload) {
			ZapperReloadTimer = 10;
			ret = ZAP_TRIGGER | ZAP_NONSENSE;
		} else if (ZapperReloadTimer) {
			ret = ZAP_NONSENSE;
		}

	} else {
		if (NESMode & USE_HORI4P) {
			UINT8 a = (JoyShifter[port&1] >> 0) & 0xff; // joy 1,3
			UINT8 b = (JoyShifter[port&1] >> 8) & 0xff; // joy 2,4
			ret  = (a & 1);			// joy 1,2 d0 (FC hardwired controllers)
			ret |= (b & 1) << 1;	// joy 3,4 d1 (controllers connected to HORI4P)
			a >>= 1;                // shift next bit
			b >>= 1;                // "
			a |= 0x80;              // feed empty high bit (active low)
			b |= 0x80;              // "

			JoyShifter[port&1] = (a & 0xff) | ((b << 8) & 0xff00);
		} else {
			ret = JoyShifter[port&1]&1;
			JoyShifter[port&1] >>= 1;   // shift next
			JoyShifter[port&1] |= 0x80000000; // feed high bit so reads past our data return high
		}

		// MIC "blow" hack (read on 1p controller address, yet, mic is on 2p controller!)
		if (NESMode & IS_FDS && port == 0) {
			if (DrvInputs[1] & ((1<<2) | (1<<3))) { // check if 2p select or start pressed. note: famicom 2p controller doesn't have start or select, we use it as a MIC button.
				ret |= 4;
			}
		}

		// note, some games (f.ex: paperboy) relies on the open bus bits in the high nibble
		ret |= cpu_open_bus & 0xf0;
	}

	return ret;
}

static UINT8 nesvs_read_joy(INT32 port)
{ // VS. Unisystem, VS. Zapper handled in psg_io_write() ctrlr strobe
	UINT8 joy = nes_read_joy(port) & 1;

	switch (port) {
		case 0: // 4016: joy + bottom 3 bits of dip + coin
			joy = (joy & 0x01) | ((NESDips[3] & 3) << 3) | (NESCoin[0] << 2);
			break;
		case 1: // 4017: joy + top 6 bits of dip
			joy = (joy & 0x01) | (NESDips[3] & 0xfc);
			break;
	}

	return joy;
}

static UINT8 psg_io_read(UINT16 address)
{
	if (address == 0x4016 || address == 0x4017)
	{	// Gamepad 1 & 2 / Zapper
		if (PPUType > RP2C02) {
			return nesvs_read_joy(address & 1);
		}
		return nes_read_joy(address & 1);
	}

	if (address == 0x4015)
	{
		return nesapuRead(0, address & 0x1f, cpu_open_bus);
	}

	if (address >= 0x4020 && psg_area_read) {
		return psg_area_read(address);
	}

	//bprintf(0, _T("psg_io_read(unmapped) %X\n"), address);

	return cpu_open_bus;
}

static void psg_io_write(UINT16 address, UINT8 data)
{
	if (address == 0x4016)
	{
		if (Cart.Mapper == 99) {
			mapper99_write(address, data);
		}

		if ((JoyStrobe & 1) && (~data & 1)) {
			switch (NESMode & (USE_4SCORE | USE_HORI4P | VS_ZAPPER | VS_REVERSED)) {
				case USE_4SCORE:
					// "Four Score" 4-player adapter (NES / World)
					JoyShifter[0] = DrvInputs[0] | (DrvInputs[2] << 8) | (bitrev_table[0x10] << 16);
					JoyShifter[1] = DrvInputs[1] | (DrvInputs[3] << 8) | (bitrev_table[0x20] << 16);
					break;
				case USE_HORI4P: // mode 2
					// "Hori" 4-player adapter Mode 4 (Japanese / Famicom)
					JoyShifter[0] = DrvInputs[0] | (DrvInputs[2] << 8);
					JoyShifter[1] = DrvInputs[1] | (DrvInputs[3] << 8);
					break;
				case VS_ZAPPER: { // VS. UniSystem Zapper
					UINT8 zap = 0x10;
					zap |= ((~nes_read_zapper() << 3) & 0x40) | (ZapperFire << 7);
					zap |= (ZapperReload) ? 0xc0 : 0x00;
					JoyShifter[0] = zap | 0xffffff00;
					JoyShifter[1] = zap | 0xffffff00;
					break;
				}
				case VS_REVERSED: {
					// some VS. games swap p1/p2 inputs (not select/start aka 0xc0)
					UINT8 in1 = (DrvInputs[1] & 0x0c) | (DrvInputs[0] & ~0x0c);
					UINT8 in0 = (DrvInputs[0] & 0x0c) | (DrvInputs[1] & ~0x0c);
					JoyShifter[0] = in0 | 0xffffff00;
					JoyShifter[1] = in1 | 0xffffff00;
					break;
				}
				default:
					// standard nes controllers
					JoyShifter[0] = DrvInputs[0] | 0xffffff00;
					JoyShifter[1] = DrvInputs[1] | 0xffffff00;
					break;
			}
		}
		JoyStrobe = data;
		return;
	}

	if (address >= 0x4000 && address <= 0x4017)
	{
		nesapuWrite(0, address & 0x1f, data);
		return;
	}

	if (address >= 0x4020 && psg_area_write) {
		psg_area_write(address, data);
		return;
	}

	if (address == 0x4020 && PPUType > RP2C02) return; // 0x4020: ignore coin counter writes on VS. Unisystem

	bprintf(0, _T("psg_io_write(unmapped) %X    %x\n"), address, data);
}

static UINT8 cpu_ram_read(UINT16 address)
{
	return NES_CPU_RAM[address & 0x7FF];
}

static void cpu_ram_write(UINT16 address, UINT8 data)
{
	NES_CPU_RAM[address & 0x7FF] = data;
}

static UINT8 prg_ram_read(UINT16 address) // expansion ram / rom on the cartridge.
{
	if (cart_exp_read) {
		return cart_exp_read(address);
	}

	// some games get irritated (low g-man, mapper 7: double dragon, battle toads) if prg ram exists
	return (NESMode & NO_WORKRAM) ? cpu_open_bus : Cart.WorkRAM[address & Cart.WorkRAMMask];
}

static void prg_ram_write(UINT16 address, UINT8 data)
{
	if (cart_exp_write) {
		cart_exp_write_abort = 0;
		cart_exp_write(address, data);
		if (cart_exp_write_abort) return;
		// fallthrough if abort wasn't set! :)
	}

	if (NESMode & NO_WORKRAM) return;
	Cart.WorkRAM[address & Cart.WorkRAMMask] = data;
}

// cheat system
static UINT8 gg_bit(UINT8 g)
{
	const UINT8 gg_str[0x11] = "APZLGITYEOXUKSVN";

	for (UINT8 i = 0; i < 0x10; i++) {
		if (g == gg_str[i]) {
			return i;
		}
	}
	return 0;
}

static INT32 gg_decode(char *gg_code, UINT16 &address, UINT8 &value, INT32 &compare)
{
	INT32 type = strlen(gg_code);

	if (type != 6 && type != 8) {
		// bad code!
		return 1;
	}

	UINT8 str_bits[8];

	for (UINT8 i = 0; i < type; i++) {
		str_bits[i] = gg_bit(gg_code[i]);
	}

	address = 0x8000 | ((str_bits[1] & 8) << 4) | ((str_bits[2] & 7) << 4) | ((str_bits[3] & 7) << 12) | ((str_bits[3] & 8) << 0) | ((str_bits[4] & 7) << 0) | ((str_bits[4] & 8) << 8) | ((str_bits[5] & 7) << 8);
	value = ((str_bits[0] & 7) << 0) | ((str_bits[0] & 8) << 4) | ((str_bits[1] & 7) << 4);
	compare = -1;

	switch (type) {
		case 6: {
			value |= ((str_bits[5] & 8) << 0);
			break;
		}
		case 8: {
			value |= ((str_bits[7] & 8) << 0);
			compare = ((str_bits[5] & 8) << 0) | ((str_bits[6] & 7) << 0) | ((str_bits[6] & 8) << 4) | ((str_bits[7] & 7) << 4);
			break;
		}
	}

	return 0;
}

static const INT32 cheat_MAX = 0x100;
static INT32 cheats_active = 0;

struct cheat_struct {
	char code[0x10]; // gamegenie code
	UINT16 address;
	UINT8 value;
	INT32 compare; // -1, compare disabled.
};

static cheat_struct cheats[cheat_MAX];

static void nes_add_cheat(char *code) // 6/8 character game genie codes allowed
{
	UINT16 address;
	UINT8 value;
	INT32 compare;

	if (!gg_decode(code, address, value, compare) && cheats_active < (cheat_MAX-1)) {
		strncpy(cheats[cheats_active].code, code, 9);
		cheats[cheats_active].address = address;
		cheats[cheats_active].value = value;
		cheats[cheats_active].compare = compare;
		bprintf(0, _T("cheat #%d (%S) added.  (%x, %x, %d)\n"), cheats_active, cheats[cheats_active].code, address, value, compare);
		cheats_active++;
	} else {
		if (cheats_active < (cheat_MAX-1)) {
			bprintf(0, _T("nes cheat engine: bad GameGenie code %S\n"), code);
		} else {
			bprintf(0, _T("nes cheat engine: too many active!\n"));
		}
	}
}

static void nes_remove_cheat(char *code)
{
	cheat_struct cheat_temp[cheat_MAX];
	INT32 temp_num = 0;

	for (INT32 i = 0; i < cheats_active; i++) {
		if (strcmp(code, cheats[i].code) != 0) {
			memcpy(&cheat_temp[temp_num], &cheats[i], sizeof(cheat_struct));
			temp_num++;
		} else {
			bprintf(0, _T("cheat %S disabled.\n"), cheats[i].code);
		}
	}

	memcpy(cheats, cheat_temp, sizeof(cheats));
	cheats_active = temp_num;
}

static inline UINT8 cheat_check(UINT16 address, UINT8 value)
{
	for (INT32 i = 0; i < cheats_active; i++) {
		if (cheats[i].address == address && (cheats[i].compare == -1 || cheats[i].compare == value)) {
#if FIND_CHEAT_ROMOFFSET
			bprintf(0, _T("%x %x -> %x, prg addy/offset: %x  %x\n"), address, cheats[i].compare, cheats[i].value, l_address, l_offset);
#endif
			return cheats[i].value;
		}
	}

	return value;
}


static UINT8 cpu_bus_read(UINT16 address)
{
	UINT8 ret = 0;
	switch (address & 0xe000)
	{
		case 0x0000:  //    0 - 1fff
			ret = cpu_ram_read(address); break;
		case 0x2000:  // 2000 - 3fff
			ret = ppu_read(address); break;
		case 0x4000:  // 4000 - 5fff
			ret = psg_io_read(address); break;
		case 0x6000:  // 6000 - 7fff
			ret = prg_ram_read(address); break;
		default:      // 8000 - ffff
			ret = mapper_prg_read(address); break;
	}

	ret = cheat_check(address, ret);

	cpu_open_bus = ret;

	return ret;
}


#define DEBUG_DMA 0

static void cpu_bus_write(UINT16 address, UINT8 data)
{
	cpu_open_bus = data;

	if (address == 0x4014) { // OAM_DMA
#if DEBUG_DMA
		bprintf(0, _T("DMA, tcyc %d   scanline: %d    pixel: %d\n"), M6502TotalCycles(), scanline, pixel);
#endif
		UINT8 byte = 0;
		M6502Stall(1);		// 1 dummy cyc

		if (M6502TotalCycles() & 1) {
			M6502Stall(1);	// 1 if starts on odd cycle
		}

		for (INT32 i = 0; i < 256; i++) {
			M6502Stall(1); // 1 for cpu_read
			byte = cpu_bus_read(data * 0x100 + i);
			M6502Stall(1); // 1 for cpu_write to ppu
			ppu_write(0x2004, byte);
		}
#if DEBUG_DMA
		bprintf(0, _T("..end-DMA, tcyc %d   scanline: %d    pixel: %d\n"), M6502TotalCycles(), scanline, pixel);
#endif
		return;
	}

	switch (address & 0xe000) {
		case 0x0000: //    0 - 1fff
			cpu_ram_write(address, data); break;
		case 0x2000: // 2000 - 3fff
			ppu_write(address, data); break;
		case 0x4000: // 4000 - 5fff
			psg_io_write(address, data); break;
		case 0x6000: // 6000 - 7fff
			prg_ram_write(address, data); break;
		default:     // 8000 - ffff
			if (mapper_write) mapper_write(address, data);
			return;
	}
}

static INT32 DrvDoReset()
{
	if (RESETMode == RESET_POWER) {
		memset(NES_CPU_RAM, 0x00, 0x800);  // only cleared @ power-on

		if (NESMode & RAM_RANDOM) { // Some games prefer random RAM @ power-on
			UINT8 Pattern[0x08] = { 0x01, 0x5a, 0x01, 0x49, 0xe5, 0xf8, 0xa5, 0x10 };

			for (INT32 i = 0; i < 0x800; i++) {
				NES_CPU_RAM[i] = Pattern[i & 0x07];
			}
		}
	}
	M6502Open(0);
	M6502Reset();
	M6502Close();
	nesapuReset();
	//	if (RESETMode == RESET_POWER) {
	// Nebs 'n Debs thinks we're in 50hz/PAL mode on reset if we don't reset PPU here..
	// note: This is actually due to a bug in the game
		ppu_reset();
	//	}
	mapper_reset();

	JoyShifter[0] = JoyShifter[1] = 0xffffffff;
	JoyStrobe = 0;
	ZapperReloadTimer = 0;

	cyc_counter = 0;
	mega_cyc_counter = 0;

	{
		INT32 nAspectX, nAspectY;
		BurnDrvGetAspect(&nAspectX, &nAspectY);
		if (NESDips[1] & 1) { // 4:3
			if (nAspectX != 4) {
				bprintf(0, _T("*  NES: Changing to 4:3 aspect\n"));
				BurnDrvSetAspect(4, 3);
				Reinitialise();
			}
		} else { // no aspect ratio handling
			if (nAspectX != SCREEN_WIDTH) {
				bprintf(0, _T("*  NES: Changing to pixel aspect\n"));
				BurnDrvSetAspect(SCREEN_WIDTH, SCREEN_HEIGHT);
				Reinitialise();
			}
		}
	}

	if (PPUType > RP2C02) {
		HiscoreReset();
	}

	return 0;
}

INT32 NESInit()
{
	nes_init_cheat_functions(nes_add_cheat, nes_remove_cheat);

	GenericTilesInit();

	NES_CPU_RAM = (UINT8*)BurnMalloc(0x800);

	cheats_active = 0;

	struct BurnRomInfo ri;
	char *romname = NULL;
	BurnDrvGetRomInfo(&ri, 0);
	BurnDrvGetRomName(&romname, 0, 0);
	UINT32 length = ri.nLen;
	UINT32 is_FDS = (strstr(romname, ".fds") != 0);
	bprintf(0, _T("ROM Name: %S\n"), romname);
	rom = BurnMalloc((length < 0x100000) ? 0x100000 : length);

	bprintf(0, _T("ROM Length: %d\n"), ri.nLen);
	RESETMode = RESET_POWER;

	if (is_FDS) {
		if (BurnLoadRom(rom + 0x6000, 0x80, 1)) return 1; // bios @ 0xe000
		if (fds_load(rom, ri.nLen, ri.nCrc)) return 1;
	} else {
		if (BurnLoadRom(rom, 0, 1)) return 1;

#if 0
#include <nes_sha1db.cpp>
#endif

		if (cartridge_load(rom, ri.nLen, ri.nCrc)) return 1;
	}

	BurnSetRefreshRate((NESMode & IS_PAL) ? 50.00 : 60.00);

	M6502Init(0, TYPE_N2A03);
	M6502Open(0);
	M6502SetWriteHandler(cpu_bus_write);
	M6502SetReadHandler(cpu_bus_read);
	M6502Close();

	ppu_init((NESMode & IS_PAL) ? 1 : 0);

	if (NESMode & IS_PAL) {
		nesapuInitPal(0, 1798773, 0);
	} else {
		nesapuInit(0, 1798773, 0);
	}
	if (NESMode & APU_HACKERY) {
		nesapu4017hack(1);
	}
	nesapuSetAllRoutes(0, 2.40, BURN_SND_ROUTE_BOTH);

	DrvDoReset();

	return 0;
}

INT32 NES4ScoreInit()
{
	INT32 rc = NESInit();

	NESMode |= USE_4SCORE;

	bprintf(0, _T("*  FourScore 4 Player device.\n"));

	return rc;
}

INT32 NESHori4pInit()
{
	INT32 rc = NESInit();

	NESMode |= USE_HORI4P;

	bprintf(0, _T("*  Hori 4 Player device (Mode 2).\n"));

	return rc;
}

INT32 NESReversedInit()
{
	INT32 rc = NESInit();

	NESMode |= VS_REVERSED;

	bprintf(0, _T("*  Inputs reversed (p1/p2 -> p2/p1).\n"));

	return rc;
}

INT32 NESZapperInit()
{
	INT32 rc = NESInit();

	NESMode |= (PPUType == RP2C02) ? USE_ZAPPER : VS_ZAPPER;
	BurnGunInit(1, true);

	bprintf(0, _T("*  Zapper on Port #2.\n"));

	return rc;
}

INT32 topriderInit()
{
	INT32 rc = NESInit();

	if (!rc) {
		// Top Rider size / crc: 163856, 0xca1a395a
		// Patch in standard controller -dink feb.25, 2021
		// Game Genie code: IUEOKOAL
		// lda $494
		// ora $311
		// and #$10
		//*bne 99c5 ; start pressed, next screen
		// jsr 9f68 ; check if code entered on controller #2
		// bcs 99c2 ; yes? start game with normal controller in port #1
		// ...
		//*d0 38  bne 99c5 -> d0 35  bne 99c2

		Cart.PRGRom[0x1998c] = 0x35;
	}

	return rc;
}

// stereo effector (delay line+comb filter)
struct ms_ring {
	INT16 *l;
	INT16 *r;
	INT32 ring_size;
	INT32 inpos;
	INT32 outpos_l;
	INT32 outpos_r;

	void exit() {
		if (ring_size || l || r) {
			BurnFree(l);
			BurnFree(r);
			ring_size = 0;
			bprintf(0, _T("ms_ring exited.\n"));
		}
	}

	void init() {
		ring_size = (INT32)((double)14 / 1000 * nBurnSoundRate); // 14ms ring buffer

		l = (INT16*)BurnMalloc(ring_size * sizeof(INT16));
		r = (INT16*)BurnMalloc(ring_size * sizeof(INT16));

		for (INT32 i = 0; i < ring_size; i++) {
			write(0, 0);
		}
		inpos = 0; // position in @ beginning of ring, out @ end
		outpos_l = 1;
		outpos_r = 1;
		bprintf(0, _T("ms_ring initted (%d entry ringbuffer)\n"), ring_size);
	}

	INT32 needs_init() {
		return (l == NULL || r == NULL || ring_size == 0);
	}

	void write(INT16 sam_l, INT16 sam_r) {
		l[inpos] = sam_l;
		r[inpos] = sam_r;
		inpos = (inpos + 1) % ring_size;
	}

	INT16 read_r() {
		INT16 temp = r[outpos_r];
		outpos_r = (outpos_r + 1) % ring_size;
		return temp;
	}

	INT16 read_l() {
		INT16 temp = l[outpos_l];
		outpos_l = (outpos_l + 1) % ring_size;
		return temp;
	}

	void process(INT16 *buffer, INT32 samples) {
		if (needs_init()) {
			init();
		}

		for (INT32 i = 0; i < samples; i++) {
			write(buffer[i * 2 + 0], buffer[i * 2 + 1]);
			INT16 sam_mask = ((read_l(), read_r()) / 2) * 0.75;
			INT16 sam_now = (buffer[i * 2 + 0] + buffer[i * 2 + 1]) / 2;
			buffer[i * 2 + 0] = sam_now + sam_mask;
			buffer[i * 2 + 1] = sam_now - sam_mask;
		}
	}
};

static ms_ring ms_delay;

// end stereoeffect

INT32 NESExit()
{
	GenericTilesExit();
	M6502Exit();
	nesapuExit();

	BurnFree(screen);

	// Mapper EXT-hardware exits
	if (Cart.Mapper == 69) { // SunSoft fme-7 (5b) audio expansion - ay8910. mapper69
		AY8910Exit(0);
	}

	if (Cart.Mapper == 85) { // VRC7 audio expansion - ym2413
		BurnYM2413Exit();
	}

	if (NESMode & FLASH_EEPROM) {
		TCHAR desc[64];
		_stprintf(desc, _T("Mapper %d"), Cart.Mapper);

		// Save IPS patch (aka: flasheeprom-saves @ exit)
		SaveIPSPatch(desc, Cart.CartOrig, Cart.Cart, Cart.CartSize);
	}

	if (Cart.FDSMode) {
		// exit saver:
		SaveIPSPatch(_T("FDS DISK"), Cart.FDSDiskRawOrig, Cart.FDSDiskRaw, Cart.FDSDiskRawSize);

		BurnFree(Cart.FDSDiskRaw);
		BurnFree(Cart.FDSDiskRawOrig);

		BurnLEDExit();
	}

	if (NESMode & (USE_ZAPPER | VS_ZAPPER))
		BurnGunExit();

	BurnFree(Cart.CartOrig);
	BurnFree(rom);
	BurnFree(NES_CPU_RAM);
	BurnFree(Cart.WorkRAM);
	BurnFree(Cart.CHRRam);

	ms_delay.exit();

	NESMode = 0;

	return 0;
}

static UINT32 EmphRGB(INT32 pal_idx, UINT8 type, UINT32 *palette)
{
/*
	dink ppu color emphasis notes
	ppumask	register	(reg >> 5) (r/g swapped for PAL)

	red		20		 	1
	green	40			2
	blue	80			4

	// All possible combinations, palette offset, (reg >> 5)
	normal	0-3f		0
	r		40-7f		1
	g		80-bf		2
	rg		c0-ff		3
	b		100 - 13f	4
	rb		140 - 17f	5
	gb		180 - 1bf	6
	rgb		1c0 - 1ff	7
*/

	UINT64 er = 1.0 * (1 << 16);
	UINT64 eg = 1.0 * (1 << 16);
	UINT64 eb = 1.0 * (1 << 16);
	UINT32 EMPH   = 1.2 * (1 << 16);
	UINT32 DEEMPH = 0.8 * (1 << 16);
	if (type & 1) {
		er = (er *   EMPH) >> 16;
		eg = (eg * DEEMPH) >> 16;
		eb = (eb * DEEMPH) >> 16;
	}
	if (type & 2) {
		er = (er * DEEMPH) >> 16;
		eg = (eg *   EMPH) >> 16;
		eb = (eb * DEEMPH) >> 16;
	}
	if (type & 4) {
		er = (er * DEEMPH) >> 16;
		eg = (eg * DEEMPH) >> 16;
		eb = (eb *   EMPH) >> 16;
	}

	UINT32 r = (((palette[pal_idx & 0x3f] >> 16) & 0xff) * er) >> 16;
	if (r > 0xff) r = 0xff;
	UINT32 g = (((palette[pal_idx & 0x3f] >>  8) & 0xff) * eg) >> 16;
	if (g > 0xff) g = 0xff;
	UINT32 b = (((palette[pal_idx & 0x3f] >>  0) & 0xff) * eb) >> 16;
	if (b > 0xff) b = 0xff;

	return BurnHighCol(r, g, b, 0);
}

static void DrvCalcPalette()
{
	UpdatePalettePointer();

	// Normal NES Palette
	for (INT32 i = 0; i < 0x40; i++) {
		DrvPalette[i] = BurnHighCol((our_palette[i] >> 16) & 0xff, (our_palette[i] >> 8) & 0xff, our_palette[i] & 0xff, 0);
	}

	// Emphasized Palettes (all combinations, see comments-table in EmphRGB)
	for (INT32 i = 0x40; i < 0x200; i++) {
		DrvPalette[i] = EmphRGB(i, i >> 6, our_palette);
	}

	// Palette for the FDS Swap Disk icon
	for (INT32 fadelv = 0; fadelv < 0x10; fadelv++) {
		for (INT32 i = 0x200; i < 0x210; i++) {
			DrvPalette[i + (fadelv * 0x10)] = BurnHighCol(disk_ab_pal[(i & 0xf) * 3 + 2] / (fadelv + 1), disk_ab_pal[(i & 0xf) * 3 + 1] / (fadelv + 1), disk_ab_pal[(i & 0xf) * 3 + 0] / (fadelv + 1), 0);
		}
	}
}

INT32 NESDraw()
{
	if (NESRecalc || last_palette != DIP_PALETTE) {
		DrvCalcPalette();
		NESRecalc = 0;
		last_palette = DIP_PALETTE;
	}

	const INT32 start_y = (NESMode & SHOW_OVERSCAN) ? 0 : 8;
	for (INT32 y = 0; y < nScreenHeight; y++) {
		for (INT32 x = 0; x < 256; x++) {
			pTransDraw[y * nScreenWidth + x] = screen[(y + start_y) * 256 + x];
		}
	}

	if ((NESMode & IS_FDS) && (FDSFrameAction == SWAP_DISK || FDSFrameAction == FA_FADEOUT)) {
		static INT32 fader = 0;
		if (FDSFrameAction == SWAP_DISK) fader = 0;
		if (FDSFrameAction == FA_FADEOUT && ((FDSFrameCounter%2)==0)) {
			if (fader < 0x8)
				fader++;
		}

		switch (FDSFrameDiskIcon) {
			case 0:
				RenderCustomTile(pTransDraw, 38, 18, 0, 109/*x*/, 100/*y*/, 0, 8, 0x200 + (fader * 0x10), &disk_ab[0]);
				break;
			case 1:
				RenderCustomTile(pTransDraw, 38, 18, 0, 109/*x*/, 100/*y*/, 0, 8, 0x200 + (fader * 0x10), &disk_ab[18*38]);
				break;
		}
	}

	BurnTransferCopy(DrvPalette);

	if (NESMode & (USE_ZAPPER | VS_ZAPPER))
		BurnGunDrawTargets();

	if (NESMode & IS_FDS)
		BurnLEDRender();

	return 0;
}

static void clear_opposites(UINT8 &inpt)
{
	// some games will straight-up crash or go berzerk if up+down or left+right
	// is pressed.  This can easily happen when playing via kbd or severly
	// worn gamepad.

	if ((inpt & ( (1 << 4) | (1 << 5) )) == ((1 << 4) | (1 << 5)) )
		inpt &= ~((1 << 4) | (1 << 5)); // up + down pressed, cancel both

	if ((inpt & ( (1 << 6) | (1 << 7) )) == ((1 << 6) | (1 << 7)) )
		inpt &= ~((1 << 6) | (1 << 7)); // left + right pressed, cancel both
}

#define DEBUG_CYC 0

//#define nes_frame_cycles 29781(ntsc) 33248(pal)

INT32 nes_scanline()
{
	return scanline;
}

INT32 NESFrame()
{
#if DEBUG_CYC
	bprintf(0, _T("enter frame:  scanline %d   pixel %d  ppucyc %d    cyc_counter %d\n"), scanline, pixel, ppu_framecycles, cyc_counter);
#endif
	if (NESDrvReset)
	{
		RESETMode = RESET_BUTTON;
		DrvDoReset();
	}

	{	// compile inputs!
		DrvInputs[0] = DrvInputs[1] = 0x00;
		DrvInputs[2] = DrvInputs[3] = 0x00;
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (NESJoy1[i] & 1) << i;
			DrvInputs[1] ^= (NESJoy2[i] & 1) << i;
			DrvInputs[2] ^= (NESJoy3[i] & 1) << i;
			DrvInputs[3] ^= (NESJoy4[i] & 1) << i;
		}

		clear_opposites(DrvInputs[0]);
		clear_opposites(DrvInputs[1]);
		clear_opposites(DrvInputs[2]);
		clear_opposites(DrvInputs[3]);

		if (NESMode & (USE_ZAPPER | VS_ZAPPER)) {
			BurnGunMakeInputs(0, ZapperX, ZapperY);

			if (ZapperReloadTimer) {
				ZapperReloadTimer--;
			}
		}

		if (NESMode & IS_FDS) {
			FDS_Insert(FDSEject);

			if (~NESDips[0] & 2) {
				FDS_FrameTicker();
				FDS_SwapSidesAuto(FDSSwap);
			} else {
				FDS_SwapSides(FDSSwap);
			}
		}
	}

	M6502Open(0);
	M6502NewFrame();
	M6502Idle(cyc_counter);
	// cpucyc, a stand-in for M6502TotalCycles(), gain a small amount of perf w/this
	INT32 cpucyc = cyc_counter;
	cyc_counter = 0;
#if DEBUG_CYC
	INT32 last_ppu = ppu_framecycles;
#endif
	if (ppu_over > 0) { // idle away extra ppu cycles
		ppu_framecycles = ppu_over;
		ppu_over = 0;
	} else {
		ppu_framecycles = 0;
	}

	for (INT32 i = 0; i < nes_frame_cycles; i++)
	{
		cyc_counter ++; // frame-based
		mega_cyc_counter ++; // "since reset"-based

		nesapu_runclock(cyc_counter - 1);  // clock dmc & external sound (n163, vrc, fds)

		if ((cyc_counter - cpucyc) > 0)	{
			cpucyc += M6502Run(cyc_counter - cpucyc);
			// to debug game hard-lock: uncomment and press n 4 times to get pc.
			// tip: 99.9% its either a mapper bug or game needs ALT_TIMING flag
#if defined (FBNEO_DEBUG)
			extern int counter;
			if (counter == -4) bprintf(0, _T("PC:  %x   tc: %d   cyc_ctr: %d\n"), M6502GetPC(-1), M6502TotalCycles(), cyc_counter);
#endif
		}

		mapper_run();

		const INT32 p_cyc = ((cyc_counter * nes_ppu_cyc_mult) >> 12) - ppu_framecycles;
		if (p_cyc > 0) {
			ppu_run(p_cyc);
		}
	}

	if (NESMode & ALT_TIMING) {
		ppu_framecycles--;
	}
	if (NESMode & IS_PAL) {
		ppu_over = ppu_framecycles - 106392;
	} else {
		ppu_over = ppu_framecycles - ((~NESMode & ALT_TIMING) ? 89342 : 89343);
	}

#if DEBUG_CYC
	INT32 cc = cyc_counter;
#endif

	cyc_counter = M6502TotalCycles() - nes_frame_cycles; // the overflow of cycles for next frame to idle away

#if DEBUG_CYC
	bprintf(0, _T("6502 cycles ran: %d   cyc_counter %d   rollover: %d    ppu.over %d   ppu.framecyc %d    last_ppu %d\n"), M6502TotalCycles(), cc, cyc_counter, ppu_over, ppu_framecycles, last_ppu);
#endif

	if (pBurnSoundOut) {
		nesapuSetDMCBitDirection(NESDips[1] & 8);
		nesapuUpdate(0, pBurnSoundOut, nBurnSoundLen);
		BurnSoundDCFilter();

		// Mapper EXT-hardware sound renders
		if (Cart.Mapper == 69) { // SunSoft fme-7 (5b) audio expansion - ay8910. mapper69
			AY8910Render(pBurnSoundOut, nBurnSoundLen);
		}

		if (Cart.Mapper == 85) { // VRC7 audio expansion - YM2413
			BurnYM2413Render(pBurnSoundOut, nBurnSoundLen);
		}

		if (NESDips[1] & 0x02) {
			ms_delay.process(pBurnSoundOut, nBurnSoundLen);
		}
	}
	M6502Close();

	if (pBurnDraw) {
		NESDraw();
	}

	return 0;
}

INT32 NESScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		M6502Scan(nAction);
		nesapuScan(nAction, pnMin);

		SCAN_VAR(cpu_open_bus);
		SCAN_VAR(cyc_counter);
		SCAN_VAR(JoyShifter);
		SCAN_VAR(JoyStrobe);
		SCAN_VAR(ZapperReloadTimer);

		ScanVar(NES_CPU_RAM, 0x800, "CPU Ram");
		ScanVar(Cart.WorkRAM, Cart.WorkRAMSize, "Work Ram");
		ScanVar(Cart.CHRRam, Cart.CHRRamSize, "CHR Ram");

		mapper_scan(nAction, pnMin);

		ppu_scan(nAction);

		// Mapper EXT-hardware sound scans
		if (Cart.Mapper == 69) { // SunSoft fme-7 (5b) audio expansion - ay8910. mapper69
			AY8910Scan(nAction, pnMin);
		}
		if (Cart.Mapper == 85) { // VRC7 audio expansion - YM2413
			BurnYM2413Scan(nAction, pnMin);
		}

		if (NESMode & (USE_ZAPPER | VS_ZAPPER))
			BurnGunScan();

		if (nAction & ACB_WRITE) {
			// save for later?
		}
	}

	if (nAction & ACB_NVRAM) {
		if (Cart.BatteryBackedSRAM) {
			ScanVar(Cart.WorkRAM, Cart.WorkRAMSize, "s-ram");
		}

		if (mapper_scan_cb_nvram) {
			mapper_scan_cb_nvram();
		}
	}

	return 0;
}
