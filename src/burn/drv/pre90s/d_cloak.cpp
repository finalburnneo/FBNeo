// FB Alpha Cloak and Dagger driver module
// Based on MAME driver by Dan Boris and Mirko Buffoni

#include "tiles_generic.h"
#include "m6502_intf.h"
#include "pokey.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvM6502ROM0;
static UINT8 *DrvM6502ROM1;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvM6502RAM0;
static UINT8 *DrvM6502RAM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvNVRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM;
static UINT16 *DrvPalRAM;
static UINT8 *bitmap[2];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 nvram_enable;
static UINT8 video_selected;
static UINT8 video_address_x;
static UINT8 video_address_y;

static INT32 watchdog;
static UINT8 vblank;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvJoy4[8];
static UINT8 DrvDips[1];
static UINT8 DrvInputs[4];
static UINT8 DrvReset;

static struct BurnInputInfo CloakInputList[] = {
	{"Coin A",		BIT_DIGITAL,	DrvJoy3 + 3,	"p1 coin"	},
	{"Coin B",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 coin"	},
	{"Start 1",		BIT_DIGITAL,	DrvJoy4 + 7,	"p1 start"	},
	{"Start 2",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 start"	},
	{"P1 Left Up",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 up"		},
	{"P1 Left Down",	BIT_DIGITAL,	DrvJoy1 + 4,	"p1 down"	},
	{"P1 Left Left",	BIT_DIGITAL,	DrvJoy1 + 7,	"p1 left"	},
	{"P1 Left Right",	BIT_DIGITAL,	DrvJoy1 + 6,	"p1 right"	},

	{"P1 Right Up",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 up"		},
	{"P1 Right Down",	BIT_DIGITAL,	DrvJoy1 + 0,	"p2 down"	},
	{"P1 Right Left",	BIT_DIGITAL,	DrvJoy1 + 3,	"p2 left"	},
	{"P1 Right Right",	BIT_DIGITAL,	DrvJoy1 + 2,	"p2 right"	},

	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 fire 1"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy3 + 5,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
};

STDINPUTINFO(Cloak)

static struct BurnDIPInfo CloakDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x02, NULL			},

	{0   , 0xfe, 0   ,    4, "Credits"		},
	{0x0f, 0x01, 0x03, 0x02, "1 Credit/1 Game"	},
	{0x0f, 0x01, 0x03, 0x01, "1 Credit/2 Games"	},
	{0x0f, 0x01, 0x03, 0x03, "2 Credits/1 Game"	},
	{0x0f, 0x01, 0x03, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x0f, 0x01, 0x0c, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x0c, 0x04, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0x0c, 0x08, "1 Coin  5 Credits"	},
	{0x0f, 0x01, 0x0c, 0x0c, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    2, "Coin A"		},
	{0x0f, 0x01, 0x10, 0x00, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x10, 0x10, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    0, "Demo Freeze Mode"	},
	{0x0f, 0x01, 0x40, 0x00, "Off"			},
	{0x0f, 0x01, 0x40, 0x40, "On"			},
};

STDDIPINFO(Cloak)

static UINT8 adjust_xy_r(INT32 offset)
{
	UINT8 pxl = bitmap[video_selected][(video_address_y * 256) | video_address_x];

	switch (offset & 7)
	{
		case 0:
			video_address_x--;
			video_address_y++;
		break;

		case 1:
			video_address_y--;
		break;

		case 2:
			video_address_x--;
		break;

		case 4:
			video_address_x++;
			video_address_y++;
		break;

		case 5:
			video_address_y++;
		break;

		case 6:
			video_address_x++;
		break;
	}

	return pxl;
}

static void adjust_xy_w(INT32 offset, INT32 data)
{
	switch (offset & 7)
	{
		case 3:
			video_address_x = data;
		break;

		case 7:
			video_address_y = data;
		break;

	    default:
			bitmap[video_selected^1][(video_address_y * 256) | video_address_x] = data & 0xf;
			adjust_xy_r(offset);
		break;
	}
}

static void cloak_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xff80) == 0x3200) {
		UINT16 offset = address - 0x3200;
		DrvPalRAM[offset & 0x3f] = ((offset & 0x40) << 2) | data;
		return;
	}

	if ((address & 0xff00) == 0x2f00) {
		return; // nop
	}

	if ((address & 0xfff0) == 0x1000) {
		pokey1_w(address & 0xf, data);
		return;
	}

	if ((address & 0xfff0) == 0x1800) {
		pokey2_w(address & 0xf, data);
		return;
	}

