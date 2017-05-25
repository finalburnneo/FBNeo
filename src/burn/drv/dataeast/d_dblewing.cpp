// FB Alpha Double Wings driver module
// Based on MAME driver by David Haywood

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "deco16ic.h"
#include "burn_ym2151.h"
#include "msm6295.h"
#include "deco146.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *Drv68KROM;
static UINT8 *Drv68KCode;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvSndROM0;
static UINT8 *Drv68KRAM;
static UINT8 *DrvUnkRAM0;
static UINT8 *DrvUnkRAM1;
static UINT8 *DrvSprRAM;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvPalRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 flipscreen;
static UINT8 soundlatch;
static UINT8 sound_irq;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT8 DrvDips[2];
static UINT8 DrvReset;
static UINT16 DrvInputs[2];

static struct BurnInputInfo DblewingInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 15,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy1 + 8,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy1 + 9,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy1 + 10,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 11,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy1 + 12,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy1 + 13,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy1 + 14,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Service",		BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Dblewing)

static struct BurnDIPInfo DblewingDIPList[]=
{
	{0x14, 0xff, 0xff, 0xff, NULL			},
	{0x15, 0xff, 0xff, 0x7f, NULL			},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x14, 0x01, 0x07, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x01, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x07, 0x07, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x07, 0x06, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x07, 0x05, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x07, 0x04, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x07, 0x03, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x07, 0x02, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    8, "Coin B"		},
	{0x14, 0x01, 0x38, 0x00, "3 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x08, "2 Coins 1 Credits"	},
	{0x14, 0x01, 0x38, 0x38, "1 Coin  1 Credits"	},
	{0x14, 0x01, 0x38, 0x30, "1 Coin  2 Credits"	},
	{0x14, 0x01, 0x38, 0x28, "1 Coin  3 Credits"	},
	{0x14, 0x01, 0x38, 0x20, "1 Coin  4 Credits"	},
	{0x14, 0x01, 0x38, 0x18, "1 Coin  5 Credits"	},
	{0x14, 0x01, 0x38, 0x10, "1 Coin  6 Credits"	},

//	not supported
//	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
//	{0x14, 0x01, 0x40, 0x40, "Off"			},
//	{0x14, 0x01, 0x40, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Region"		},
	{0x14, 0x01, 0x80, 0x80, "Japan"		},
	{0x14, 0x01, 0x80, 0x00, "Korea"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x15, 0x01, 0x03, 0x02, "1"			},
	{0x15, 0x01, 0x03, 0x01, "2"			},
	{0x15, 0x01, 0x03, 0x03, "3"			},
	{0x15, 0x01, 0x03, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x0c, 0x08, "Easy"			},
	{0x15, 0x01, 0x0c, 0x0c, "Normal"		},
	{0x15, 0x01, 0x0c, 0x04, "Hard"			},
	{0x15, 0x01, 0x0c, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x15, 0x01, 0x30, 0x20, "Every 100,000"	},
	{0x15, 0x01, 0x30, 0x30, "Every 150,000"	},
	{0x15, 0x01, 0x30, 0x10, "Every 300,000"	},
	{0x15, 0x01, 0x30, 0x00, "250,000 Only"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x15, 0x01, 0x40, 0x00, "Off"			},
	{0x15, 0x01, 0x40, 0x40, "On"			},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x80, 0x80, "Off"			},
	{0x15, 0x01, 0x80, 0x00, "On"			},
};

STDDIPINFO(Dblewing)

static void __fastcall dblewing_write_word(UINT32 address, UINT16 data)
{
	if ((address & 0xffc000) == 0x280000) {
		deco146_104_prot_ww(0, address, data);
		return;
	}

	deco16_write_control_word(0, address, 0x28c000, data);
}

static void __fastcall dblewing_write_byte(UINT32 address, UINT8 data)
{
	if ((address & 0xffc000) == 0x280000) {
		deco146_104_prot_wb(0, address, data);
		return;
	}
}

static UINT16 __fastcall dblewing_read_word(UINT32 address)
{
	if ((address & 0xffc000) == 0x280000) {	
		return deco146_104_prot_rw(0, address);
	}

	return 0;
}

static UINT8 __fastcall dblewing_read_byte(UINT32 address)
{
	if ((address & 0xffc000) == 0x280000) {	
		return deco146_104_prot_rb(0, address);
	}

	return 0;
}

