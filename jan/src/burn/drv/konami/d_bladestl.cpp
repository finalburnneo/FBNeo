// FB Alpha Blades of Steel driver module
// Based on MAME driver by Manuel Abadia

// Needs trackball hooked up

#include "tiles_generic.h"
#include "m6809_intf.h"
#include "hd6309_intf.h"
#include "burn_ym2203.h"
#include "upd7759.h"
#include "k007342_k007420.h"
#include "konamiic.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvHD6309ROM;
static UINT8 *DrvM6809ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvUpdROM;
static UINT8 *DrvLutPROM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *DrvHD6309RAM;
static UINT8 *DrvM6809RAM;
static UINT8 *DrvPalRAM;

static UINT8 HD6309Bank;
static UINT8 soundlatch;
static INT32 spritebank;
static UINT8 soundbank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[9];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;
static INT32 DrvAnalog0 = 0;
static INT32 DrvAnalog1 = 0;
static INT32 DrvAnalog2 = 0;
static INT32 DrvAnalog3 = 0;

static INT32 watchdog;

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo BladestlInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	A("P1 Trackball X",    	BIT_ANALOG_REL, &DrvAnalog0,    "mouse x-axis" ),
	A("P1 Trackball Y",    	BIT_ANALOG_REL, &DrvAnalog1,    "mouse y-axis" ),

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	A("P2 Trackball X",    	BIT_ANALOG_REL, &DrvAnalog2,    "mouse x-axis" ),
	A("P2 Trackball Y",    	BIT_ANALOG_REL, &DrvAnalog3,    "mouse y-axis" ),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",		BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Bladestl)

static struct BurnDIPInfo BladestlDIPList[]=
{
	{0x17, 0xff, 0xff, 0xe0, NULL			},
	{0x18, 0xff, 0xff, 0xc0, NULL			},
	{0x19, 0xff, 0xff, 0xff, NULL			},
	{0x1a, 0xff, 0xff, 0x5b, NULL			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x17, 0x01, 0x20, 0x20, "Off"			},
	{0x17, 0x01, 0x20, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x17, 0x01, 0x80, 0x80, "Off"			},
	{0x17, 0x01, 0x80, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Period time set"	},
	{0x18, 0x01, 0x80, 0x80, "4"			},
	{0x18, 0x01, 0x80, 0x00, "7"			},

	{0   , 0xfe, 0   ,   16, "Coin A"		},
	{0x19, 0x01, 0x0f, 0x02, "4 Coins 1 Credits"	},
	{0x19, 0x01, 0x0f, 0x05, "3 Coins 1 Credits"	},
	{0x19, 0x01, 0x0f, 0x08, "2 Coins 1 Credits"	},
	{0x19, 0x01, 0x0f, 0x04, "3 Coins 2 Credits"	},
	{0x19, 0x01, 0x0f, 0x01, "4 Coins 3 Credits"	},
	{0x19, 0x01, 0x0f, 0x0f, "1 Coin  1 Credits"	},
	{0x19, 0x01, 0x0f, 0x00, "4 Coins 5 Credits"	},
	{0x19, 0x01, 0x0f, 0x03, "3 Coins 4 Credits"	},
	{0x19, 0x01, 0x0f, 0x07, "2 Coins 3 Credits"	},
	{0x19, 0x01, 0x0f, 0x0e, "1 Coin  2 Credits"	},
	{0x19, 0x01, 0x0f, 0x06, "2 Coins 5 Credits"	},
	{0x19, 0x01, 0x0f, 0x0d, "1 Coin  3 Credits"	},
	{0x19, 0x01, 0x0f, 0x0c, "1 Coin  4 Credits"	},
	{0x19, 0x01, 0x0f, 0x0b, "1 Coin  5 Credits"	},
	{0x19, 0x01, 0x0f, 0x0a, "1 Coin  6 Credits"	},
	{0x19, 0x01, 0x0f, 0x09, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,   16, "Coin B"		},
	{0x19, 0x01, 0xf0, 0x20, "4 Coins 1 Credits"	},
	{0x19, 0x01, 0xf0, 0x50, "3 Coins 1 Credits"	},
	{0x19, 0x01, 0xf0, 0x80, "2 Coins 1 Credits"	},
	{0x19, 0x01, 0xf0, 0x40, "3 Coins 2 Credits"	},
	{0x19, 0x01, 0xf0, 0x10, "4 Coins 3 Credits"	},
	{0x19, 0x01, 0xf0, 0xf0, "1 Coin  1 Credits"	},
	{0x19, 0x01, 0xf0, 0x00, "4 Coins 5 Credits"	},
	{0x19, 0x01, 0xf0, 0x30, "3 Coins 4 Credits"	},
	{0x19, 0x01, 0xf0, 0x70, "2 Coins 3 Credits"	},
	{0x19, 0x01, 0xf0, 0xe0, "1 Coin  2 Credits"	},
	{0x19, 0x01, 0xf0, 0x60, "2 Coins 5 Credits"	},
	{0x19, 0x01, 0xf0, 0xd0, "1 Coin  3 Credits"	},
	{0x19, 0x01, 0xf0, 0xc0, "1 Coin  4 Credits"	},
	{0x19, 0x01, 0xf0, 0xb0, "1 Coin  5 Credits"	},
	{0x19, 0x01, 0xf0, 0xa0, "1 Coin  6 Credits"	},
	{0x19, 0x01, 0xf0, 0x90, "1 Coin  7 Credits"	},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x1a, 0x01, 0x04, 0x00, "Upright"		},
	{0x1a, 0x01, 0x04, 0x04, "Cocktail"		},