#if 0
	if ((address & 0xfe00) == 0x2800) {
		if (nvram_enable) {
			DrvNVRAM[address & 0x1ff] = data;
		}
		return;
	}
#endif

	switch (address)
	{
		case 0x2600:
			// custom (unused)
		return;

		case 0x3800:
		case 0x3801:
			// coin counter
		return;

		case 0x3803:
			flipscreen = data & 0x80;
		return;

		case 0x3805:
		return;	// nop

		case 0x3806:
		case 0x3807:
			// leds
		return;

		case 0x3a00:
			watchdog = 0;
		return;

		case 0x3c00:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x3e00:
			nvram_enable = data & 1;
		return;
	}
}

static UINT8 cloak_main_read(UINT16 address)
{
	if ((address & 0xff00) == 0x2f00) {
		return 0; // nop
	}

	if ((address & 0xfff0) == 0x1000) {		// pokey 1
		switch (address&0xf) {
			case ALLPOT_C: return DrvInputs[3] ^ 0xf0;
			default: return pokey1_r(address&0xf);
		}
	}

	if ((address & 0xfff0) == 0x1800) {		// pokey 2
		switch (address&0xf) {
			case ALLPOT_C: return DrvDips[0];
			default: return pokey2_r(address&0xf);
		}
	}

	switch (address)
	{
		case 0x2000:
			return DrvInputs[0]; // p1

		case 0x2200:
			return 0xff;

		case 0x2400:
			return (DrvInputs[2] & 0xfe) | ((vblank) ? 0x00 : 0x01);
	}

	return 0;
}

static void cloak_sub_write(UINT16 address, UINT8 data)
{
	if (address < 0x0008 || (address >= 0x0010 && address <= 0x07ff)) {
		DrvM6502RAM1[address & 0x7ff] = data;
		return;
	}

	if ((address & 0xfff8) == 0x0008) {
		adjust_xy_w(address, data);
		return;
	}

	switch (address)
	{
		case 0x1000:
			M6502SetIRQLine(0, CPU_IRQSTATUS_NONE);
		return;

		case 0x1200:
		{
			video_selected = data & 1;

			if (data & 2) {
				memset (bitmap[video_selected^1], 0, 256 * 256);
			}
		}
		return;

		case 0x1400:
			// custom (unused)
		return;
	}
}