static void __fastcall dblewing_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xa000:
			BurnYM2151SelectRegister(data);
		return;

		case 0xa001:
			BurnYM2151WriteRegister(data);
		return;

		case 0xb000:
		case 0xf000:
			MSM6295Command(0, data);
		return;
	}
}

static UINT8 __fastcall dblewing_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
		case 0xa001:
			return BurnYM2151ReadStatus();

		case 0xb000:
		case 0xf000:
			return MSM6295ReadStatus(0);

		case 0xc000:
			return soundlatch;

		case 0xd000:
			sound_irq &= ~0x02;
			ZetSetIRQLine(0, (sound_irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
			return sound_irq;
	}

	return 0;
}

static void sound_callback(UINT16 data)
{
	soundlatch = data & 0xff;
	sound_irq |=  0x02;

	ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
}

static UINT16 input_read()
{
	return DrvInputs[0];
}

static UINT16 system_read()
{
	return (DrvInputs[1] & 0x7) | deco16_vblank;
}

static UINT16 dips_read()
{
	return (DrvDips[1] * 256) + DrvDips[0];
}

static UINT8 __fastcall dblewing_sound_read_port(UINT16 port)
{
	return DrvZ80ROM[port];
}

static void DrvYM2151IrqHandler(INT32 state)
{
	if (state) 
		sound_irq |=  0x01;
	else
		sound_irq &= ~0x01;

	ZetSetIRQLine(0, (sound_irq) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static INT32 dblewing_bank_callback( const INT32 bank )
{
	return ((bank >> 4) & 0x7) * 0x1000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	SekOpen(0);
	SekReset();
	SekClose();

	ZetOpen(0);
	ZetReset();
	BurnYM2151Reset();
	ZetClose();

	MSM6295Reset(0);

	deco16Reset();

	flipscreen = 0;
	soundlatch = 0;
	sound_irq = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	Drv68KROM	= Next; Next += 0x080000;
	Drv68KCode	= Next; Next += 0x080000;

	DrvZ80ROM	= Next; Next += 0x010000;

	DrvGfxROM0	= Next; Next += 0x200000;
	DrvGfxROM1	= Next; Next += 0x200000;
	DrvGfxROM2	= Next; Next += 0x400000;

	MSM6295ROM	= Next;
	DrvSndROM0	= Next; Next += 0x080000;

	DrvPalette	= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam		= Next;

	Drv68KRAM	= Next; Next += 0x004000;
	DrvUnkRAM0	= Next; Next += 0x000400;
	DrvUnkRAM1	= Next; Next += 0x000400;
	DrvSprRAM	= Next; Next += 0x000800;
	DrvPalRAM	= Next; Next += 0x000800;
	DrvZ80RAM	= Next; Next += 0x000800;

	RamEnd		= Next;

	MemEnd		= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(58.00);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(Drv68KROM  + 0x000000,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KROM  + 0x000001,  1, 2)) return 1;

		if (BurnLoadRom(DrvZ80ROM  + 0x000000,  2, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000,  3, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM2 + 0x000000,  4, 2)) return 1;
		if (BurnLoadRom(DrvGfxROM2 + 0x000001,  5, 2)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000,  6, 1)) return 1;
		memcpy (DrvSndROM0 + 0x20000, DrvSndROM0, 0x20000);

		deco102_decrypt_cpu(Drv68KROM, Drv68KCode, 0x80000, 0x399d, 0x25, 0x3d);
		deco56_decrypt_gfx(DrvGfxROM1, 0x100000);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM0, 0x100000, 1);
		deco16_tile_decode(DrvGfxROM1, DrvGfxROM1, 0x100000, 0);
		deco16_sprite_decode(DrvGfxROM2, 0x200000);
	}	

	deco16Init(1, 0, 1);
	deco16_set_graphics(DrvGfxROM0, 0x100000 * 2, DrvGfxROM1, 0x100000 * 2, NULL, 0);
	deco16_set_global_offsets(0, 8);
	deco16_set_color_base(0, 0);
	deco16_set_color_base(1, 0x100);
	deco16_set_color_mask(0, 0xf);
	deco16_set_color_mask(1, 0xf);
	deco16_set_transparency_mask(0, 0xf);
	deco16_set_transparency_mask(1, 0xf);
	deco16_set_bank_callback(0, dblewing_bank_callback);
	deco16_set_bank_callback(1, dblewing_bank_callback);

	SekInit(0, 0x68000);
	SekOpen(0);
	SekMapMemory(Drv68KROM,			0x000000, 0x07ffff, MAP_READ);
	SekMapMemory(Drv68KCode,		0x000000, 0x07ffff, MAP_FETCH);
	SekMapMemory(deco16_pf_ram[0],		0x100000, 0x100fff, MAP_RAM);
	SekMapMemory(deco16_pf_ram[1],		0x102000, 0x102fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[0],	0x104000, 0x104fff, MAP_RAM);
	SekMapMemory(deco16_pf_rowscroll[1],	0x106000, 0x106fff, MAP_RAM);
	SekMapMemory(DrvUnkRAM0,		0x284000, 0x284400, MAP_RAM);
	SekMapMemory(DrvUnkRAM1,		0x288000, 0x288400, MAP_RAM);
	SekMapMemory(DrvSprRAM,			0x300000, 0x3007ff, MAP_RAM);
	SekMapMemory(DrvPalRAM,			0x320000, 0x3207ff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff0000, 0xff3fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff4000, 0xff7fff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xff8000, 0xffbfff, MAP_RAM);
	SekMapMemory(Drv68KRAM,			0xffc000, 0xffffff, MAP_RAM);
	SekSetWriteWordHandler(0,		dblewing_write_word);
	SekSetWriteByteHandler(0,		dblewing_write_byte);
	SekSetReadWordHandler(0,		dblewing_read_word);
	SekSetReadByteHandler(0,		dblewing_read_byte);
	SekClose();

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM,			0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM,			0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(dblewing_sound_write);
	ZetSetReadHandler(dblewing_sound_read);
	ZetSetInHandler(dblewing_sound_read_port);
	ZetClose();

	deco_104_init();
	deco_146_104_set_interface_scramble_interleave();
	deco_146_104_set_use_magic_read_address_xor(1);
	deco_146_104_set_port_a_cb(input_read); // inputs
	deco_146_104_set_port_b_cb(system_read); // system
	deco_146_104_set_port_c_cb(dips_read); // dips
	deco_146_104_set_soundlatch_cb(sound_callback);

	BurnYM2151Init(3580000);
	BurnYM2151SetIrqHandler(&DrvYM2151IrqHandler);
	BurnYM2151SetAllRoutes(1.00, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1000000 / 132, 1);
	MSM6295SetRoute(0, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	deco16Exit();

	MSM6295Exit(0);
	BurnYM2151Exit();

	SekExit();
	ZetExit();

	BurnFree (AllMem);

	MSM6295ROM = NULL;

	return 0;
}

