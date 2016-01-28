// FB Alpha Appoooh driver module
// Based on MAME driver
// Todo:
//   finish msm5205/adpcm implementation
//   cleanup(s)
//   Robo Wres init ok?

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "msm5205.h"

static INT32 DrvZ80Bank0;
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvInputs[3];
static UINT8 DrvDip[2];
static UINT8 DrvReset;
static UINT8 DrvRecalc;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRAM0;
static UINT8 *DrvRAM1;
static UINT8 *DrvFgVidRAM;
static UINT8 *DrvBgVidRAM;
static UINT8 *DrvSprRAM0;
static UINT8 *DrvSprRAM1;
static UINT8 *DrvFgColRAM;
static UINT8 *DrvBgColRAM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxTMP0;
static UINT8 *DrvGfxTMP1;
static UINT8 *DrvColPROM;
static UINT8 *DrvMainROM;
static UINT8 *DrvSoundROM;
static UINT32 *DrvPalette;

static UINT8 scroll_x;
static UINT8 flipscreen;
static UINT8 priority;
static UINT8 interrupt_enable;
static INT32 nCyclesTotal;

static struct BurnInputInfo AppooohInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 7,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
};

STDINPUTINFO(Appoooh)


static struct BurnDIPInfo AppooohDIPList[]=
{
	{0x13, 0xff, 0xff, 0x60, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x03, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0x18, 0x18, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x18, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x18, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x20, 0x00, "Off"		},
	{0x13, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x40, 0x40, "Upright"		},
	{0x13, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x13, 0x01, 0x80, 0x00, "Easy"		},
	{0x13, 0x01, 0x80, 0x80, "Hard"		},
};

STDDIPINFO(Appoooh)

static struct BurnDIPInfo RobowresDIPList[]=
{
	{0x13, 0xff, 0xff, 0x60, NULL		},