static UINT8 cloak_sub_read(UINT16 address)
{
	if (address < 0x0008 || (address >= 0x0010 && address <= 0x07ff)) {
		return DrvM6502RAM1[address & 0x7ff];
	}	

	if ((address & 0xfff8) == 0x0008) {
		return adjust_xy_r(address);
	}

	return 0;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset(AllRam, 0, RamEnd - AllRam);
	}

	M6502Open(0);
	M6502Reset();
	M6502Close();

	M6502Open(1);
	M6502Reset();
	M6502Close();

	watchdog = 0;

	flipscreen = 0;
	nvram_enable = 0;
	video_selected = 0;
	video_address_x = 0;
	video_address_y = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvM6502ROM0		= Next; Next += 0x010000;
	DrvM6502ROM1		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x004000;

	DrvNVRAM		= Next; Next += 0x000200;

	DrvPalette		= (UINT32*)Next; Next += 0x0040 * sizeof(UINT32);

	AllRam			= Next;

	DrvM6502RAM0		= Next; Next += 0x000800;
	DrvM6502RAM1		= Next; Next += 0x000800;
	DrvVidRAM		= Next; Next += 0x000800;
	DrvShareRAM		= Next; Next += 0x000800;
	DrvSprRAM		= Next; Next += 0x000100;

	DrvPalRAM		= (UINT16*)Next; Next += 0x0040 * 2;

	bitmap[0]		= Next; Next += 256 * 256;
	bitmap[1]		= Next; Next += 256 * 256;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[4] = { STEP4(0,1) };
	INT32 XOffs[8] = { 0x1000*8+0, 0x1000*8+4, 0, 4, 0x1000*8+8, 0x1000*8+12, 8, 12 };
	INT32 YOffs[16]= { STEP16(0,16) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x2000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x2000);

	GfxDecode(0x0100, 4, 8,  8, Plane, XOffs, YOffs, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x2000);

	GfxDecode(0x0080, 4, 8, 16, Plane, XOffs, YOffs, 0x100, tmp, DrvGfxROM1);

	BurnFree(tmp);

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
		if (BurnLoadRom(DrvM6502ROM0 + 0x4000,  0, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x6000,  1, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0x8000,  2, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM0 + 0xc000,  3, 1)) return 1;

		if (BurnLoadRom(DrvM6502ROM1 + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x4000,  5, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x6000,  6, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0x8000,  7, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0xa000,  8, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0xc000,  9, 1)) return 1;
		if (BurnLoadRom(DrvM6502ROM1 + 0xe000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0   + 0x0000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0   + 0x1000, 12, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1   + 0x0000, 13, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1   + 0x1000, 14, 1)) return 1;

		DrvGfxDecode();
	}

	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502RAM0,		0x0000, 0x03ff, MAP_RAM);
	M6502MapMemory(DrvVidRAM,		0x0400, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,		0x0800, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvNVRAM,		0x2800, 0x29ff, MAP_RAM);
	M6502MapMemory(DrvSprRAM,		0x3000, 0x30ff, MAP_RAM);
//	M6502MapMemory(DrvPalRAM,		0x3200, 0x32ff, MAP_RAM); // handler
	M6502MapMemory(DrvM6502ROM0 + 0x4000,	0x4000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(cloak_main_write);
	M6502SetReadHandler(cloak_main_read);
	M6502Close();

	M6502Init(1, TYPE_M6502);
	M6502Open(1);
	M6502MapMemory(DrvM6502RAM1,		0x0000, 0x00ff, MAP_FETCH);
	M6502MapMemory(DrvM6502RAM1 + 0x0100,	0x0100, 0x07ff, MAP_RAM);
	M6502MapMemory(DrvShareRAM,		0x0800, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvM6502ROM1 + 0x2000,	0x2000, 0xffff, MAP_ROM);
	M6502SetWriteHandler(cloak_sub_write);
	M6502SetReadHandler(cloak_sub_read);
	M6502Close();

	PokeyInit(1250000, 2, 1.00, 0);

	GenericTilesInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	M6502Exit();
	PokeyExit();

	BurnFree(AllMem);

	return 0;
}

#define MAX_NETS        3
#define MAX_RES_PER_NET 18
#define Combine2Weights(tab,w0,w1)	((int)(((tab)[0]*(w0) + (tab)[1]*(w1)) + 0.5))
#define Combine3Weights(tab,w0,w1,w2)	((int)(((tab)[0]*(w0) + (tab)[1]*(w1) + (tab)[2]*(w2)) + 0.5))