static void draw_sprites()
{
	UINT16 *spriteram = (UINT16 *)DrvSprRAM;

	for (INT32 offs = 0; offs < 0x400; offs += 4)
	{
		INT32 inc, mult;

		INT32 sprite = BURN_ENDIAN_SWAP_INT16(spriteram[offs+1]);

		INT32 y = BURN_ENDIAN_SWAP_INT16(spriteram[offs]);

		if ((y & 0x1000) && (nCurrentFrame & 1)) continue;

		INT32 w = y & 0x0800;
		INT32 x = BURN_ENDIAN_SWAP_INT16(spriteram[offs + 2]);
		INT32 colour = (x >> 9) & 0x1f;

		INT32 fx = y & 0x2000;
		INT32 fy = y & 0x4000;
		INT32 multi = (1 << ((y & 0x0600) >> 9)) - 1;

		x = x & 0x01ff;
		y = y & 0x01ff;
		if (x >= 320) x -= 512;
		if (y >= 256) y -= 512;
		y = 240 - y;
		x = 304 - x;

		if (x > 320) continue;

		sprite &= ~multi;

		if (fy)
			inc = -1;
		else
		{
			sprite += multi;
			inc = 1;
		}

		if (!flipscreen)
		{
			y = 240 - y;
			x = 304 - x;
			if (fy) fy = 0; else fy = 1;
			if (fx) fx = 0; else fx = 1;
			mult = 16;

		}
		else mult = -16;

		INT32 mult2 = multi + 1;

		while (multi >= 0)
		{
			INT32 code = (sprite - multi * inc) & 0x3fff;

			if (fy) {
				if (fx) {
					Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
				}
			} else {
				if (fx) {
					Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
				} else {
					Render16x16Tile_Mask_Clip(pTransDraw, code, x, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
				}
			}

			if (w)
			{
				code = ((sprite - multi * inc)-mult2) & 0x3fff;

				if (fy) {
					if (fx) {
						Render16x16Tile_Mask_FlipXY_Clip(pTransDraw, code, x-16, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
					} else {
						Render16x16Tile_Mask_FlipY_Clip(pTransDraw, code, x-16, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
					}
				} else {
					if (fx) {
						Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, x-16, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
					} else {
						Render16x16Tile_Mask_Clip(pTransDraw, code, x-16, (y + mult * multi) - 8, colour, 4, 0, 0x200, DrvGfxROM2);
					}
				}
			}

			multi--;
		}
	}
}

static void DrvPaletteUpdate()
{
	UINT16 *p = (UINT16*)DrvPalRAM;

	for (INT32 i = 0; i < 0x800 / 2; i++)
	{
		UINT8 b = (p[i] >> 8) & 0xf;
		UINT8 g = (p[i] >> 4) & 0xf;
		UINT8 r = (p[i] >> 0) & 0xf;

		DrvPalette[i] = BurnHighCol(r+r*16, g+g*16, b+b*16, 0);
	}
}

static INT32 DrvDraw()
{
//	if (DrvRecalc) {
		DrvPaletteUpdate();
		DrvRecalc = 0;
//	}

	deco16_pf12_update();

	flipscreen = 1; //~deco16_pf_control[0][0] & 0x80;

	//bprintf (0, _T("%4.4x\n"), deco16_pf_control[0][0]);

	for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) {
		pTransDraw[i] = 0;
	}

	if (nBurnLayer & 1) deco16_draw_layer(1, pTransDraw, 0);

	if (nBurnLayer & 2) deco16_draw_layer(0, pTransDraw, 0);

	if (nBurnLayer & 4) draw_sprites();

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
		memset (DrvInputs, 0xff, 2 * sizeof(UINT16)); 

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[2] = { 14000000 / 58, 3580000 / 58 };
	INT32 nCyclesDone[2] = { 0, 0 };
	INT32 nSoundBufferPos = 0;

	SekOpen(0);
	ZetOpen(0);

	deco16_vblank = 8;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == 29) deco16_vblank = 0;
		if (i == 248) {
			deco16_vblank = 0x08;
			SekSetIRQLine(6, CPU_IRQSTATUS_AUTO);
		}

		nCyclesDone[0] += SekRun(nCyclesTotal[0] / nInterleave);
		nCyclesDone[1] += ZetRun(nCyclesTotal[1] / nInterleave);

		if (pBurnSoundOut && i%4==3) {
			INT32 nSegmentLength = nBurnSoundLen / (nInterleave/4);
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}
	}

	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			MSM6295Render(0, pSoundBuf, nSegmentLength);
		}
	}

	ZetClose();
	SekClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029682;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);

		deco16Scan();

		BurnYM2151Scan(nAction);
		MSM6295Scan(0, nAction);

		SCAN_VAR(flipscreen);
		SCAN_VAR(soundlatch);
		SCAN_VAR(sound_irq);

	}

	return 0;
}


