// FB Alpha Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion / Pac-Man - 25th Anniversary Edition driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z180_intf.h"
#include "namco_snd.h"
#include "dac.h"
#include "eeprom.h"
#include "watchdog.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ180ROM;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ180RAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvSprLut;
static UINT8 *DrvSprGfx;
static UINT8 *Drv48000RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 game_selected;
static UINT8 stars_seed[2];
static UINT8 stars_ctrl;
static UINT8 global_flip;
static UINT8 irq_mask;
static UINT8 _47100_val;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static UINT32 sprite_pal_base = 0;

static struct BurnInputInfo Pacgal20InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start (right)",	BIT_DIGITAL,	DrvJoy2 + 4,	"p2 start"	},
	{"P1 Start (left)",	BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 1"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"	},
	{"P2 Start (right)",	BIT_DIGITAL,	DrvJoy2 + 6,	"p4 start"	},
	{"P2 Start (left)",	BIT_DIGITAL,	DrvJoy2 + 7,	"p3 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service Mode",	BIT_DIGITAL,    DrvJoy3 + 7,    "diag"		},
};

STDINPUTINFO(Pacgal20)

static void set_bank(INT32 select)
{
	if (select == 0)
	{
		Z180MapMemory(DrvZ180ROM + 0x08000, 0x48000, 0x49fff, MAP_ROM);
		Z180MapMemory(NULL, 0x48000, 0x49fff, MAP_WRITE);
	}
	else
	{
		Z180MapMemory(DrvVidRAM,   0x48000, 0x487ff, MAP_RAM);
		Z180MapMemory(Drv48000RAM, 0x48800, 0x49fff, MAP_RAM);
	}
}

static UINT8 __fastcall pacgal20_read(UINT32 address)
{
	switch (address)
	{
		case 0x47100:
			return _47100_val;
	}

	return 0;
}

static void __fastcall pacgal20_write(UINT32 address, UINT8 data)
{
	if (address >= 0x45040 && address <= 0x4505f) {
		NamcoSoundWrite(address & 0x1f, data);
	}

	if (address >= 0x45000 && address <= 0x450ff) {
		DrvZ180RAM[address - 0x44800] = data;
		return;
	}

	if (address >= 0x45f00 && address <= 0x45fff) {
		namcos1_custom30_write(address & 0xff, data);
		return;
	}

	switch (address)
	{
		case 0x47100:
			_47100_val = data;
		return;
	}
}

static void __fastcall pacgal20_write_port(UINT32 address, UINT8 data)
{
	address &= 0xff;

	if (address <= 0x7f) return; // z180 internal regs

	switch (address)
	{
		case 0x80:
			BurnWatchdogWrite();
		return;

		case 0x81:
			// timer_pulse_w (unemulated)
		return;

		case 0x82:
			irq_mask = data & 1;
			if (~data & 1) Z180SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x85:
		case 0x86:
			stars_seed[address - 0x85] = data;
		return;

		case 0x87:
			EEPROMWriteBit((data & 0x80) ? 1 : 0);
			EEPROMSetCSLine((data & 0x20) ? 0  : 1);
			EEPROMSetClockLine((data & 0x40) ? 1 : 0);
		return;

		case 0x88:
			game_selected = data & 1;
			set_bank(data & 1);
		return;

		case 0x89:
			DACSignedWrite(0,data);
		return;

		case 0x8a:
			stars_ctrl = data;
		return;

		case 0x8b:
			global_flip = data;
		return;

		case 0x8f:
			// coin_counter_w
		return;
	}
}
		
static UINT8 __fastcall pacgal20_read_port(UINT32 address)
{
	address &= 0xff;

	if (address <= 0x7f) return 0; // z180 internal regs

	switch (address)
	{
		case 0x80:
		case 0x81:
		case 0x82:
			return DrvInputs[address & 3];

		case 0x87:
			return EEPROMRead() ? 0x80 : 0;
	}

	return 0;
}

