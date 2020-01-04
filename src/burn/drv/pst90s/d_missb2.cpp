// FB Alpha Miss Bubble 2 / Bubble Pong Pong driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3526.h"
#include "msm6295.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM;
static UINT8 *DrvVidPROM;
static UINT8 *DrvZ80RAM0;
static UINT8 *DrvZ80RAM1;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvVidRAM;
static UINT8 *DrvObjRAM;
static UINT8 *DrvShareRAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bgvram;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT8 sound_nmi_enable;
static UINT8 sound_pending_nmi;
static UINT8 sound_cpu_in_reset;
static UINT8 video_enable;
static UINT8 bankdata[2];

static UINT8 *Drvfe00RAM;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo Missb2InputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy1 + 3,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 2"	},

	{"P2 Coin",			BIT_DIGITAL,	DrvJoy1 + 2,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 0,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 2"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 3,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 2,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Missb2)

static struct BurnDIPInfo Missb2DIPList[]=
{
	{0x0f, 0xff, 0xff, 0xfe, NULL					},
	{0x10, 0xff, 0xff, 0x3f, NULL					},

	{0   , 0xfe, 0   ,    2, "Language"				},
	{0x0f, 0x01, 0x01, 0x00, "English"				},
	{0x0f, 0x01, 0x01, 0x01, "Japanese"				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x0f, 0x01, 0x02, 0x02, "Off"					},
	{0x0f, 0x01, 0x02, 0x00, "On"					},
	
	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x0f, 0x01, 0x04, 0x00, "Off"					},
	{0x0f, 0x01, 0x04, 0x04, "On"					},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x0f, 0x01, 0x08, 0x00, "Off"					},
	{0x0f, 0x01, 0x08, 0x08, "On"					},

	{0   , 0xfe, 0   ,    4, "Coin A"				},
	{0x0f, 0x01, 0x30, 0x10, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0x30, 0x30, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0x30, 0x00, "2 Coins 3 Credits"	},
	{0x0f, 0x01, 0x30, 0x20, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x0f, 0x01, 0xc0, 0x40, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0xc0, 0xc0, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0xc0, 0x00, "2 Coins 3 Credits"	},
	{0x0f, 0x01, 0xc0, 0x80, "1 Coin  2 Credits"	},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x10, 0x01, 0x03, 0x02, "Easy"					},
	{0x10, 0x01, 0x03, 0x03, "Medium"				},
	{0x10, 0x01, 0x03, 0x01, "Hard"					},
	{0x10, 0x01, 0x03, 0x00, "Hardest"				},

	{0   , 0xfe, 0   ,    4, "Bonus Life"			},
	{0x10, 0x01, 0x0c, 0x08, "20K 80K"				},
	{0x10, 0x01, 0x0c, 0x0c, "30K 100K"				},
	{0x10, 0x01, 0x0c, 0x04, "40K 200K"				},
	{0x10, 0x01, 0x0c, 0x00, "50K 250K"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x10, 0x01, 0x30, 0x10, "1"					},
	{0x10, 0x01, 0x30, 0x00, "2"					},
	{0x10, 0x01, 0x30, 0x30, "3"					},
	{0x10, 0x01, 0x30, 0x20, "5"					},

	{0   , 0xfe, 0   ,    4, "Monster Speed"		},
	{0x10, 0x01, 0xc0, 0x00, "Normal"				},
	{0x10, 0x01, 0xc0, 0x40, "Medium"				},
	{0x10, 0x01, 0xc0, 0x80, "High"					},
	{0x10, 0x01, 0xc0, 0xc0, "Very High"			},
};

STDDIPINFO(Missb2)