// Double Wings

static struct BurnRomInfo dblewingRomDesc[] = {
	{ "kp_00-.3d",	0x040000, 0x547dc83e, 1 | BRF_PRG | BRF_ESS }, //  0 68k Code
	{ "kp_01-.5d",	0x040000, 0x7a210c33, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "kp_02-.10h",	0x010000, 0xdef035fa, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 Code

	{ "mbe-02.8h",	0x100000, 0x5a6d3ac5, 3 | BRF_GRA },           //  3 Character and Background Tiles

	{ "mbe-00.14a",	0x100000, 0xe33f5c93, 4 | BRF_GRA },           //  4 Sprites
	{ "mbe-01.16a",	0x100000, 0xef452ad7, 4 | BRF_GRA },           //  5

	{ "kp_03-.16h",	0x020000, 0x5d7f930d, 5 | BRF_SND },           //  6 OKI M6295 Samples
};

STD_ROM_PICK(dblewing)
STD_ROM_FN(dblewing)

struct BurnDriver BurnDrvDblewing = {
	"dblewing", NULL, NULL, NULL, "1993",
	"Double Wings\0", NULL, "Mitchell", "DECO IC16",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_DATAEAST, GBF_VERSHOOT, 0,
	NULL, dblewingRomInfo, dblewingRomName, NULL, NULL, DblewingInputInfo, DblewingDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x400,
	240, 320, 3, 4
};