static INT32 DrvSyncDAC()
{
	return (INT32)(float)(nBurnSoundLen * (Z180TotalCycles() / (18432000.000 / (nBurnFPS / 100.000))));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	Z180Open(0);
	Z180Reset();
	set_bank(0);
	Z180Close();

	EEPROMReset();

	BurnWatchdogReset();

	NamcoSoundReset();
	DACReset();

	stars_seed[0] = stars_seed[1] = 0;
	stars_ctrl = 0;
	global_flip = 0;
	irq_mask = 0;
	_47100_val = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ180ROM	= Next; Next += 0x040000;

	DrvColPROM	= Next; Next += 0x008000;

	DrvPalette	= (UINT32*)Next; Next += 0x3040 * sizeof(UINT32);

	AllRam		= Next;

	DrvZ180RAM	= Next; Next += 0x001800;
	DrvCharRAM	= Next; Next += 0x001000;
	DrvVidRAM	= Next; Next += 0x000800;
	DrvSprRAM	= Next; Next += 0x001f00;
	DrvSprLut	= Next; Next += 0x000100;
	DrvSprGfx	= Next; Next += 0x002000;
	Drv48000RAM	= Next; Next += 0x002000;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

const eeprom_interface eeprom_interface_20pacgal =
{
	7,                // address bits
	8,                // data bits
	"*110",           // read command
	"*101",           // write command
	0,                // erase command
	"*10000xxxxx",    // lock command
	"*10011xxxxx",    // unlock command
	1, // 1?
	0
};

static INT32 DrvInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ180ROM,  0, 1)) return 1;

		if (BurnLoadRom(DrvColPROM,  1, 1)) return 1;
	}

	Z180Init(0);
	Z180Open(0);
	Z180MapMemory(DrvZ180ROM,		0x00000, 0x3ffff, MAP_ROM);
	Z180MapMemory(DrvVidRAM,		0x44000, 0x447ff, MAP_RAM);
	Z180MapMemory(DrvZ180RAM,		0x44800, 0x44fff, MAP_RAM);
	Z180MapMemory(DrvZ180RAM + 0x0800,	0x45000, 0x450ff, MAP_ROM);
	Z180MapMemory(DrvZ180RAM + 0x0900,	0x45100, 0x45eff, MAP_RAM);
	Z180MapMemory(DrvZ180ROM + 0xa000,	0x4a000, 0x4ffff, MAP_ROM);
	Z180MapMemory(DrvCharRAM,		0x46000, 0x46fff, MAP_WRITE);
	Z180MapMemory(DrvSprGfx,		0x4c000, 0x4dfff, MAP_WRITE);
	Z180MapMemory(DrvSprRAM,		0x4e000, 0x4feff, MAP_WRITE);
	Z180MapMemory(DrvSprLut,		0x4ff00, 0x4ffff, MAP_WRITE);
	Z180SetReadHandler(pacgal20_read);
	Z180SetWriteHandler(pacgal20_write);
	Z180SetReadPortHandler(pacgal20_read_port); 
	Z180SetWritePortHandler(pacgal20_write_port); 
	Z180Close();

	NamcoSoundInit(73728000 / 4 / 6 / 32, 3, 0);
	NacmoSoundSetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	DACInit(0, 0, 1, DrvSyncDAC);
	DACSetRoute(0, 0.90, BURN_SND_ROUTE_BOTH);

	BurnWatchdogInit(DrvDoReset, 180);

	EEPROMInit(&eeprom_interface_20pacgal);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	Z180Exit();
	DACExit();

	EEPROMExit();

	NamcoSoundExit();

	BurnFree (AllMem);

	sprite_pal_base = 0;

	GenericTilesExit();

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 offs = 0; offs < 0x2000 ;offs++)
	{
		INT32 bit0 = (DrvColPROM[offs] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[offs] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[offs] >> 2) & 0x01;
		INT32 r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit0 = (DrvColPROM[offs] >> 3) & 0x01;
		bit1 = (DrvColPROM[offs] >> 4) & 0x01;
		bit2 = (DrvColPROM[offs] >> 5) & 0x01;
		INT32 g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		bit1 = (DrvColPROM[offs] >> 6) & 0x01;
		bit2 = (DrvColPROM[offs] >> 7) & 0x01;
		INT32 b = 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[offs] = BurnHighCol(r, g, b, 0);
	}

	for (INT32 offs = 0; offs < 64; offs++)
	{
		static const INT32 map[4] = { 0x00, 0x47, 0x97 ,0xde };

		INT32 r = map[(offs >> 0) & 0x03];
		INT32 g = map[(offs >> 2) & 0x03];
		INT32 b = map[(offs >> 4) & 0x03];

		DrvPalette[0x2000 + offs] = BurnHighCol(r, g, b, 0);
		DrvPalette[0x3000 + offs] = DrvPalette[0x2000 + offs];
	}
}