static void bankswitch0(INT32 data)
{
	bankdata[0] = data;
	
	INT32 bank = ((data ^ 4) & 7) * 0x4000;

	ZetMapMemory(DrvZ80ROM0 + 0x10000 + bank, 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall missb2_main_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xfa00:
		{
			soundlatch = data;
			if (sound_nmi_enable) {
				ZetNmi(2);
			} else {
				sound_pending_nmi = 1;
			}
		}
		return;

		case 0xfb40:
		{
			sound_cpu_in_reset = ~data & 0x10;
			if (sound_cpu_in_reset) {
				ZetReset(1);
			}

			bankswitch0(data);

			video_enable = data & 0x40;
			flipscreen = data & 0x80;
		}
		return;
	}
	
	if (address >= 0xfe00) Drvfe00RAM[address & 0x1ff] = data;
}

static UINT8 __fastcall missb2_main_read(UINT16 address)
{
	switch (address)
	{
		case 0xff00:
		case 0xff01:
			return DrvDips[address & 1];

		case 0xff02:
		case 0xff03:
			return DrvInputs[address & 1];
	}

	if (address >= 0xfe00) return Drvfe00RAM[address & 0x1ff];
	
	return 0;
}

static void bankswitch1(INT32 data)
{
	bankdata[1] = data;
	
	INT32 bank = ((data & 2) ? 1 : 0) | ((data & 1) ? 4 : 0);

	ZetMapMemory(DrvZ80ROM1 + 0x8000 + bank * 0x1000, 0x9000, 0xafff, MAP_ROM);
}

static void __fastcall missb2_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xd000:
			bankswitch1(data);
		return;

		case 0xd002:
		return;	//nop

		case 0xd003:
			bgvram = data;
		return;
	}
}

static void __fastcall missb2_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x9000:
			MSM6295Write(0, data);
		return;

		case 0xa000:
		case 0xa001:
			BurnYM3526Write(address & 1, data);
		return;

		case 0xb000:
		return;	//	nop

		case 0xb001:
			sound_nmi_enable = 1;
			if (sound_pending_nmi) {
				ZetNmi();
				sound_pending_nmi = 0;
			}
		return;

		case 0xb002:
			sound_nmi_enable = 0;
		return;
	}
}

static UINT8 __fastcall missb2_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x9000:
			return MSM6295Read(0);

		case 0xa000:
		case 0xa001:
			return BurnYM3526Read(address & 1);

		case 0xb000:
			return soundlatch;

		case 0xb001:
			return 0; // nop
	}

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetReset(0);
	ZetReset(1);

	ZetOpen(2);
	BurnYM3526Reset();
	MSM6295Reset(0);
	ZetReset();
	ZetClose();

	bgvram = 0;
	soundlatch = 0;
	flipscreen = 0;
	sound_nmi_enable = 0;
	sound_pending_nmi = 0;
	sound_cpu_in_reset = 0;
	video_enable = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x020000;
	DrvZ80ROM1		= Next; Next += 0x010000;
	DrvZ80ROM2		= Next; Next += 0x008000;

	DrvGfxROM0		= Next; Next += 0x100000;
	DrvGfxROM1		= Next; Next += 0x200000;

	MSM6295ROM		= Next;
	DrvSndROM		= Next; Next += 0x040000;

	DrvVidPROM		= Next; Next += 0x000100;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM0		= Next; Next += 0x000200;
	DrvZ80RAM1		= Next; Next += 0x000800;
	DrvZ80RAM2		= Next; Next += 0x001000;
	DrvVidRAM		= Next; Next += 0x001d00;
	DrvObjRAM		= Next; Next += 0x000300;
	DrvShareRAM		= Next; Next += 0x001800;
	DrvPalRAM		= Next; Next += 0x000400;
	
	Drvfe00RAM		= Next; Next += 0x000200;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode(INT32 type)
{
	INT32 Plane0[8] = { (0x40000*8*0)+0, (0x40000*8*0)+4, (0x40000*8*1)+0, (0x40000*8*1)+4, 
				(0x40000*8*2)+0, (0x40000*8*2)+4, (0x40000*8*3)+0, (0x40000*8*3)+4 };
	INT32 XOffs0[8] = { STEP4(3,-1), STEP4(8+3, -1) };
	INT32 YOffs0[8] = { STEP8(0,16) };
	INT32 Plane1[8] = { STEP8(0,1) };
	INT32 XOffs1[2][256] = { { 0, }, { STEP64(0,16), STEP64(0x400,16), STEP64(0x800, 16), STEP64(0xc00, 16) } };
	INT32 YOffs1[16] = { STEP16(0,128) };

	for (INT32 i = 0; i < 256; i++)
	{
		INT32 j = (i & 1) | ((i & 2) << 10) | ((i & 4) << 1) | ((i & 8) >> 1) | ((i & 0x10) << 4) | ((i & 0x20) << 5) | ((i & 0x40) << 3) | ((i & 0x80) >> 6);
		XOffs1[0][i] = j * 8;
		j = ((i & 0xf0) << 4) | (i & 0xf);
		XOffs1[1][i] = j * 8;
	}

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);

	for (INT32 i = 0; i  < 0x100000; i++) tmp[i] = DrvGfxROM0[i] ^ 0xff;

	GfxDecode(0x4000, 8,   8,  8, Plane0, XOffs0, YOffs0, 0x080, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x200000);

	GfxDecode(0x0200, 8, 256, 16, Plane1, XOffs1[type], YOffs1, 0x8000, tmp, DrvGfxROM1);

	BurnFree(tmp);
}