static double ComputeResistorWeights(INT32 MinVal, INT32 MaxVal, double Scaler, INT32 Count1, const INT32 *Resistances1, double *Weights1, INT32 PullDown1, INT32 PullUp1,	INT32 Count2, const INT32 *Resistances2, double *Weights2, INT32 PullDown2, INT32 PullUp2, INT32 Count3, const INT32 *Resistances3, double *Weights3, INT32 PullDown3, INT32 PullUp3)
{
	INT32 NetworksNum;

	INT32 ResCount[MAX_NETS];
	double r[MAX_NETS][MAX_RES_PER_NET];
	double w[MAX_NETS][MAX_RES_PER_NET];
	double ws[MAX_NETS][MAX_RES_PER_NET];
	INT32 r_pd[MAX_NETS];
	INT32 r_pu[MAX_NETS];

	double MaxOut[MAX_NETS];
	double *Out[MAX_NETS];

	INT32 i, j, n;
	double Scale;
	double Max;

	NetworksNum = 0;
	for (n = 0; n < MAX_NETS; n++) {
		INT32 Count, pd, pu;
		const INT32 *Resistances;
		double *Weights;

		switch (n) {
			case 0: {
				Count = Count1;
				Resistances = Resistances1;
				Weights = Weights1;
				pd = PullDown1;
				pu = PullUp1;
				break;
			}
			
			case 1: {
				Count = Count2;
				Resistances = Resistances2;
				Weights = Weights2;
				pd = PullDown2;
				pu = PullUp2;
				break;
			}
		
			case 2:
			default: {
				Count = Count3;
				Resistances = Resistances3;
				Weights = Weights3;
				pd = PullDown3;
				pu = PullUp3;
				break;
			}
		}

		if (Count > 0) {
			ResCount[NetworksNum] = Count;
			for (i = 0; i < Count; i++) {
				r[NetworksNum][i] = 1.0 * Resistances[i];
			}
			Out[NetworksNum] = Weights;
			r_pd[NetworksNum] = pd;
			r_pu[NetworksNum] = pu;
			NetworksNum++;
		}
	}

	for (i = 0; i < NetworksNum; i++) {
		double R0, R1, Vout, Dst;

		for (n = 0; n < ResCount[i]; n++) {
			R0 = (r_pd[i] == 0) ? 1.0 / 1e12 : 1.0 / r_pd[i];
			R1 = (r_pu[i] == 0) ? 1.0 / 1e12 : 1.0 / r_pu[i];

			for (j = 0; j < ResCount[i]; j++) {
				if (j == n) {
					if (r[i][j] != 0.0) R1 += 1.0 / r[i][j];
				} else {
					if (r[i][j] != 0.0) R0 += 1.0 / r[i][j];
				}
			}

			R0 = 1.0/R0;
			R1 = 1.0/R1;
			Vout = (MaxVal - MinVal) * R0 / (R1 + R0) + MinVal;

			Dst = (Vout < MinVal) ? MinVal : (Vout > MaxVal) ? MaxVal : Vout;

			w[i][n] = Dst;
		}
	}

	j = 0;
	Max = 0.0;
	for (i = 0; i < NetworksNum; i++) {
		double Sum = 0.0;

		for (n = 0; n < ResCount[i]; n++) Sum += w[i][n];

		MaxOut[i] = Sum;
		if (Max < Sum) {
			Max = Sum;
			j = i;
		}
	}

	if (Scaler < 0.0) {
		Scale = ((double)MaxVal) / MaxOut[j];
	} else {
		Scale = Scaler;
	}

	for (i = 0; i < NetworksNum; i++) {
		for (n = 0; n < ResCount[i]; n++) {
			ws[i][n] = w[i][n] * Scale;
			(Out[i])[n] = ws[i][n];
		}
	}

	return Scale;
}