	{0   , 0xfe, 0   ,    8, "Coin A"		},
	{0x13, 0x01, 0x07, 0x03, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x01, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x07, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x07, 0x07, "2 Coins 3 Credits"		},
	{0x13, 0x01, 0x07, 0x04, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x07, 0x06, "1 Coin  4 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0x18, 0x18, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x18, 0x00, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x18, 0x08, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x20, 0x00, "Off"		},
	{0x13, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x40, 0x40, "Upright"		},
	{0x13, 0x01, 0x40, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    0, "Language"		},
	{0x01, 0x01, 0x80, 0x00, "Japanese"		},
	{0x01, 0x01, 0x80, 0x80, "English"		},
};

STDDIPINFO(Robowres)

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x220; i++)
	{
		INT32 bit0,bit1,bit2,r,g,b;
		UINT8 pen;

		if (i < 0x100)
		/* charset #1 */
			pen = (DrvColPROM[0x020 + (i - 0x000)] & 0x0f) | 0x00;
		else
		/* charset #2 */
			pen = (DrvColPROM[0x120 + (i - 0x100)] & 0x0f) | 0x10;

		/* red component */
		bit0 = (DrvColPROM[pen] >> 0) & 0x01;
		bit1 = (DrvColPROM[pen] >> 1) & 0x01;
		bit2 = (DrvColPROM[pen] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* green component */
		bit0 = (DrvColPROM[pen] >> 3) & 0x01;
		bit1 = (DrvColPROM[pen] >> 4) & 0x01;
		bit2 = (DrvColPROM[pen] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		/* blue component */
		bit0 = 0;
		bit1 = (DrvColPROM[pen] >> 6) & 0x01;
		bit2 = (DrvColPROM[pen] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void DrvRobowresPaletteInit()
{
	for (INT32 i = 0; i < 0x220; i++)
	{
		INT32 bit0, bit1, bit2, r, g, b;


		UINT8 pen = DrvColPROM[0x020 + i] & 0x0f;


		/* red component */
		bit0 = (DrvColPROM[pen] >> 0) & 0x01;
		bit1 = (DrvColPROM[pen] >> 1) & 0x01;
		bit2 = (DrvColPROM[pen] >> 2) & 0x01;
		r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;


		/* green component */
		bit0 = (DrvColPROM[pen] >> 3) & 0x01;
		bit1 = (DrvColPROM[pen] >> 4) & 0x01;
		bit2 = (DrvColPROM[pen] >> 5) & 0x01;
		g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;


		/* blue component */
		bit0 = 0;
		bit1 = (DrvColPROM[pen] >> 6) & 0x01;
		bit2 = (DrvColPROM[pen] >> 7) & 0x01;
		b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	 	= Next; Next += 0x40000;

	AllRam			= DrvMainROM + 0xe000;

	DrvRAM0			= DrvMainROM + 0xe000;
	DrvRAM1			= DrvMainROM + 0xe800;
	DrvSprRAM0	 	= DrvMainROM + 0xf000;
	DrvFgVidRAM	    = DrvMainROM + 0xf020;
	DrvFgColRAM	    = DrvMainROM + 0xf420;
	DrvSprRAM1	 	= DrvMainROM + 0xf800;
	DrvBgVidRAM	    = DrvMainROM + 0xf820;
	DrvBgColRAM	    = DrvMainROM + 0xfc20;
	RamEnd			= DrvMainROM + 0x10000;

	DrvColPROM		= Next; Next += 0x00220;
	DrvSoundROM	    = Next; Next += 0x0a000;
	DrvPalette      = (UINT32*)Next; Next += 0x00220 * sizeof(UINT32);
	DrvGfxTMP0		= Next; Next += 0x0c000;
	DrvGfxTMP1		= Next; Next += 0x0c000;
	DrvGfxROM0		= Next; Next += 0x0c000*8;
	DrvGfxROM1		= DrvGfxROM0 + 0x0c000*4;
	DrvGfxROM2		= Next; Next += 0x0c000*8;
	DrvGfxROM3		= DrvGfxROM2 + 0x0c000*4;

	MemEnd			= Next;

	return 0;
}

static void DrvGfxDecode()
{
	UINT8 *tmp = (UINT8*)BurnMalloc(0xC000*2);
	if (tmp == NULL)
	{
		return;
	}
	memcpy (tmp, DrvGfxTMP0, 0xC000*2);
	
	INT32 Planes0[3] = { 2*2048*8*8, 1*2048*8*8, 0*2048*8*8 }; /* the bitplanes are separated */
	INT32 XOffs0[8] = {7, 6, 5, 4, 3, 2, 1, 0};
	INT32 YOffs0[8] = {STEP8(0, 8)};//{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },


	GfxDecode(0x0800, 3,  8,  8, Planes0, XOffs0, YOffs0, 0x040, tmp, DrvGfxROM0); // modulo 0x040 to verify !!!
	GfxDecode(0x0800, 3,  8,  8, Planes0, XOffs0, YOffs0, 0x040, tmp+0xc000, DrvGfxROM1); // modulo 0x040 to verify !!!

	INT32 Planes1[3] = { 2*2048*8*8, 1*2048*8*8, 0*2048*8*8 }; /* the bitplanes are separated */
	INT32 XOffs1[16] = {7, 6, 5, 4, 3, 2, 1, 0 , 8*8+7,8*8+6,8*8+5,8*8+4,8*8+3,8*8+2,8*8+1,8*8+0};
	INT32 YOffs1[16] = {STEP8(0, 8), 16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8};

	GfxDecode(0x0200, 3, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp, DrvGfxROM2);
	GfxDecode(0x0200, 3, 16, 16, Planes1, XOffs1, YOffs1, 0x100, tmp+0xc000, DrvGfxROM3);

//         GFXDECODE_ENTRY( "gfx1", 0, charlayout,        0, 32 )
//        GFXDECODE_ENTRY( "gfx2", 0, charlayout,     32*8, 32 )	   ignored
//        GFXDECODE_ENTRY( "gfx1", 0, spritelayout,      0, 32 )
//        GFXDECODE_ENTRY( "gfx2", 0, spritelayout,   32*8, 32 )	  ignored

	BurnFree (tmp);
}

static void bankswitch(INT32 data)
{
	DrvZ80Bank0 = (data & 0x40);
// cpu_setbank(1,&RAM[data&0x40 ? 0x10000 : 0x0a000]);
// 1 bank de 16k																  
	if(DrvZ80Bank0)
	{
		//bprintf (0, _T("bankswitch %02d\n"), DrvZ80Bank0);
		ZetMapArea(0xa000, 0xdfff, 0, DrvMainROM + 0x10000);
		ZetMapArea(0xa000, 0xdfff, 2, DrvMainROM + 0x10000);
	}
	else
	{
		ZetMapArea(0xa000, 0xdfff, 0, DrvMainROM + 0x0a000);
		ZetMapArea(0xa000, 0xdfff, 2, DrvMainROM + 0x0a000);
	}
}

static void __fastcall appoooh_write(unsigned short address, unsigned char data)
{
//	
//	bprintf (0, _T("appoooh_write %04x %02d\n"), address,data);
	
	 if(address>=0xf020 && address <=0xf3ff)
	{
		DrvFgVidRAM[address-0xf020] = data;
		return;
	}

	 if(address>=0xf420 && address <=0xf7ff)
	{
		DrvFgColRAM[address-0xf420] = data;
		return;
	}

	 if(address>=0xf820 && address <=0xfbff)
	{
		DrvBgVidRAM[address-0xf820] = data;
		return;
	}

	 if(address>=0xfc20 && address <=0xffff)
	{
		DrvBgColRAM[address-0xfc20] = data;
		return;
	}
	DrvMainROM[address] = data;
}

static unsigned char __fastcall appoooh_read(unsigned short address)
{
//	bprintf (0, _T("appoooh_read %04x\n"), address);
  
	 if(address>=0xf020 && address <=0xf3ff)
	{
		return DrvFgVidRAM[address-0xf020];
	}

	 if(address>=0xf420 && address <=0xf7ff)
	{
		return DrvFgColRAM[address-0xf420];
	}

	 if(address>=0xf820 && address <=0xfbff)
	{
		return DrvBgVidRAM[address-0xf820];
	}

	 if(address>=0xfc20 && address <=0xffff)
	{
		return DrvBgColRAM[address-0xfc20];
	}	

	if(address >= 0xa000 && address <= 0xdfff)
	{
		bprintf (0, _T("address >= 0xa000 && address <= 0xdfff\n"));
	}

	if(address<0x12000)
		return DrvMainROM[address];
	else
	{
		bprintf (0, _T("address %04x\n"),address);
		return 0;
	}
}

inline static INT32 DrvMSM5205SynchroniseStream(INT32 nSoundRate)
{
	return (INT64)((double)ZetTotalCycles() * nSoundRate / (nCyclesTotal * 130));
}

static void appoooh_adpcm_w( UINT8 data )
{
		bprintf (0, _T("appoooh_adpcm_w %02d\n"), data);

//        appoooh_state *state = space->machine->driver_data<appoooh_state>();

//  		MSM5205ResetWrite(0, (data >> 7) & 1);
//		MSM5205DataWrite(0, data);
//		MSM5205VCLKWrite(0, 1);
//		MSM5205VCLKWrite(0, 0);
//        state->adpcm_address = data << 8;
//        msm5205_reset_w(state->adpcm, 0);
//        state->adpcm_data = 0xffffffff;
}

static void appoooh_out_w( UINT8 data )
{
//		bprintf (0, _T("appoooh_out_w %02d\n"), data);

	/* bit 0 controls NMI */
//	if (data & 0x01) interrupt_enable_w(0,1);
//	else interrupt_enable_w(0,0);
//	if (data & 0x01)	ZetNmi();
	interrupt_enable = (data & 0x01);

	/* bit 1 flip screen */
	if ((data & 0x02) != flipscreen)
	{
		flipscreen = data & 0x02;
	}

	/* bits 2-3 unknown */

	/* bits 4-5 are playfield/sprite priority */
	/* TODO: understand how this works, currently the only thing I do is draw */
	/* the front layer behind sprites when priority == 0, and invert the sprite */
	/* order when priority == 1 */
	priority = (data & 0x30) >> 4;

	/* bit 6 ROM bank select */
	{
//		unsigned char *RAM = memory_region(REGION_CPU1);
		bankswitch(data);
//		cpu_setbank(1,&RAM[data&0x40 ? 0x10000 : 0x0a000]);
	}


	/*
        appoooh_state *state = device->machine->driver_data<appoooh_state>();


        if (state->adpcm_address != 0xffffffff)
        {
                if (state->adpcm_data == 0xffffffff)
                {
                        UINT8 *RAM = memory_region(device->machine, "adpcm");


                        state->adpcm_data = RAM[state->adpcm_address++];
                        msm5205_data_w(device, state->adpcm_data >> 4);


                        if (state->adpcm_data == 0x70)
                        {
                                state->adpcm_address = 0xffffffff;
                                msm5205_reset_w(device, 1);
                        }
                }
                else
                {
                        msm5205_data_w(device, state->adpcm_data & 0x0f );
                        state->adpcm_data = -1;
                }
        }
		*/
}

UINT8 __fastcall appoooh_in(UINT16 address)
{
//	bprintf (0, _T("appoooh_in %02d\n"), address & 0xff);
	UINT8 ret = 0;

	switch (address & 0xff)
	{
		case 0x00:
		{
			for (INT32 i = 0; i < 8; i++) ret |= DrvJoy1[i] << i;	 //AM_READ_PORT("P1")
			return ret;
		}

		case 0x01:
		{
			for (INT32 i = 0; i < 8; i++) ret |= DrvJoy2[i] << i;//AM_READ_PORT("P2")
			return ret;
		}

		case 0x03:
		{
			return DrvDip[0]; // AM_READ_PORT("DSW1") 
		}

		case 0x04:
			for (INT32 i = 0; i < 8; i++) ret |= DrvJoy3[i] << i; // AM_READ_PORT("BUTTON3")
			return ret;
//			return DrvDip[1]; // AM_READ_PORT("BUTTON3")
	}

	return 0;
}

void __fastcall appoooh_out(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("appoooh_out %02d %02d\n"), address & 0xff,data);
	switch (address & 0xff)
	{
		case 0x00:
			SN76496Write(0, data);//AM_DEVWRITE("sn1", sn76496_w)
		break;
		
		case 0x01:
			SN76496Write(1, data);//AM_DEVWRITE("sn2", sn76496_w)
		break;
		
		case 0x02:
			SN76496Write(2, data);//AM_DEVWRITE("sn3", sn76496_w)
		break;

		case 0x03:
			appoooh_adpcm_w(data); // AM_WRITE(appoooh_adpcm_w)
		break;

		case 0x04:
			appoooh_out_w(data); // AM_WRITE(appoooh_out_w)
		break;

		case 0x05:
			scroll_x = data; // AM_WRITE(appoooh_scroll_w) /* unknown */
		break;
	}
}

static void DrawSprites(UINT8 *sprite, UINT8 *gfx, UINT32 offset)
{
	INT32 offs;
	INT32 flipy = 0; //flip_screen_get(gfx->machine);

	for (offs = 0x20 - 4; offs >= 0; offs -= 4)
	{
		INT32 sy    = 240 - sprite[offs + 0];
		INT32 code  = (sprite[offs + 1] >> 2) + ((sprite[offs + 2] >> 5) & 0x07) * 0x40;
		INT32 color = sprite[offs + 2] & 0x0f;    /* TODO: bit 4 toggles continuously, what is it? */
		INT32 sx    = sprite[offs + 3];
		INT32 flipx = sprite[offs + 1] & 0x01;

		if(sx >= 248)
			sx -= 256;

		if (flipy)
		{
			sx = 239 - sx;
			sy = 239 - sy;
			flipx = !flipx;
		}


		if(flipx)
			Render16x16Tile_Mask_FlipX_Clip(pTransDraw, code, sx, sy, color, 3/* 3 bits*/, 0, offset, gfx);
		else
			Render16x16Tile_Mask_Clip(pTransDraw, code, sx, sy, color, 3/* 3 bits*/, 0, offset, gfx);

//		Render16x16Tile_Mask_Clip(pTransDraw, (vbt/16), vbt, 16, color, 3/* 3 bits*/, 0, 0, gfx);
//vbt+=16;
/*                drawgfx_transpen( dest_bmp, cliprect,
                                gfx,
                                code,
                                color,
                                flipx,flipy,
                                sx, sy, 0);	*/
	}
}

static INT32 DrawBgTiles()
{
	INT32 offs;
	INT32 scroll;

	for (offs = 0x3e0 - 1;offs >= 0;offs--)
	{
		/* char set #2 */
		{
			INT32 sx,sy,code,color,flipx;

			sx = offs % 32;
			sy = offs / 32;
			code = DrvBgVidRAM[offs] + 256 * ((DrvBgColRAM[offs]>>5) & 0x07);
			color = DrvBgColRAM[offs] & 0x0f;
			flipx = DrvBgColRAM[offs] & 0x10;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
			}

			if(flipx)
				Render8x8Tile_FlipX_Clip(pTransDraw, code, sx*8, sy*8, color, 3, 0x100, DrvGfxROM1);
			else
				Render8x8Tile_Clip(pTransDraw, code, sx*8, sy*8, color, 3, 0x100, DrvGfxROM1);
		}
	}

	scroll = -scroll_x;
	scroll = 0;

	return 0;
}

static INT32 DrawFgTiles()
{
	INT32 offs;
	INT32 scroll;

	for (offs = 0x3e0 - 1;offs >= 0;offs--)
	{
		/* char set #1 */
		{
			INT32 sx,sy,code,color,flipx;

			sx = offs % 32;
			sy = offs / 32;
			code = DrvFgVidRAM[offs] + 256 * ((DrvFgColRAM[offs]>>5) & 7);
			code &= 0x7ff;
			color = DrvFgColRAM[offs] & 0x0f;
			flipx = DrvFgColRAM[offs] & 0x10;
			if (flipscreen)
			{
				sx = 31 - sx;
				sy = 31 - sy;
				flipx = !flipx;
			}
			if(flipx)
				Render8x8Tile_Mask_FlipX_Clip(pTransDraw, code, sx*8, sy*8, color, 3, 0, 0, DrvGfxROM0);
			else
				Render8x8Tile_Mask_Clip(pTransDraw, code, sx*8, sy*8, color, 3, 0, 0, DrvGfxROM0);
		}
	}

	scroll = -scroll_x;
	scroll = 0;

	return 0;
}



static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	DrawBgTiles();

	if(priority == 0)
		DrawFgTiles();

	/* draw sprites */
	if(priority) {
		/* sprite set #1 */
		DrawSprites(DrvSprRAM0,DrvGfxROM2, 0);
		/* sprite set #2 */
		DrawSprites(DrvSprRAM1,DrvGfxROM3, 0x100);
	}
	else
	{
		/* sprite set #2 */
		DrawSprites(DrvSprRAM1,DrvGfxROM3, 0x100);
		/* sprite set #1 */
		DrawSprites(DrvSprRAM0,DrvGfxROM2, 0);
	}

	if(priority != 0)
		DrawFgTiles();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	DrvZ80Bank0 = 0;
	scroll_x = 0;
	flipscreen = 0;

	ZetOpen(0);
	ZetReset();
	bankswitch(0);
	ZetClose();

	return 0;
}

static INT32 DrvRobowresLoadRoms()
{
	if (BurnLoadRom(DrvMainROM + 0x00000, 0, 1)) return 1; //epr-7540.13d
	if (BurnLoadRom(DrvMainROM + 0x08000, 1, 1)) return 1; //epr-7541.14d
	if (BurnLoadRom(DrvMainROM + 0x14000, 2, 1)) return 1; //epr-7542.15d

	if (BurnLoadRom(DrvGfxTMP0 + 0x00000, 3, 1)) return 1;	// epr-7544.7h
	if (BurnLoadRom(DrvGfxTMP0 + 0x08000, 4, 1)) return 1;	// epr-7545.6h
	if (BurnLoadRom(DrvGfxTMP0 + 0x10000, 5, 1)) return 1;	// epr-7546.5h

	if (BurnLoadRom(DrvGfxTMP1 + 0x00000, 6, 1)) return 1;	// epr-7547.7d
	if (BurnLoadRom(DrvGfxTMP1 + 0x08000, 7, 1)) return 1;	// epr-7548.6d
	if (BurnLoadRom(DrvGfxTMP1 + 0x10000, 8, 1)) return 1;	// epr-7549.5d

	if (BurnLoadRom(DrvColPROM + 0x0000,  9, 1)) return 1;	// pr7571.10a
	if (BurnLoadRom(DrvColPROM + 0x0020, 10, 1)) return 1;	// pr7572.7f
	if (BurnLoadRom(DrvColPROM + 0x0120, 11, 1)) return 1;	// pr7573.7g

	if (BurnLoadRom(DrvSoundROM + 0x0000, 12, 1)) return 1;	// epr-7543.12b

	return 0;
}

static INT32 DrvLoadRoms()
{
	for (INT32 i = 0; i < 5; i++)
		if (BurnLoadRom(DrvMainROM + i * 0x2000, i +  0, 1)) return 1;

 	if (BurnLoadRom(DrvMainROM + 0x0a000,  5, 1)) return 1;	// epr-5911.bin
	if (BurnLoadRom(DrvMainROM + 0x0c000,  6, 1)) return 1;	// epr-5913.bin

	if (BurnLoadRom(DrvMainROM + 0x10000,  7, 1)) return 1;	// epr-5912.bin
	if (BurnLoadRom(DrvMainROM + 0x12000,  8, 1)) return 1;	// epr-5914.bin

	if (BurnLoadRom(DrvGfxTMP0 + 0x00000,  9, 1)) return 1;	// epr-5895.bin
	if (BurnLoadRom(DrvGfxTMP0 + 0x04000, 10, 1)) return 1;	// epr-5896.bin
	if (BurnLoadRom(DrvGfxTMP0 + 0x08000, 11, 1)) return 1;	// epr-5897.bin

	if (BurnLoadRom(DrvGfxTMP1 + 0x00000, 12, 1)) return 1;	// epr-5898.bin
	if (BurnLoadRom(DrvGfxTMP1 + 0x04000, 13, 1)) return 1;	// epr-5899.bin
	if (BurnLoadRom(DrvGfxTMP1 + 0x08000, 14, 1)) return 1;	// epr-5900.bin

	if (BurnLoadRom(DrvColPROM + 0x0000, 15, 1)) return 1;	// pr5921.prm
	if (BurnLoadRom(DrvColPROM + 0x0020, 16, 1)) return 1;	// pr5922.prm
	if (BurnLoadRom(DrvColPROM + 0x0120, 17, 1)) return 1;	// pr5923.prm

	if (BurnLoadRom(DrvSoundROM + 0x0000, 18, 1)) return 1;	// epr-5901.bin
	if (BurnLoadRom(DrvSoundROM + 0x2000, 19, 1)) return 1;	// epr-5902.bin
	if (BurnLoadRom(DrvSoundROM + 0x4000, 20, 1)) return 1;	// epr-5903.bin
	if (BurnLoadRom(DrvSoundROM + 0x6000, 21, 1)) return 1;	// epr-5904.bin
	if (BurnLoadRom(DrvSoundROM + 0x8000, 22, 1)) return 1;	// epr-5905.bin

	return 0;
}
static INT32 DrvRobowresInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();
	if(DrvRobowresLoadRoms()) return 1;
	DrvRobowresPaletteInit();

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
	if(DrvLoadRoms()) return 1;
	DrvPaletteInit();
	DrvGfxDecode();
	nCyclesTotal = 3072000 / 60;

	ZetInit(0);
	ZetOpen(0);
	ZetSetInHandler(appoooh_in);
	ZetSetOutHandler(appoooh_out);
	ZetMapArea(0x0000, 0x7fff, 0, DrvMainROM + 0x0000);	 // AM_RANGE(0x0000, 0x7fff) AM_ROM
	ZetMapArea(0x0000, 0x7fff, 2, DrvMainROM + 0x0000);
	ZetMapArea(0x8000, 0x9fff, 0, DrvMainROM + 0x8000);	// AM_RANGE(0x8000, 0x9fff) AM_ROM
	ZetMapArea(0x8000, 0x9fff, 2, DrvMainROM + 0x8000);

	//ZetMapMemory(DrvMainROM			, 0xe000, 0xe7ff, MAP_RAM);	// AM_RANGE(0xe000, 0xe7ff) AM_RAM

	ZetMapArea(0x0a000, 0xdfff, 0, DrvMainROM + 0x0a000   );
	ZetMapArea(0x0a000, 0xdfff, 2, DrvMainROM + 0x0a000   );

//        AM_RANGE(0xa000, 0xdfff) AM_ROMBANK("bank1")	voir bank_switch
	ZetMapMemory(DrvRAM0			, 0xe000, 0xe7ff, MAP_RAM);	// AM_RANGE(0xe000, 0xe7ff) AM_RAM
	ZetMapMemory(DrvRAM1			, 0xe800, 0xefff, MAP_RAM);		// AM_RANGE(0xe800, 0xefff) AM_RAM // RAM ? 
//	ZetMapMemory(DrvSprRAM0		, 0xf000, 0xf01f, MAP_RAM);		// AM_BASE_MEMBER(appoooh_state, spriteram)
//	ZetMapMemory(DrvFgVidRAM	, 0xf020, 0xf3ff, MAP_RAM);		// AM_RANGE(0xf020, 0xf3ff) AM_WRITE(appoooh_fg_videoram_w) AM_BASE_MEMBER(appoooh_state, fg_videoram)
//	ZetMapMemory(DrvFgColRAM	, 0xf420, 0xf7ff, MAP_RAM);		// AM_WRITE(appoooh_fg_colorram_w) AM_BASE_MEMBER(appoooh_state, fg_colorram)
//	ZetMapMemory(DrvSprRAM1		, 0xf800, 0xf81f, MAP_RAM);		// AM_BASE_MEMBER(appoooh_state, spriteram_2)
//	ZetMapMemory(DrvBgVidRAM	, 0xf820, 0xfbff, MAP_RAM);		// AM_WRITE(appoooh_bg_videoram_w) AM_BASE_MEMBER(appoooh_state, bg_videoram)
//	ZetMapMemory(DrvBgColRAM	, 0xfc20, 0xffff, MAP_RAM);		// AM_WRITE(appoooh_bg_colorram_w) AM_BASE_MEMBER(appoooh_state, bg_colorram)

 	ZetSetWriteHandler(appoooh_write);
	ZetSetReadHandler(appoooh_read);
//	ZetMapMemory(DrvRAM2, 0xf000, 0xffff, MAP_RAM);	//AM_RANGE(0xf000, 0xffff) AM_RAM

	ZetClose();
	
	SN76489Init(0, 18432000 / 6, 0);
	SN76489Init(1, 18432000 / 6, 1);
	SN76489Init(2, 18432000 / 6, 1);
	SN76496SetRoute(0, 0.30, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.30, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(2, 0.30, BURN_SND_ROUTE_BOTH);

	MSM5205Init(0, DrvMSM5205SynchroniseStream, 384000, NULL, MSM5205_S64_4B, 1);
	MSM5205SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();
	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();
	ZetExit();

	SN76496Exit();
	MSM5205Exit();

	BurnFree (AllMem);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	memset (DrvInputs, 0x00, 3); // DrvJoy1 = active low, 2, 3 = active high
	memset (DrvInputs, 0xff, 1); // DrvJoy1 = active low, 2, 3 = active high

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;

	}

	ZetOpen(0);

	ZetRun(nCyclesTotal);

	if (interrupt_enable) ZetNmi();

	MSM5205Update();
	
	if (pBurnSoundOut) 
	{
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(2, pBurnSoundOut, nBurnSoundLen);
		MSM5205Render(0, pBurnSoundOut, nBurnSoundLen);
	}
	ZetClose();

	if (pBurnDraw) {
		DrvDraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd-AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {

		ZetScan(nAction);

		SN76496Scan(nAction, pnMin);

		MSM5205Scan(nAction, pnMin);

		SCAN_VAR(priority);
		SCAN_VAR(interrupt_enable);
		SCAN_VAR(flipscreen);
		SCAN_VAR(DrvZ80Bank0);
		SCAN_VAR(scroll_x);
		
		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			bankswitch(DrvZ80Bank0);
			ZetClose();
		}
	}
	return 0;
}


// Appoooh

static struct BurnRomInfo appooohRomDesc[] = {
	{ "epr-5906.bin",	0x2000, 0xfffae7fe, 1 }, //  0 maincpu
	{ "epr-5907.bin",	0x2000, 0x57696cd6, 1 }, //  1
	{ "epr-5908.bin",	0x2000, 0x4537cddc, 1 }, //  2
	{ "epr-5909.bin",	0x2000, 0xcf82718d, 1 }, //  3
	{ "epr-5910.bin",	0x2000, 0x312636da, 1 }, //  4
	{ "epr-5911.bin",	0x2000, 0x0bc2acaa, 1 }, //  5
	{ "epr-5913.bin",	0x2000, 0xf5a0e6a7, 1 }, //  6
	{ "epr-5912.bin",	0x2000, 0x3c3915ab, 1 }, //  7
	{ "epr-5914.bin",	0x2000, 0x58792d4a, 1 }, //  8

	{ "epr-5895.bin",	0x4000, 0x4b0d4294, 2 }, //  9 gfx1
	{ "epr-5896.bin",	0x4000, 0x7bc84d75, 2 }, // 10
	{ "epr-5897.bin",	0x4000, 0x745f3ffa, 2 }, // 11

	{ "epr-5898.bin",	0x4000, 0xcf01644d, 3 }, // 12 gfx2
	{ "epr-5899.bin",	0x4000, 0x885ad636, 3 }, // 13
	{ "epr-5900.bin",	0x4000, 0xa8ed13f3, 3 }, // 14

	{ "pr5921.prm",		0x0020, 0xf2437229, 4 }, // 15 proms
	{ "pr5922.prm",		0x0100, 0x85c542bf, 4 }, // 16
	{ "pr5923.prm",		0x0100, 0x16acbd53, 4 }, // 17

	{ "epr-5901.bin",	0x2000, 0x170a10a4, 5 }, // 18 adpcm
	{ "epr-5902.bin",	0x2000, 0xf6981640, 5 }, // 19
	{ "epr-5903.bin",	0x2000, 0x0439df50, 5 }, // 20
	{ "epr-5904.bin",	0x2000, 0x9988f2ae, 5 }, // 21
	{ "epr-5905.bin",	0x2000, 0xfb5cd70e, 5 }, // 22
};

STD_ROM_PICK(appoooh)
STD_ROM_FN(appoooh)


static struct BurnRomInfo robowresRomDesc[] = {
	{ "epr-7540.13d",	0x8000, 0xa2a54237, 1 }, //  0 maincpu
	{ "epr-7541.14d",	0x8000, 0xcbf7d1a8, 1 }, //  1
	{ "epr-7542.15d",	0x8000, 0x3475fbd4, 1 }, //  2

	{ "epr-7544.7h",	0x8000, 0x07b846ce, 2 }, //  3 gfx1
	{ "epr-7545.6h",	0x8000, 0xe99897be, 2 }, // 4
	{ "epr-7546.5h",	0x8000, 0x1559235a, 2 }, // 5

	{ "epr-7547.7d",	0x8000, 0xb87ad4a4, 3 }, // 6 gfx2
	{ "epr-7548.6d",	0x8000, 0x8b9c75b3, 3 }, // 7
	{ "epr-7549.5d",	0x8000, 0xf640afbb, 3 }, // 8

	{ "pr7571.10a",	0x0020, 0xe82c6d5c, 4 }, // 9 proms
	{ "pr7572.7f",		0x0100, 0x2b083d0c, 4 }, // 10
	{ "pr7573.7g",	0x0100, 0x2b083d0c, 4 }, // 11

	{ "epr-7543.12b",	0x8000, 0x4d108c49, 5 }, // 18 adpcm
};

STD_ROM_PICK(robowres)
STD_ROM_FN(robowres)

struct BurnDriver BurnDrvAppoooh = {
	"appoooh", NULL, NULL, NULL, "1984",
	"Appoooh\0", NULL, "Sanritsu / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, appooohRomInfo, appooohRomName, NULL, NULL, AppooohInputInfo, AppooohDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 512,
	256, 224, 4, 3
};

struct BurnDriver BurnDrvRobowres = {
	"robowres", NULL, NULL, NULL, "1986",
	"Robo Wres 2001\0", NULL, "Sanritsu / Sega", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, robowresRomInfo, robowresRomName, NULL, NULL, AppooohInputInfo, RobowresDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 512,
	256, 224, 4, 3
};