static INT32 DrvInit(INT32 type)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x00000,   0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x10000,   1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x00000,   2, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x00000,   3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x00000,   4, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x40000,   5, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x80000,   6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0xc0000,   7, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x100001,  8, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x100000,  9, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000001, 10, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 2)) return 1;

		if (BurnLoadRom(DrvSndROM + 0x00000,   12, 1)) return 1;

		if (BurnLoadRom(DrvVidPROM + 0x00000,  13, 1)) return 1;

		DrvGfxDecode(type);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvVidRAM,			0xc000, 0xdcff, MAP_RAM);
	ZetMapMemory(DrvObjRAM,			0xdd00, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvPalRAM,			0xf800, 0xf9ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM0,		0xfc00, 0xfdff, MAP_RAM);
	ZetSetWriteHandler(missb2_main_write);
	ZetSetReadHandler(missb2_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80ROM1 + 0xb000,	0xb000, 0xb1ff, MAP_ROM);
	ZetMapMemory(DrvPalRAM + 0x0200,	0xc000, 0xc1ff, MAP_RAM);
	ZetMapMemory(DrvZ80RAM1,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(missb2_sub_write);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,		0x8000, 0x8fff, MAP_RAM);
	ZetMapMemory(DrvZ80ROM2 + 0xe000,	0xe000, 0xefff, MAP_RAM);
	ZetSetWriteHandler(missb2_sound_write);
	ZetSetReadHandler(missb2_sound_read);
	ZetClose();

	BurnYM3526Init(3000000, NULL, 0);
	BurnTimerAttachYM3526(&ZetConfig, 3000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 0.40, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1056000 / 132, 1);
	MSM6295SetRoute(0, 0.40, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	BurnYM3526Exit();
	MSM6295Exit(0);
	MSM6295ROM = NULL;

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteUpdate()
{
	for (INT32 i = 0; i < 0x400; i+=2)
	{
		INT32 r = DrvPalRAM[i+0] >> 4;
		INT32 g = DrvPalRAM[i+0] & 0xf;
		INT32 b = DrvPalRAM[i+1] >> 4;

		DrvPalette[i/2] = BurnHighCol(r+r*16,g+g*16, b+b*16, 0);
	}
}

static void draw_bg_layer()
{
	for (INT32 offs = (bgvram * 16); offs < ((bgvram * 16) | 0xf); offs++)
	{
		if ((offs & 0xf) == 0 || (offs & 0xf) == 0xf) continue;

		RenderCustomTile_Clip(pTransDraw, 256, 16, offs&0x1ff, 0, ((offs & 0xf) * 16) - 16, 0, 8, 0x100, DrvGfxROM1);
	}
}

static void draw_sprites()
{
	INT32 sx = 0;
	
	for (INT32 offs = 0; offs < 0x300; offs += 4)
	{
		if (*(UINT32 *)(&DrvObjRAM[offs]) == 0)
			continue;

		INT32 gfx_num = DrvObjRAM[offs + 1];
		INT32 gfx_attr = DrvObjRAM[offs + 3];
		UINT8 *prom_line = DrvVidPROM + 0x80 + ((gfx_num & 0xe0) >> 1);

		INT32 gfx_offs = ((gfx_num & 0x1f) * 0x80);
		if ((gfx_num & 0xa0) == 0xa0) gfx_offs |= 0x1000;

		INT32 sy = -DrvObjRAM[offs + 0];

		for (INT32 yc = 0; yc < 32; yc++)
		{
			if (prom_line[yc / 2] & 0x08) continue;

			if (!(prom_line[yc / 2] & 0x04))
			{
				sx = DrvObjRAM[offs + 2];
				if (gfx_attr & 0x40) sx -= 256;
			}

			for (INT32 xc = 0; xc < 2; xc++)
			{
				INT32 goffs = gfx_offs + xc * 0x40 + (yc & 7) * 0x02 + (prom_line[yc/2] & 0x03) * 0x10;
				INT32 code = DrvVidRAM[goffs] + 256 * (DrvVidRAM[goffs + 1] & 0x03) + 1024 * (gfx_attr & 0x0f);
				INT32 flipx = DrvVidRAM[goffs + 1] & 0x40;
				INT32 flipy = DrvVidRAM[goffs + 1] & 0x80;
				INT32 x = sx + xc * 8;
				INT32 y = (sy + yc * 8) & 0xff;

				if (flipscreen)
				{
					x = 248 - x;
					y = 248 - y;
					flipx = !flipx;
					flipy = !flipy;
				}

				Draw8x8MaskTile(pTransDraw, code & 0x3fff, x, y - 16, flipx, flipy, 0, 8, 0xff, 0, DrvGfxROM0);
			}
		}

		sx += 16;
	}
}

static INT32 DrvDraw()
{
	//if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 1;
	//}

	BurnTransferClear(0xff);

	if (video_enable)
	{
		if (nBurnLayer & 1) draw_bg_layer();
		if (nBurnLayer & 2) draw_sprites();
	}

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 2);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 100;
	INT32 nCyclesTotal[3] = { 6000000 / 60, 6000000 / 60, 3000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == nInterleave - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		if (sound_cpu_in_reset) {
			nCyclesDone[1] += nCyclesTotal[1] / nInterleave;
			ZetIdle(nCyclesTotal[1] / nInterleave);
		} else {
			nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);
		}
		if (i == nInterleave - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdateYM3526((i + 1) * (nCyclesTotal[2] / nInterleave));
		if (i == nInterleave - 1) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();
	}

	ZetOpen(2);

	BurnTimerEndFrameYM3526(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		BurnYM3526Scan(nAction,pnMin);
		MSM6295Scan(nAction, pnMin);

		SCAN_VAR(bgvram);
		SCAN_VAR(soundlatch);
		SCAN_VAR(flipscreen);
		SCAN_VAR(sound_nmi_enable);
		SCAN_VAR(sound_pending_nmi);
		SCAN_VAR(sound_cpu_in_reset);
		SCAN_VAR(video_enable);
		SCAN_VAR(bankdata);
	}

	if (nAction & ACB_WRITE) {
		ZetOpen(0);
		bankswitch0(bankdata[0]);
		ZetClose();

		ZetOpen(1);
		bankswitch1(bankdata[1]);
		ZetClose();
	}

	return 0;
}