	{0   , 0xfe, 0   ,    4, "Bonus time set"	},
	{0x1a, 0x01, 0x18, 0x18, "30 secs"		},
	{0x1a, 0x01, 0x18, 0x10, "20 secs"		},
	{0x1a, 0x01, 0x18, 0x08, "15 secs"		},
	{0x1a, 0x01, 0x18, 0x00, "10 secs"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x1a, 0x01, 0x60, 0x60, "Easy"			},
	{0x1a, 0x01, 0x60, 0x40, "Normal"		},
	{0x1a, 0x01, 0x60, 0x20, "Difficult"		},
	{0x1a, 0x01, 0x60, 0x00, "Very Difficult"	},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x1a, 0x01, 0x80, 0x80, "Off"			},
	{0x1a, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Bladestl)

static void bankswitch(INT32 bank)
{
	HD6309Bank = bank;
	HD6309MapMemory(DrvHD6309ROM + (((bank & 0x60) >> 5) * 0x2000), 0x6000, 0x7fff, MAP_ROM);
}

static void bladestl_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfff8) == 0x2600) {
		K007342Regs[0][address & 7] = data;
		return;
	}

	if ((address & 0xffe0) == 0x2f80) {
		K051733Write(address & 0x1f, data);
		return;
	}

	switch (address)
	{
		case 0x2e80:
			soundlatch = data;
			M6809SetIRQLine(0, CPU_IRQSTATUS_ACK);
		return;

		case 0x2ec0:
			watchdog = 0;
		return;

		case 0x2f40:
			// coin counters = data & 0x03;
			// leds data & 0x0c;
			spritebank = (data & 0x80) << 3;
			bankswitch(data);
		return;

		case 0x2fc0:
		return; // nop
	}
}

static UINT8 trackball_read(INT32 /*offset*/)
{
/*
	int curr = m_trackball[offset]->read();
	int delta = (curr - m_last_track[offset]) & 0xff;
	m_last_track[offset] = curr;

	return (delta & 0x80) | (curr >> 1);
*/
	return 0;
}

static UINT8 bladestl_main_read(UINT16 address)
{
	if ((address & 0xffe0) == 0x2f80) {
		return K051733Read(address & 0x1f);
	}

	switch (address)
	{
		case 0x2e00:
			return (DrvInputs[0] & 0x1f) | (DrvDips[0] & 0xe0);

		case 0x2e01:
			return (DrvInputs[1] & 0x7f) | (DrvDips[1] & 0x80);

		case 0x2e02:
			return DrvInputs[2];

		case 0x2e03:
			return DrvDips[3];

		case 0x2e40:
			return DrvDips[2];

		case 0x2f00:
		case 0x2f01:
		case 0x2f02:
		case 0x2f03:
			return trackball_read(address & 3);
	}

	return 0;
}

static void bladestl_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x1000:
		case 0x1001:
			BurnYM2203Write(0, address & 1, data);
		return;

		case 0x3000:
			UPD7759ResetWrite(0, data & 1);
			UPD7759StartWrite(0, data & 2);
		return;

		case 0x5000:
		return;
	}
}