static void draw_stars()
{
	if ((stars_ctrl >> 5) & 1)
	{
		UINT16 lfsr =   stars_seed[0] + stars_seed[1] * 256;
		UINT8 feedback = (stars_ctrl >> 6) & 1;
		UINT16 star_seta = 0x3fc0 | (((stars_ctrl >> 3) & 0x01) << 14);
		UINT16 star_setb = 0x3fc0 | (((stars_ctrl >> 3) & 0x02) << 14);
		INT32 cnt = 0;

		for (INT32 clock = 0; clock < nScreenWidth * nScreenHeight; clock++)
		{
			INT32 x = clock % nScreenWidth;
			INT32 y = clock / nScreenWidth;

			INT32 carryout = ((lfsr >> 4) ^ feedback ^ 1) & 1;
			feedback = (lfsr >> 15) & 1;
			lfsr = (lfsr << 1) | carryout;

			if (((lfsr & 0xffc0) == star_seta) || ((lfsr & 0xffc0) == star_setb))
			{
				if (y >= 0 && y < nScreenHeight)
					pTransDraw[(y * nScreenWidth) + x] = 0x2000 + (lfsr & 0x3f);
				cnt++;
			}
		}
	}
}

static void draw_chars()
{
	INT32 flip = global_flip & 0x01;

	for (INT32 offs = 0; offs < 0x400; offs++)
	{
		INT32 y, x;

		UINT8 *gfx = DrvCharRAM + (DrvVidRAM[offs] << 4);
		UINT32 color_base = ((DrvVidRAM[0x0400 | offs] & 0x3f) << 2);

		if ((offs & 0x03c0) == 0)
		{
			y = (offs & 0x1f) - 2;
			x = (offs >> 5) + 34;
		}
		else if ((offs & 0x03c0) == 0x3c0)
		{
			y = (offs & 0x1f) - 2;
			x = (offs >> 5) - 30;
		}
		else
		{
			y = (offs >> 5) - 2;
			x = (offs & 0x1f) + 2;
		}

		if ((y < 0) || (y > 27)) continue;

		y = y << 3;
		x = x << 3;

		if (flip)
		{
			y = nScreenHeight - 1 - y;
			x = nScreenWidth - 1 - x;
		}

		for (INT32 sy = 0; sy < 8; sy++)
		{
			int x_sav = x;

			UINT16 data = (gfx[8] << 8) | gfx[0];

			for (INT32 sx = 0; sx < 8; sx++)
			{
				UINT8 col = ((data & 0x8000) >> 14) | ((data & 0x0800) >> 11);

				if (col != 0)
					pTransDraw[(y * nScreenWidth) + x] = (color_base | col) << 4;

				if (flip)
					x = x - 1;
				else
					x = x + 1;

				if (sx == 0x03)
					data = data << 5;
				else
					data = data << 1;
			}

			if (flip)
				y = y - 1;
			else
				y = y + 1;

			x = x_sav;

			gfx = gfx + 1;
		}
	}
}

static void draw_sprite(int y, int x, UINT8 code, UINT8 color, int flip_y, int flip_x)
{
	INT32 pen_base = (color & 0x1f) << 2;
	pen_base += sprite_pal_base;

	if (flip_y) y = y + 0x0f;
	if (flip_x) x = x + 0x0f;

	for (INT32 sy = 0; sy < 0x10; sy++)
	{
		INT32 x_sav = x;

		if ((y >= 0) && (y < nScreenHeight))
		{
			UINT32 gfx_offs = ((code & 0x7f) << 6) | (sy << 2);

			gfx_offs = (gfx_offs & 0x1f83) | ((gfx_offs & 0x003c) << 1) | ((gfx_offs & 0x0040) >> 4);

			UINT32 data = (DrvSprGfx[gfx_offs + 0] << 24) | (DrvSprGfx[gfx_offs + 1] << 16) |
					(DrvSprGfx[gfx_offs + 2] << 8) | (DrvSprGfx[gfx_offs + 3] << 0);

			for (INT32 sx = 0; sx < 0x10; sx++)
			{
				if ((x >= 0) && (x < nScreenWidth))
				{
					UINT32 pen = (data & 0xc0000000) >> 30;

					UINT8 col = DrvSprLut[pen_base | pen] & 0x0f;

					if (col)
						pTransDraw[(y * nScreenWidth) + x] = (pTransDraw[(y * nScreenWidth) + x] & 0xff0) | col;
				}

				if (flip_x)
					x = x - 1;
				else
					x = x + 1;

				data = data << 2;
			}
		}

		if (flip_y)
			y = y - 1;
		else
			y = y + 1;

		x = x_sav;
	}
}