// Miss Bubble II

static struct BurnRomInfo missb2RomDesc[] = {
	{ "msbub2-u.204",		0x10000, 0xb633bdde, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "msbub2-u.203",		0x10000, 0x29fd8afe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "msbub2-u.11",		0x10000, 0x003dc092, 2 | BRF_GRA },           //  2 Z80 #1 Code

	{ "msbub2-u.211",		0x08000, 0x08e5d846, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #2 Code

	{ "msbub2-u.14",		0x40000, 0xb3164b47, 4 | BRF_GRA },           //  4 Background Tiles
	{ "msbub2-u.126",		0x40000, 0xb0a9a353, 4 | BRF_GRA },           //  5
	{ "msbub2-u.124",		0x40000, 0x4b0d8e5b, 4 | BRF_GRA },           //  6
	{ "msbub2-u.125",		0x40000, 0x77b710e2, 4 | BRF_GRA },           //  7

	{ "msbub2-u.ic1",		0x80000, 0xd621cbc3, 5 | BRF_GRA },           //  8 Sprites
	{ "msbub2-u.ic3",		0x80000, 0x90e56035, 5 | BRF_GRA },           //  9
	{ "msbub2-u.ic2",		0x80000, 0x694c2783, 5 | BRF_GRA },           // 10
	{ "msbub2-u.ic4",		0x80000, 0xbe71c9f0, 5 | BRF_GRA },           // 11

	{ "msbub2-u.13",		0x20000, 0x7a4f4272, 6 | BRF_SND },           // 12 OKI Samples

	{ "a71-25.bin",			0x00100, 0x2d0f8545, 7 | BRF_GRA },           // 13 Sprite Control PROM
};

STD_ROM_PICK(missb2)
STD_ROM_FN(missb2)

static INT32 missb2Init()
{
	return DrvInit(0);
}

struct BurnDriver BurnDrvMissb2 = {
	"missb2", NULL, NULL, NULL, "1996",
	"Miss Bubble II\0", NULL, "Alpha Co.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, missb2RomInfo, missb2RomName, NULL, NULL, NULL, NULL, Missb2InputInfo, Missb2DIPInfo,
	missb2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// Bubble Pong Pong

static struct BurnRomInfo bublpongRomDesc[] = {
	{ "u204",				0x08000, 0xdaeff303, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "u203",				0x10000, 0x29fd8afe, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "ic11",				0x10000, 0xdc1c72ba, 2 | BRF_GRA },           //  2 Z80 #1 Code