static UINT8 bladestl_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x1000:
		case 0x1001:
			return BurnYM2203Read(0, address & 1);

		case 0x4000:
			return (UPD7759BusyRead(0) ? 1 : 0);

		case 0x6000:
			M6809SetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;
	}

	return 0;
}

static void bladestl_tile_callback(INT32 layer, INT32 /*bank*/, INT32 *code, INT32 *color, INT32 */*flags*/)
{
	*code |= ((*color & 0x0f) << 8) | ((*color & 0x40) << 6);
	*color = 0x10 + layer;
}

static void bladestl_sprite_callback(INT32 *code, INT32 *color)
{
	*code |= ((*color & 0xc0) << 2) + spritebank;
	*code = (*code << 2) | ((*color & 0x30) >> 4);
	*color = (*color & 0x0f);
}

static void bladestl_ym2203_write_portA(UINT32, UINT32 data)
{
	UPD7759PortWrite(0, data);
}

static void bladestl_ym2203_write_portB(UINT32, UINT32 data)
{
	soundbank = data;
	memcpy (DrvUpdROM, DrvUpdROM + 0x20000 + (((data & 0x38) >> 3) * 0x20000), 0x20000);
}

inline static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)M6809TotalCycles() * nSoundRate / 2000000;
}

inline static double DrvGetTime()
{
	return (double)M6809TotalCycles() / 2000000.0;
}