static void draw_sprites()
{
	for (INT32 offs = 0x80 - 2; offs >= 0; offs -= 2)
	{
		static const int code_offs[2][2] =
		{
			{ 0, 1 },
			{ 2, 3 }
		};
		int x, y;

		UINT8 code = DrvSprRAM[offs + 0x000];
		UINT8 color = DrvSprRAM[offs + 0x001];

		int sx = DrvSprRAM[offs + 0x081] - 41 + 0x100*(DrvSprRAM[offs + 0x101] & 3);
		int sy = 256 - DrvSprRAM[offs + 0x080] + 1;

		int flip_x = (DrvSprRAM[offs + 0x100] & 0x01) >> 0;
		int flip_y = (DrvSprRAM[offs + 0x100] & 0x02) >> 1;
		int size_x = (DrvSprRAM[offs + 0x100] & 0x04) >> 2;
		int size_y = (DrvSprRAM[offs + 0x100] & 0x08) >> 3;

		sy = sy - (16 * size_y);
		sy = (sy & 0xff) - 32;

		if (game_selected && (global_flip & 0x01))
		{
			flip_x = !flip_x;
			flip_y = !flip_y;
		}

		for (y = 0; y <= size_y; y++)
			for (x = 0; x <= size_x; x++)
				draw_sprite(sy + (16 * y), sx + (16 * x),code + code_offs[y ^ (size_y * flip_y)][x ^ (size_x * flip_x)], color, flip_y, flip_x);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	BurnTransferClear();

	draw_stars();
	draw_chars();
	draw_sprites();

	BurnTransferCopy(DrvPalette + (game_selected * 0x1000));

	return 0;
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(0);
	}

	Z180NewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}

		if (sprite_pal_base) 	// 25pacmano
			DrvInputs[2] ^= 0x7e;
	}

	INT32 nInterleave = 224;
	INT32 nCyclesTotal[1] = { 18432000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	Z180Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		nCyclesDone[0] += Z180Run(nCyclesTotal[0] / nInterleave);

		if (i == (nInterleave - 1) && irq_mask) Z180SetIRQLine(0, CPU_IRQSTATUS_ACK);
	}

	if (pBurnSoundOut) {
		NamcoSoundUpdate(pBurnSoundOut, nBurnSoundLen);
		DACUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	Z180Close();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029737;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		Z180Scan(nAction);

		NamcoSoundScan(nAction, pnMin);

		EEPROMScan(nAction, pnMin);
		BurnWatchdogScan(nAction);

		SCAN_VAR(game_selected);
		SCAN_VAR(stars_seed);
		SCAN_VAR(stars_ctrl);
		SCAN_VAR(global_flip);
		SCAN_VAR(irq_mask);
		SCAN_VAR(_47100_val);
	}

	return 0;
}


// Pac-Man - 25th Anniversary Edition (Rev 3.00)