	{ "s-1.u21",			0x08000, 0x08e5d846, 3 | BRF_PRG | BRF_ESS }, //  3 Z80 #2 Code

	{ "mp-6.ic14",			0x40000, 0x00f4896b, 4 | BRF_GRA },           //  4 Background Tiles
	{ "mp-5.ic126",			0x40000, 0x1fd30a32, 4 | BRF_GRA },           //  5
	{ "5.ic124",			0x40000, 0x55666102, 4 | BRF_GRA },           //  6
	{ "6.ic125",			0x40000, 0xaa1c4c32, 4 | BRF_GRA },           //  7

	{ "4.ic1",				0x80000, 0x652a49f8, 5 | BRF_GRA },           //  8 Sprites
	{ "2.ic3",				0x80000, 0xf8b52c29, 5 | BRF_GRA },           //  9
	{ "3.ic2",				0x80000, 0x10263373, 5 | BRF_GRA },           // 10
	{ "1.ic4",				0x80000, 0x9e19ad78, 5 | BRF_GRA },           // 11

	{ "ic13",				0x20000, 0x7a4f4272, 6 | BRF_SND },           // 12 OKI Samples

	{ "a71-25.bin",			0x00100, 0x2d0f8545, 7 | BRF_GRA },           // 13 Sprite Control PROM
};

STD_ROM_PICK(bublpong)
STD_ROM_FN(bublpong)

static INT32 bublpongInit()
{
	return DrvInit(1);
}

struct BurnDriver BurnDrvBublpong = {
	"bublpong", "missb2", NULL, NULL, "1996",
	"Bubble Pong Pong\0", NULL, "Top Ltd.", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_POST90S, GBF_PLATFORM, 0,
	NULL, bublpongRomInfo, bublpongRomName, NULL, NULL, NULL, NULL, Missb2InputInfo, Missb2DIPInfo,
	bublpongInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