static INT32 DrvDoReset(INT32 clear_ram)
{
	if (clear_ram) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	HD6309Open(0);
	HD6309Reset();
	HD6309Close();

	M6809Open(0);
	M6809Reset();
	UPD7759Reset();
	BurnYM2203Reset();
	M6809Close();

	K007342Reset();

	bladestl_ym2203_write_portB(0,0);

	HiscoreReset();

	HD6309Bank = 0;
	soundlatch = 0;
	spritebank = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvHD6309ROM		= Next; Next += 0x010000;
	DrvM6809ROM		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x080000;
	DrvGfxROM1		= Next; Next += 0x080000;

	DrvUpdROM		= Next; Next += 0x120000;

	DrvLutPROM		= Next; Next += 0x000100;
;
	DrvPalette		= (UINT32*)Next; Next += 0x120 * sizeof(UINT32);

	AllRam			= Next;

	DrvHD6309RAM		= Next; Next += 0x002000;
	DrvM6809RAM		= Next; Next += 0x000800;
	DrvPalRAM		= Next; Next += 0x000100;

	K007342VidRAM[0]	= Next; Next += 0x002000;
	K007342ScrRAM[0]	= Next; Next += 0x000200;

	K007420RAM[0]		= Next; Next += 0x000200;

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

static void DrvGfxExpand(UINT8 *src, INT32 len)
{
	for (INT32 i = (len - 1) * 2; i >= 0; i-=2) {
		src[i+0] = src[i/2] >> 4;
		src[i+1] = src[i/2] & 0xf;
	}
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
		if (BurnLoadRom(DrvHD6309ROM + 0x00000,  0, 1)) return 1;

		if (BurnLoadRom(DrvM6809ROM  + 0x08000,  1, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x00000,  2, 1)) return 1;
		BurnByteswap(DrvGfxROM0, 0x40000);

		if (BurnLoadRom(DrvGfxROM1   + 0x00000,  3, 1)) return 1;
	
		if (BurnLoadRom(DrvLutPROM   + 0x00000,  4, 1)) return 1;
	
		if (BurnLoadRom(DrvUpdROM    + 0x20000,  5, 1)) return 1;
		if (BurnLoadRom(DrvUpdROM    + 0xa0000,  6, 1)) return 1;

		DrvGfxExpand(DrvGfxROM0, 0x40000);
		DrvGfxExpand(DrvGfxROM1, 0x40000);
	}

	HD6309Init(0);
	HD6309Open(0);
	HD6309MapMemory(K007342VidRAM[0],	0x0000, 0x1fff, MAP_RAM);
	HD6309MapMemory(K007420RAM[0], 		0x2000, 0x21ff, MAP_RAM);
	HD6309MapMemory(K007342ScrRAM[0],	0x2200, 0x23ff, MAP_RAM);
	HD6309MapMemory(DrvPalRAM,		0x2400, 0x24ff, MAP_RAM);
	HD6309MapMemory(DrvHD6309RAM,		0x4000, 0x5fff, MAP_RAM);
	HD6309MapMemory(DrvHD6309ROM + 0x08000, 0x8000, 0xffff, MAP_ROM);
	HD6309SetWriteHandler(bladestl_main_write);
	HD6309SetReadHandler(bladestl_main_read);
	HD6309Close();

	M6809Init(1);
	M6809Open(0);
	M6809MapMemory(DrvM6809RAM,		0x0000, 0x07ff, MAP_RAM);
	M6809MapMemory(DrvM6809ROM  + 0x08000,	0x8000, 0xffff, MAP_ROM);
	M6809SetWriteHandler(bladestl_sound_write);
	M6809SetReadHandler(bladestl_sound_read);
	M6809Close();

	K007342Init(DrvGfxROM0, bladestl_tile_callback);
	K007342SetOffsets(0, 16);

	K007420Init(0x3ff, bladestl_sprite_callback);
	K007420SetOffsets(0, 16);

	UPD7759Init(0, UPD7759_STANDARD_CLOCK, DrvUpdROM);
	UPD7759SetRoute(0, 0.60, BURN_SND_ROUTE_BOTH);

	BurnYM2203Init(1, 3579545, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnYM2203SetPorts(0, NULL, NULL, &bladestl_ym2203_write_portA, &bladestl_ym2203_write_portB);
	BurnTimerAttachM6809(2000000);
	BurnYM2203SetAllRoutes(0, 0.45, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetPSGVolume(0, 0.35); // when filters get hooked up, remove this line.

	// needs filters hooked up

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6809Exit();
	HD6309Exit();

	BurnYM2203Exit();
	UPD7759Exit();

	BurnFree (AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	UINT32 pens[0x60];
	UINT16 *pal = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x60 / 2; i++) {
		UINT16 p = (pal[i] << 8) | (pal[i] >> 8);
		INT32 r = (p & 0x1f);
		INT32 g = (p >> 5) & 0x1f;
		INT32 b = (p >> 10) & 0x1f;

		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);

		pens[i] = BurnHighCol(r,g,b,0);
	}

	for (INT32 i = 0; i < 0x100; i++) {
		DrvPalette[i] = pens[(DrvLutPROM[i] & 0xf)|0x20];
	}

	for (INT32 i = 0; i < 0x020; i++) {
		DrvPalette[i + 0x100] = pens[i];
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 1;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) K007342DrawLayer(1, 0 | K007342_OPAQUE, 0);
	if (nSpriteEnable & 1) K007420DrawSprites(DrvGfxROM1);
	if (nBurnLayer & 2) K007342DrawLayer(1, 1 | K007342_OPAQUE, 0);
	if (nBurnLayer & 4) K007342DrawLayer(0, 0, 0);
	if (nBurnLayer & 8) K007342DrawLayer(0, 1, 0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	watchdog++;
	if (DrvReset) {
		DrvDoReset(0);
	}

	if (DrvReset) {
		DrvDoReset(1);
	}

	M6809NewFrame();

	{
		memset (DrvInputs, 0xff, 3);
		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] =  { 6000000 / 60, 2000000 / 60 };

	HD6309Open(0);
	M6809Open(0);

	HD6309SetIRQLine(0x20, CPU_IRQSTATUS_AUTO); // nmi

	for (INT32 i = 0; i < nInterleave; i++) {

		HD6309Run(nCyclesTotal[0] / nInterleave);

		if (i == 240 && K007342_irq_enabled()) HD6309SetIRQLine(1, CPU_IRQSTATUS_AUTO); // firq

		BurnTimerUpdate((i + 1) * (nCyclesTotal[1] / nInterleave));
	}

	BurnTimerEndFrame(nCyclesTotal[1]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		UPD7759Update(0, pBurnSoundOut, nBurnSoundLen);
	}

	HD6309Close();
	M6809Close();
	
	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029704;
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		HD6309Scan(nAction);
		M6809Scan(nAction);

		K051733Scan(nAction);
		BurnYM2203Scan(nAction, pnMin);
		UPD7759Scan(0, nAction, pnMin);

		K007342Scan(nAction);

		SCAN_VAR(HD6309Bank);
		SCAN_VAR(soundlatch);
		SCAN_VAR(spritebank);
		SCAN_VAR(soundbank);
	}

	if (nAction & ACB_WRITE) {
		HD6309Open(0);
		bankswitch(HD6309Bank);
		HD6309Close();

		bladestl_ym2203_write_portB(0,soundbank);
	}

	return 0;
}


// Blades of Steel (version T)

static struct BurnRomInfo bladestlRomDesc[] = {
	{ "797-t01.19c",	0x10000, 0x89d7185d, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "797-c02.12d",	0x08000, 0x65a331ea, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "797a05.19h",		0x40000, 0x5491ba28, 3 | BRF_GRA },           //  2 Tiles

	{ "797a06.13h",		0x40000, 0xd055f5cc, 4 | BRF_GRA },           //  3 Sprites

	{ "797a07.16i",		0x00100, 0x7aecad4e, 5 | BRF_GRA },           //  4 Sprite Color Lookup Tables

	{ "797a03.11a",		0x80000, 0x9ee1a542, 6 | BRF_GRA },           //  5 UPD7759 Samples
	{ "797a04.9a",		0x40000, 0x9ac8ea4e, 6 | BRF_GRA },           //  6
};

STD_ROM_PICK(bladestl)
STD_ROM_FN(bladestl)

struct BurnDriver BurnDrvBladestl = {
	"bladestl", NULL, NULL, NULL, "1987",
	"Blades of Steel (version T)\0", NULL, "Konami", "GX797",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, bladestlRomInfo, bladestlRomName, NULL, NULL, BladestlInputInfo, BladestlDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x120,
	224, 256, 3, 4
};


// Blades of Steel (version L)

static struct BurnRomInfo bladestllRomDesc[] = {
	{ "797-l01.19c",	0x10000, 0x1ab14c40, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "797-c02.12d",	0x08000, 0x65a331ea, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "797a05.19h",		0x40000, 0x5491ba28, 3 | BRF_GRA },           //  2 Tiles

	{ "797a06.13h",		0x40000, 0xd055f5cc, 4 | BRF_GRA },           //  3 Sprites

	{ "797a07.16i",		0x00100, 0x7aecad4e, 5 | BRF_GRA },           //  4 Sprite Color Lookup Tables

	{ "797a03.11a",		0x80000, 0x9ee1a542, 6 | BRF_GRA },           //  5 UPD7759 Samples
	{ "797a04.9a",		0x40000, 0x9ac8ea4e, 6 | BRF_GRA },           //  6
};

STD_ROM_PICK(bladestll)
STD_ROM_FN(bladestll)

struct BurnDriver BurnDrvBladestll = {
	"bladestll", "bladestl", NULL, NULL, "1987",
	"Blades of Steel (version L)\0", NULL, "Konami", "GX797",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, bladestllRomInfo, bladestllRomName, NULL, NULL, BladestlInputInfo, BladestlDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x120,
	224, 256, 3, 4
};


// Blades of Steel (version E)

static struct BurnRomInfo bladestleRomDesc[] = {
	{ "797-e01.19c",	0x10000, 0xf8472e95, 1 | BRF_PRG | BRF_ESS }, //  0 HD6309 Code

	{ "797-c02.12d",	0x08000, 0x65a331ea, 2 | BRF_PRG | BRF_ESS }, //  1 M6809 Code

	{ "797a05.19h",		0x40000, 0x5491ba28, 3 | BRF_GRA },           //  2 Tiles

	{ "797a06.13h",		0x40000, 0xd055f5cc, 4 | BRF_GRA },           //  3 Sprites

	{ "797a07.16i",		0x00100, 0x7aecad4e, 5 | BRF_GRA },           //  4 Sprite Color Lookup Tables

	{ "797a03.11a",		0x80000, 0x9ee1a542, 6 | BRF_GRA },           //  5 UPD7759 Samples
	{ "797a04.9a",		0x40000, 0x9ac8ea4e, 6 | BRF_GRA },           //  6
};

STD_ROM_PICK(bladestle)
STD_ROM_FN(bladestle)

struct BurnDriver BurnDrvBladestle = {
	"bladestle", "bladestl", NULL, NULL, "1987",
	"Blades of Steel (version E)\0", NULL, "Konami", "GX797",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED | BDF_HISCORE_SUPPORTED, 2, HARDWARE_PREFIX_KONAMI, GBF_SPORTSMISC, 0,
	NULL, bladestleRomInfo, bladestleRomName, NULL, NULL, BladestlInputInfo, BladestlDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x120,
	224, 256, 3, 4
};