static struct BurnRomInfo Pacman25RomDesc[] = {
	{ "pacman25ver3.u1",		0x40000, 0x55b0076e, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "pacman_25th.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacman25)
STD_ROM_FN(Pacman25)

static INT32 Pacman25Init()
{
	sprite_pal_base = 0x80;

	return DrvInit();
}

struct BurnDriver BurnDrvPacman25 = {
	"25pacman", NULL, NULL, NULL, "2006",
	"Pac-Man - 25th Anniversary Edition (Rev 3.00)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, Pacman25RomInfo, Pacman25RomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	Pacman25Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};


// Pac-Man - 25th Anniversary Edition (Rev 2.00)

static struct BurnRomInfo Pacman25oRomDesc[] = {
	{ "pacman_25th_rev2.0.u13",	0x40000, 0x99a52784, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "pacman_25th.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacman25o)
STD_ROM_FN(Pacman25o)

struct BurnDriver BurnDrvPacman25o = {
	"25pacmano", "25pacman", NULL, NULL, "2005",
	"Pac-Man - 25th Anniversary Edition (Rev 2.00)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE, 0,
	NULL, Pacman25oRomInfo, Pacman25oRomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	Pacman25Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};


// Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.08)

static struct BurnRomInfo Pacgal20RomDesc[] = {
	{ "ms_pac-galaga_v1.08.u13",	0x40000, 0x2ea16809, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "ms_pac-galaga.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacgal20)
STD_ROM_FN(Pacgal20)

struct BurnDriver BurnDrvPacgal20 = {
	"20pacgal", NULL, NULL, NULL, "2000",
	"Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.08)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, Pacgal20RomInfo, Pacgal20RomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};


// Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.04)

static struct BurnRomInfo Pacgal20r4RomDesc[] = {
	{ "ms_pac-galaga_v1.04.u13",	0x40000, 0x6c474d2d, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "ms_pac-galaga.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacgal20r4)
STD_ROM_FN(Pacgal20r4)

struct BurnDriver BurnDrvPacgal20r4 = {
	"20pacgalr4", "20pacgal", NULL, NULL, "2000",
	"Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.04)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, Pacgal20r4RomInfo, Pacgal20r4RomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};


// Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.03)

static struct BurnRomInfo Pacgal20r3RomDesc[] = {
	{ "ms_pac-galaga_v1.03.u13",	0x40000, 0xe13dce63, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "ms_pac-galaga.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacgal20r3)
STD_ROM_FN(Pacgal20r3)

struct BurnDriver BurnDrvPacgal20r3 = {
	"20pacgalr3", "20pacgal", NULL, NULL, "2000",
	"Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.03)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, Pacgal20r3RomInfo, Pacgal20r3RomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};


// Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.02)

static struct BurnRomInfo Pacgal20r2RomDesc[] = {
	{ "ms_pac-galaga_v1.02.u13",	0x40000, 0xb939f805, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "ms_pac-galaga.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacgal20r2)
STD_ROM_FN(Pacgal20r2)

struct BurnDriver BurnDrvPacgal20r2 = {
	"20pacgalr2", "20pacgal", NULL, NULL, "2000",
	"Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.02)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, Pacgal20r2RomInfo, Pacgal20r2RomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};


// Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.01)

static struct BurnRomInfo Pacgal20r1RomDesc[] = {
	{ "ms_pac-galaga_v1.01.u13",	0x40000, 0x77159582, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "ms_pac-galaga.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacgal20r1)
STD_ROM_FN(Pacgal20r1)

struct BurnDriver BurnDrvPacgal20r1 = {
	"20pacgalr1", "20pacgal", NULL, NULL, "2000",
	"Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.01)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, Pacgal20r1RomInfo, Pacgal20r1RomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};


// Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.00)

static struct BurnRomInfo Pacgal20r0RomDesc[] = {
	{ "ms_pac-galaga_v1.0.u13",	0x40000, 0x3c92a269, 1 | BRF_PRG | BRF_ESS }, //  0 Z180 Code

	{ "ms_pac-galaga.u14",		0x08000, 0xc19d9ad0, 2 | BRF_GRA },           //  1 Color PROM
};

STD_ROM_PICK(Pacgal20r0)
STD_ROM_FN(Pacgal20r0)

struct BurnDriver BurnDrvPacgal20r0 = {
	"20pacgalr0", "20pacgal", NULL, NULL, "2000",
	"Ms. Pac-Man/Galaga - 20th Anniversary Class of 1981 Reunion (V1.00)\0", NULL, "Namco / Cosmodog", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_MISC_POST90S, GBF_MAZE | GBF_SHOOT, 0,
	NULL, Pacgal20r0RomInfo, Pacgal20r0RomName, NULL, NULL, NULL, NULL, Pacgal20InputInfo, NULL,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x1040,
	224, 288, 3, 4
};