static void palette_update()
{
	static const int resistances_rgb[3] = { 10000, 4700, 2200 };
	double weights_rgb[3];

	ComputeResistorWeights(0, 0xff, -1.0,
						   3, &resistances_rgb[0], weights_rgb, 0, 1000,
						   0,0,0,0,0,
						   0,0,0,0,0);

	for (INT32 i = 0; i < 0x40; i++)
	{
		INT32 bit0 = (~DrvPalRAM[i] >> 6) & 0x01;
		INT32 bit1 = (~DrvPalRAM[i] >> 7) & 0x01;
		INT32 bit2 = (~DrvPalRAM[i] >> 8) & 0x01;
		INT32 r = Combine3Weights(weights_rgb, bit0, bit1, bit2);

		bit0 = (~DrvPalRAM[i] >> 3) & 0x01;
		bit1 = (~DrvPalRAM[i] >> 4) & 0x01;
		bit2 = (~DrvPalRAM[i] >> 5) & 0x01;
		INT32 g = Combine3Weights(weights_rgb, bit0, bit1, bit2);

		bit0 = (~DrvPalRAM[i] >> 0) & 0x01;
		bit1 = (~DrvPalRAM[i] >> 1) & 0x01;
		bit2 = (~DrvPalRAM[i] >> 2) & 0x01;
		INT32 b = Combine3Weights(weights_rgb, bit0, bit1, bit2);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}
#undef MAX_NETS
#undef MAX_RES_PER_NET
#undef Combine2Weights
#undef Combine3Weights

static void draw_layer()
{
	for (INT32 offs = /*3 * 32*/0; offs < 32 * 32; offs++)
	{
		INT32 sx = (offs & 0x1f) * 8;
		INT32 sy = (offs / 0x20) * 8;

		INT32 code = DrvVidRAM[offs];

		Render8x8Tile_Clip(pTransDraw, code, sx, sy - 24, 0, 4, 0, DrvGfxROM0);
	}
}

static void draw_bitmap()
{
	for (INT32 y = 24; y < 256; y++)
	{
		for (INT32 x = 0; x < 256; x++)
		{
			UINT8 pen = bitmap[video_selected][(y * 256) | x] & 0x07;

			if (pen) {
				pTransDraw[((y - 24) * nScreenWidth) + ((x - 6) & 0xff)] = 0x10 | ((x & 0x80) >> 4) | pen;
					//0x10 | ((x >> 4) & 8) | pen;
			}
		}
	}
}

static void draw_sprites()
{
	for (INT32 offs = (0x100 / 4) - 1; offs >= 0; offs--)
	{
		INT32 code  = DrvSprRAM[offs + 64] & 0x7f;
		INT32 color = 0;
		INT32 flipx = DrvSprRAM[offs + 64] & 0x80;
		INT32 flipy = 0;
		INT32 sx    = DrvSprRAM[offs + 192];
		INT32 sy    = 240 - DrvSprRAM[offs];

		if (flipscreen)
		{
			sx -= 9;
			sy = 240 - sy;
			flipx = !flipx;
			flipy = !flipy;
		} else {
			sy -= 24;
		}

		if (flipy) {
			if (flipx) {
				RenderCustomTile_Mask_FlipXY_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0x20, DrvGfxROM1);
			} else {
				RenderCustomTile_Mask_FlipY_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0x20, DrvGfxROM1);
			}
		} else {
			if (flipx) {
				RenderCustomTile_Mask_FlipX_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0x20, DrvGfxROM1);
			} else {
				RenderCustomTile_Mask_Clip(pTransDraw, 8, 16, code, sx, sy, color, 4, 0, 0x20, DrvGfxROM1);
			}
		}
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		palette_update();
		DrvRecalc = 0;
	//}

	BurnTransferClear();

	if (nBurnLayer & 1) draw_layer();
	if (nBurnLayer & 2) draw_bitmap();
	if (nBurnLayer & 4) draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	watchdog++;
	if (watchdog >= 180) {
		DrvDoReset(0);
	}

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 262;
	INT32 nCyclesTotal[2] = { 1000000 / 60, 1250000 / 60 };
	INT32 nCyclesDone[2] = { 0, 0 };

	vblank = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		M6502Open(0);
		nCyclesDone[0] += M6502Run(nCyclesTotal[0] / nInterleave);
		if ((i & 0x3f) == 0x3f) M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		M6502Close();

		M6502Open(1);
		nCyclesDone[1] += M6502Run(nCyclesTotal[1] / nInterleave);
		if ((i & 0x7f) == 0x7f) M6502SetIRQLine(0, CPU_IRQSTATUS_HOLD);
		M6502Close();

		if (i == 240) vblank = 1;
	}

	if (pBurnSoundOut) {
		pokey_update(0, pBurnSoundOut, nBurnSoundLen);
		pokey_update(1, pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029727;
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data		= DrvNVRAM;
		ba.nLen		= 0x000200;
		ba.szName	= "Nonvolatile RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_VOLATILE) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		M6502Scan(nAction);

		pokey_scan(nAction, pnMin);

		SCAN_VAR(nvram_enable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(video_selected);
		SCAN_VAR(video_address_x);
		SCAN_VAR(video_address_y);
	}

	return 0;
}


// Cloak & Dagger (rev 5)

static struct BurnRomInfo cloakRomDesc[] = {
	{ "136023-501.bin",	0x2000, 0xc2dbef1b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136023-502.bin",	0x2000, 0x316d0c7b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136023-503.bin",	0x4000, 0xb9c291a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136023-504.bin",	0x4000, 0xd014a1c0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136023-509.bin",	0x2000, 0x46c021a4, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136023-510.bin",	0x2000, 0x8c9cf017, 2 | BRF_GRA },           //  5
	{ "136023-511.bin",	0x2000, 0x66fd8a34, 2 | BRF_GRA },           //  6
	{ "136023-512.bin",	0x2000, 0x48c8079e, 2 | BRF_GRA },           //  7
	{ "136023-513.bin",	0x2000, 0x13f1cbab, 2 | BRF_GRA },           //  8
	{ "136023-514.bin",	0x2000, 0x6f8c7991, 2 | BRF_GRA },           //  9
	{ "136023-515.bin",	0x2000, 0x835438a0, 2 | BRF_GRA },           // 10

	{ "136023-105.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136023-106.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136023-107.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136023-108.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(cloak)
STD_ROM_FN(cloak)

struct BurnDriver BurnDrvCloak = {
	"cloak", NULL, NULL, NULL, "1983",
	"Cloak & Dagger (rev 5)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cloakRomInfo, cloakRomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Cloak & Dagger (Spanish)

static struct BurnRomInfo cloakspRomDesc[] = {
	{ "136023-501.bin",	0x2000, 0xc2dbef1b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136023-502.bin",	0x2000, 0x316d0c7b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136023-503.bin",	0x4000, 0xb9c291a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136023-804.bin",	0x4000, 0x994899c7, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136023-509.bin",	0x2000, 0x46c021a4, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136023-510.bin",	0x2000, 0x8c9cf017, 2 | BRF_GRA },           //  5
	{ "136023-511.bin",	0x2000, 0x66fd8a34, 2 | BRF_GRA },           //  6
	{ "136023-512.bin",	0x2000, 0x48c8079e, 2 | BRF_GRA },           //  7
	{ "136023-513.bin",	0x2000, 0x13f1cbab, 2 | BRF_GRA },           //  8
	{ "136023-514.bin",	0x2000, 0x6f8c7991, 2 | BRF_GRA },           //  9
	{ "136023-515.bin",	0x2000, 0x835438a0, 2 | BRF_GRA },           // 10

	{ "136023-105.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136023-106.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136023-107.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136023-108.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(cloaksp)
STD_ROM_FN(cloaksp)

struct BurnDriver BurnDrvCloaksp = {
	"cloaksp", "cloak", NULL, NULL, "1983",
	"Cloak & Dagger (Spanish)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cloakspRomInfo, cloakspRomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Cloak & Dagger (French)

static struct BurnRomInfo cloakfrRomDesc[] = {
	{ "136023-501.bin",	0x2000, 0xc2dbef1b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136023-502.bin",	0x2000, 0x316d0c7b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136023-503.bin",	0x4000, 0xb9c291a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136023-704.bin",	0x4000, 0xbf225ea0, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136023-509.bin",	0x2000, 0x46c021a4, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136023-510.bin",	0x2000, 0x8c9cf017, 2 | BRF_GRA },           //  5
	{ "136023-511.bin",	0x2000, 0x66fd8a34, 2 | BRF_GRA },           //  6
	{ "136023-512.bin",	0x2000, 0x48c8079e, 2 | BRF_GRA },           //  7
	{ "136023-513.bin",	0x2000, 0x13f1cbab, 2 | BRF_GRA },           //  8
	{ "136023-514.bin",	0x2000, 0x6f8c7991, 2 | BRF_GRA },           //  9
	{ "136023-515.bin",	0x2000, 0x835438a0, 2 | BRF_GRA },           // 10

	{ "136023-105.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136023-106.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136023-107.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136023-108.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(cloakfr)
STD_ROM_FN(cloakfr)

struct BurnDriver BurnDrvCloakfr = {
	"cloakfr", "cloak", NULL, NULL, "1983",
	"Cloak & Dagger (French)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cloakfrRomInfo, cloakfrRomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Cloak & Dagger (German)

static struct BurnRomInfo cloakgrRomDesc[] = {
	{ "136023-501.bin",	0x2000, 0xc2dbef1b, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136023-502.bin",	0x2000, 0x316d0c7b, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136023-503.bin",	0x4000, 0xb9c291a6, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136023-604.bin",	0x4000, 0x7ac66aea, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136023-509.bin",	0x2000, 0x46c021a4, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136023-510.bin",	0x2000, 0x8c9cf017, 2 | BRF_GRA },           //  5
	{ "136023-511.bin",	0x2000, 0x66fd8a34, 2 | BRF_GRA },           //  6
	{ "136023-512.bin",	0x2000, 0x48c8079e, 2 | BRF_GRA },           //  7
	{ "136023-513.bin",	0x2000, 0x13f1cbab, 2 | BRF_GRA },           //  8
	{ "136023-514.bin",	0x2000, 0x6f8c7991, 2 | BRF_GRA },           //  9
	{ "136023-515.bin",	0x2000, 0x835438a0, 2 | BRF_GRA },           // 10

	{ "136023-105.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136023-106.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136023-107.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136023-108.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(cloakgr)
STD_ROM_FN(cloakgr)

struct BurnDriver BurnDrvCloakgr = {
	"cloakgr", "cloak", NULL, NULL, "1983",
	"Cloak & Dagger (German)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, cloakgrRomInfo, cloakgrRomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Agent X (prototype, rev 4)

static struct BurnRomInfo agentx4RomDesc[] = {
	{ "136401-023.bin",	0x2000, 0xf7edac86, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136402-023.bin",	0x2000, 0xdb5e2382, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136403-023.bin",	0x4000, 0x87de01b4, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136404-023.bin",	0x4000, 0xb97219dc, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136409-023.bin",	0x2000, 0xadd4a749, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136410-023.bin",	0x2000, 0xb1e1c074, 2 | BRF_GRA },           //  5
	{ "136411-023.bin",	0x2000, 0x6d0c1ee5, 2 | BRF_GRA },           //  6
	{ "136412-023.bin",	0x2000, 0x815af543, 2 | BRF_GRA },           //  7
	{ "136413-023.bin",	0x2000, 0x2ad9e622, 2 | BRF_GRA },           //  8
	{ "136414-023.bin",	0x2000, 0xcadf9ab0, 2 | BRF_GRA },           //  9
	{ "136415-023.bin",	0x2000, 0xf5024961, 2 | BRF_GRA },           // 10

	{ "136105-023.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136106-023.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136107-023.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136108-023.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(agentx4)
STD_ROM_FN(agentx4)

struct BurnDriver BurnDrvAgentx4 = {
	"agentx4", "cloak", NULL, NULL, "1983",
	"Agent X (prototype, rev 4)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, agentx4RomInfo, agentx4RomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Agent X (prototype, rev 3)

static struct BurnRomInfo agentx3RomDesc[] = {
	{ "136301-023.bin",	0x2000, 0xfba1d9de, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136302-023.bin",	0x2000, 0xe5694c72, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136303-023.bin",	0x4000, 0x70ef51c5, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136304-023.bin",	0x4000, 0xf4a86cda, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136309-023.bin",	0x2000, 0x1c04282d, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136310-023.bin",	0x2000, 0xa61eaa88, 2 | BRF_GRA },           //  5
	{ "136311-023.bin",	0x2000, 0xa670f4b4, 2 | BRF_GRA },           //  6
	{ "136312-023.bin",	0x2000, 0xe955af62, 2 | BRF_GRA },           //  7
	{ "136313-023.bin",	0x2000, 0xb4b46d9d, 2 | BRF_GRA },           //  8
	{ "136314-023.bin",	0x2000, 0x3138a3b2, 2 | BRF_GRA },           //  9
	{ "136315-023.bin",	0x2000, 0xd12f5523, 2 | BRF_GRA },           // 10

	{ "136105-023.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136106-023.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136107-023.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136108-023.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(agentx3)
STD_ROM_FN(agentx3)

struct BurnDriver BurnDrvAgentx3 = {
	"agentx3", "cloak", NULL, NULL, "1983",
	"Agent X (prototype, rev 3)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, agentx3RomInfo, agentx3RomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Agent X (prototype, rev 2)

static struct BurnRomInfo agentx2RomDesc[] = {
	{ "136201-023.bin",	0x2000, 0xe6c7041f, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136202-023.bin",	0x2000, 0x4c94929e, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136203-023.bin",	0x4000, 0xc7a59697, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136204-023.bin",	0x4000, 0xe6e06a9c, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136209-023.bin",	0x2000, 0x319772d8, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136210-023.bin",	0x2000, 0x6e95f628, 2 | BRF_GRA },           //  5
	{ "136211-023.bin",	0x2000, 0x8d936132, 2 | BRF_GRA },           //  6
	{ "136212-023.bin",	0x2000, 0x9a3074c8, 2 | BRF_GRA },           //  7
	{ "136213-023.bin",	0x2000, 0x15984981, 2 | BRF_GRA },           //  8
	{ "136214-023.bin",	0x2000, 0xdba311ec, 2 | BRF_GRA },           //  9
	{ "136215-023.bin",	0x2000, 0xdc20c185, 2 | BRF_GRA },           // 10

	{ "136105-023.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136106-023.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136107-023.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136108-023.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(agentx2)
STD_ROM_FN(agentx2)

struct BurnDriver BurnDrvAgentx2 = {
	"agentx2", "cloak", NULL, NULL, "1983",
	"Agent X (prototype, rev 2)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, agentx2RomInfo, agentx2RomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};


// Agent X (prototype, rev 1)

static struct BurnRomInfo agentx1RomDesc[] = {
	{ "136101-023.bin",	0x2000, 0xa12b9c22, 1 | BRF_PRG | BRF_ESS }, //  0 M6502 #0 code
	{ "136102-023.bin",	0x2000, 0xe65d30df, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136103-023.bin",	0x4000, 0xc6f8a128, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136104-023.bin",	0x4000, 0xdb002945, 1 | BRF_PRG | BRF_ESS }, //  3

	{ "136109-023.bin",	0x2000, 0x31487f5d, 2 | BRF_GRA },           //  4 M6502 #1 code
	{ "136110-023.bin",	0x2000, 0x1ee38ecb, 2 | BRF_GRA },           //  5
	{ "136111-023.bin",	0x2000, 0xca6a6b0c, 2 | BRF_GRA },           //  6
	{ "136112-023.bin",	0x2000, 0x933051bc, 2 | BRF_GRA },           //  7
	{ "136113-023.bin",	0x2000, 0x1706a674, 2 | BRF_GRA },           //  8
	{ "136114-023.bin",	0x2000, 0x7c7c905d, 2 | BRF_GRA },           //  9
	{ "136115-023.bin",	0x2000, 0x7f36710c, 2 | BRF_GRA },           // 10

	{ "136105-023.bin",	0x1000, 0xee443909, 3 | BRF_GRA },           // 11 Background tiles
	{ "136106-023.bin",	0x1000, 0xd708b132, 3 | BRF_GRA },           // 12

	{ "136107-023.bin",	0x1000, 0xc42c84a4, 4 | BRF_GRA },           // 13 Sprites
	{ "136108-023.bin",	0x1000, 0x4fe13d58, 4 | BRF_GRA },           // 14

	{ "136023-116.3n",	0x0100, 0xef2668e5, 5 | BRF_OPT },           // 15 Timing prom
};

STD_ROM_PICK(agentx1)
STD_ROM_FN(agentx1)

struct BurnDriver BurnDrvAgentx1 = {
	"agentx1", "cloak", NULL, NULL, "1983",
	"Agent X (prototype, rev 1)\0", NULL, "Atari", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, agentx1RomInfo, agentx1RomName, NULL, NULL, CloakInputInfo, CloakDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x40,
	256, 232, 4, 3
};
