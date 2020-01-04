// FB Alpha Gauntlet driver module
// Based on MAME driver by Aaron Giles

// to do:
//	play test hard!! verify

#include "tiles_generic.h"
#include "m68000_intf.h"
#include "m6502_intf.h"
#include "burn_ym2151.h"
#include "atariic.h"
#include "atarimo.h"
#include "slapstic.h"
#include "pokey.h"
#include "tms5220.h"
#include "watchdog.h"

static UINT8 *Mem                  = NULL;
static UINT8 *MemEnd               = NULL;
static UINT8 *RamStart             = NULL;
static UINT8 *RamEnd               = NULL;
static UINT8 *Drv68KRom            = NULL;
static UINT8 *Drv68KRam            = NULL;
static UINT8 *DrvM6502Rom          = NULL;
static UINT8 *DrvM6502Ram          = NULL;
static UINT8 *DrvPlayfieldRam      = NULL;
static UINT8 *DrvMOSpriteRam       = NULL;
static UINT8 *DrvAlphaRam          = NULL;
static UINT8 *DrvMOSlipRam         = NULL;
static UINT8 *DrvPaletteRam        = NULL;
static UINT8 *DrvChars             = NULL;
static UINT8 *DrvMotionObjectTiles = NULL;
static UINT32 *DrvPalette          = NULL;

static UINT8 DrvVBlank;
static UINT16 DrvSoundResetVal;
static UINT8 DrvSoundCPUHalt;
static UINT8 DrvCPUtoSoundReady;
static UINT8 DrvSoundtoCPUReady;
static UINT8 DrvCPUtoSound;
static UINT8 DrvSoundtoCPU;
static INT16 DrvScrollX;
static INT16 DrvScrollY[262];
static UINT8 DrvTileBank;

static INT32 nCyclesDone[2], nCyclesTotal[2];
static INT32 nCyclesSegment;

static UINT8 speech_val;
static UINT8 last_speech_write;

static UINT8 DrvInputPort0[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInputPort1[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInputPort2[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInputPort3[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInputPort5[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static UINT8 DrvInput[6];
static UINT8 DrvDip[1];
static UINT8 DrvReset;

#define GAME_GAUNTLET		0
#define GAME_GAUNTLET2		1
#define GAME_VINDCTR2		2

static UINT8 DrvGameType;

static struct BurnInputInfo GauntletInputList[] =
{
	{"Coin 1", 				BIT_DIGITAL  , DrvInputPort5 + 3, 	"p1 coin"   },
	{"Coin 2", 				BIT_DIGITAL  , DrvInputPort5 + 2, 	"p2 coin"   },
	{"Coin 3", 				BIT_DIGITAL  , DrvInputPort5 + 1, 	"p3 coin"   },
	{"Coin 4", 				BIT_DIGITAL  , DrvInputPort5 + 0, 	"p4 coin"   },

	{"P1 Up", 				BIT_DIGITAL  , DrvInputPort0 + 7, 	"p1 up"     },
	{"P1 Down", 			BIT_DIGITAL  , DrvInputPort0 + 6, 	"p1 down"   },
	{"P1 Left", 			BIT_DIGITAL  , DrvInputPort0 + 5, 	"p1 left"   },
	{"P1 Right", 			BIT_DIGITAL  , DrvInputPort0 + 4, 	"p1 right"  },
	{"P1 Fire 1", 			BIT_DIGITAL  , DrvInputPort0 + 1, 	"p1 fire 1" },
	{"P1 Fire 2", 			BIT_DIGITAL  , DrvInputPort0 + 0, 	"p1 fire 2" },
	
	{"P2 Up", 				BIT_DIGITAL  , DrvInputPort1 + 7, 	"p2 up"     },
	{"P2 Down", 			BIT_DIGITAL  , DrvInputPort1 + 6, 	"p2 down"   },
	{"P2 Left", 			BIT_DIGITAL  , DrvInputPort1 + 5, 	"p2 left"   },
	{"P2 Right", 			BIT_DIGITAL  , DrvInputPort1 + 4, 	"p2 right"  },
	{"P2 Fire 1", 			BIT_DIGITAL  , DrvInputPort1 + 1, 	"p2 fire 1" },
	{"P2 Fire 2", 			BIT_DIGITAL  , DrvInputPort1 + 0, 	"p2 fire 2" },
	
	{"P3 Up", 				BIT_DIGITAL  , DrvInputPort2 + 7,	"p3 up"     },
	{"P3 Down", 			BIT_DIGITAL  , DrvInputPort2 + 6,	"p3 down"   },
	{"P3 Left", 			BIT_DIGITAL  , DrvInputPort2 + 5,	"p3 left"   },
	{"P3 Right", 			BIT_DIGITAL  , DrvInputPort2 + 4,	"p3 right"  },
	{"P3 Fire 1" , 			BIT_DIGITAL  , DrvInputPort2 + 1,	"p3 fire 1" },
	{"P3 Fire 2", 			BIT_DIGITAL  , DrvInputPort2 + 0,	"p3 fire 2" },
	
	{"P4 Up", 				BIT_DIGITAL  , DrvInputPort3 + 7,	"p4 up"     },
	{"P4 Down", 			BIT_DIGITAL  , DrvInputPort3 + 6,	"p4 down"   },
	{"P4 Left", 			BIT_DIGITAL  , DrvInputPort3 + 5,	"p4 left"   },
	{"P4 Right", 			BIT_DIGITAL  , DrvInputPort3 + 4,	"p4 right"  },
	{"P4 Fire 1", 			BIT_DIGITAL  , DrvInputPort3 + 1,	"p4 fire 1" },
	{"P4 Fire 2" ,			BIT_DIGITAL  , DrvInputPort3 + 0,	"p4 fire 2" },

	{"Reset", 				BIT_DIGITAL  , &DrvReset,			"reset"     },
	{"Dip 1", 				BIT_DIPSWITCH, DrvDip + 0,			"dip"       },
//	{"Diagnostics", 		BIT_DIGITAL  , DrvSrv + 0,			"diag"      },
};

STDINPUTINFO(Gauntlet)

static struct BurnInputInfo Vindctr2InputList[] = {
	{"Coin 1",				BIT_DIGITAL,	DrvInputPort5 + 3,	"p1 coin"	},
	{"Coin 1",				BIT_DIGITAL,	DrvInputPort5 + 2,	"p2 coin"	},
	{"Coin 2",				BIT_DIGITAL,	DrvInputPort5 + 1,	"p3 coin"	},
	{"Coin 3",				BIT_DIGITAL,	DrvInputPort5 + 0,	"p4 coin"	},

	{"P1 Start",			BIT_DIGITAL,	DrvInputPort2 + 0,	"p1 start"	},
	{"P1 Left Stick Up",	BIT_DIGITAL,	DrvInputPort0 + 4,	"p1 up"		},
	{"P1 Left Stick Down",	BIT_DIGITAL,	DrvInputPort0 + 6,	"p1 down"	},
	{"P1 Right Stick Up",	BIT_DIGITAL,	DrvInputPort0 + 5,	"p3 up"		},
	{"P1 Right Stick Down",	BIT_DIGITAL,	DrvInputPort0 + 7,	"p3 down"	},
	{"P1 Left Stick Fire",	BIT_DIGITAL,	DrvInputPort0 + 0,	"p1 fire 1"	},
	{"P1 Left Stick Thumb",	BIT_DIGITAL,	DrvInputPort0 + 2,	"p1 fire 2"	},
	{"P1 Right Stick Fire",	BIT_DIGITAL,	DrvInputPort0 + 1,	"p1 fire 3"	},
	{"P1 Right Stick Thumb",BIT_DIGITAL,	DrvInputPort0 + 3,	"p1 fire 4"	},

	{"P2 Start",			BIT_DIGITAL,	DrvInputPort2 + 1,	"p2 start"	},
	{"P2 Left Stick Up",	BIT_DIGITAL,	DrvInputPort1 + 4,	"p2 up"		},
	{"P2 Left Stick Down",	BIT_DIGITAL,	DrvInputPort1 + 6,	"p2 down"	},
	{"P2 Right Stick Up",	BIT_DIGITAL,	DrvInputPort1 + 5,	"p4 up"		},
	{"P2 Right Stick Down",	BIT_DIGITAL,	DrvInputPort1 + 7,	"p4 down"	},
	{"P2 Left Stick Fire",	BIT_DIGITAL,	DrvInputPort1 + 0,	"p2 fire 1"	},
	{"P2 Left Stick Thumb",	BIT_DIGITAL,	DrvInputPort1 + 2,	"p2 fire 2"	},
	{"P2 Right Stick Fire",	BIT_DIGITAL,	DrvInputPort1 + 1,	"p2 fire 3"	},
	{"P2 Right Stick Thumb",BIT_DIGITAL,	DrvInputPort1 + 3,	"p2 fire 4"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,			"reset"		},
	{"Dip 1",				BIT_DIPSWITCH,	DrvDip + 0,			"dip"		},
};

STDINPUTINFO(Vindctr2)

static struct BurnDIPInfo DrvDIPList[]=
{
	{0x1d, 0xff, 0xff, 0x08, NULL				},

	{0   , 0xfe, 0   , 2   , "Service Mode"		},
	{0x1d, 0x01, 0x08, 0x08, "Off"				},
	{0x1d, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Drv)

static struct BurnDIPInfo Vindctr2DIPList[]=
{
	{0x17, 0xff, 0xff, 0x08, NULL				},

	{0   , 0xfe, 0   , 2   , "Service Mode"		},
	{0x17, 0x01, 0x08, 0x08, "Off"				},
	{0x17, 0x01, 0x08, 0x00, "On"				},
};

STDDIPINFO(Vindctr2)

static inline void soundcpuSync()
{
	if (!DrvSoundCPUHalt) {
		INT32 nCycles = (SekTotalCycles() / 4) - nCyclesDone[1];
		if (nCycles > 0) nCyclesDone[1] += M6502Run(nCycles);
	} else {
		nCyclesDone[1] = SekTotalCycles() / 4;
	}
}

static void __fastcall Gauntlet68KWriteWord(UINT32 a, UINT16 d)
{
	if ((a & 0xffe000) == 0x902000) {
		*((UINT16*)(DrvMOSpriteRam + (a & 0x1ffe))) = d;
		AtariMoWrite(0, (a / 2) & 0xfff, d);
		return;
	}

	switch (a)
	{
		case 0x803100:
			BurnWatchdogWrite();
		return;
		
		case 0x803120:
		case 0x80312e: {
			INT32 OldVal = DrvSoundResetVal;
			DrvSoundResetVal = d;
			if ((OldVal ^ DrvSoundResetVal) & 1) {
				if (DrvSoundResetVal & 1) {
    				M6502Open(0);
					M6502Reset();
					DrvSoundtoCPUReady = 0;
					M6502Run(10); // why's this needed? who knows...
					M6502Close();
					DrvSoundCPUHalt = 0;
				} else {
					DrvSoundCPUHalt = 1;
				}
 			}
			
			return;
		}
		
		case 0x803140:
			SekSetIRQLine(4, CPU_IRQSTATUS_NONE);
		return;
		
		case 0x803150:
			AtariEEPROMUnlockWrite();
		return;
		
		case 0x803170: {
			DrvCPUtoSound = d & 0xff;
			M6502Open(0);
			soundcpuSync();
			DrvCPUtoSoundReady = 1;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_ACK);
			M6502Close();
			return;
		}
		
		case 0x930000:
			DrvScrollX = d & 0x1ff;
		return;
	}

	bprintf (0, _T("WB %5.5x, %4.4x\n"), a, d);
}

static void __fastcall Gauntlet68KWriteByte(UINT32 a, UINT8 d)
{
	if ((a & 0xffe000) == 0x902000) {
		DrvMOSpriteRam[(a & 0x1fff) ^ 1] = d;
		AtariMoWrite(0, (a / 2) & 0xfff, *((UINT16*)(DrvMOSpriteRam + (a & 0x1ffe))));
		return;
	}

	bprintf (0, _T("WB %5.5x, %2.2x\n"), a, d);
}

static UINT16 __fastcall Gauntlet68KReadWord(UINT32 a)
{
	switch (a)
	{
		case 0x803000:
			return DrvInput[0];

		case 0x803002:
			return DrvInput[1];

		case 0x803004:
			return DrvInput[2];

		case 0x803006:
			return DrvInput[3];

		case 0x803008: {
			UINT8 Res = (DrvInput[4] | (DrvVBlank ? 0x00 : 0x40)) & ~0x30;
			if (DrvCPUtoSoundReady) Res ^= 0x20;
			if (DrvSoundtoCPUReady) Res ^= 0x10;
			return 0x0000 | Res;
		}
		
		case 0x80300e: {
			DrvSoundtoCPUReady = 0;
			SekSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return 0xff00 | DrvSoundtoCPU;
		}
	}

	return 0;
}

static UINT8 __fastcall Gauntlet68KReadByte(UINT32 a)
{	
	switch (a)
	{
		case 0x803000:
			return DrvInput[0] >> 8;

		case 0x803001:
			return DrvInput[0];

		case 0x803002:
			return DrvInput[1] >> 8;

		case 0x803003:
			return DrvInput[1];
		
		case 0x803004:
			return DrvInput[2] >> 8;
		
		case 0x803005:
			return DrvInput[2];

		case 0x803006:
			return DrvInput[3] >> 8;

		case 0x803007:
			return DrvInput[3];

		case 0x803009: {
			UINT8 Res = (DrvInput[4] | (DrvVBlank ? 0x00 : 0x40)) & ~0x30;
			if (DrvCPUtoSoundReady) Res ^= 0x20;
			if (DrvSoundtoCPUReady) Res ^= 0x10;
			return Res;
		}
		
		case 0x80300f: {
			DrvSoundtoCPUReady = 0;
			SekSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return DrvSoundtoCPU;
		}
	}
	
	return 0;
}

static UINT8 GauntletSoundRead(UINT16 Address)
{
	if ((Address & 0xd830) == 0x1800) {
		return pokey1_r(Address & 0xf);
	}

	switch (Address)
	{
		case 0x1010: {
			DrvCPUtoSoundReady = 0;
			M6502SetIRQLine(M6502_INPUT_LINE_NMI, CPU_IRQSTATUS_NONE);
			return DrvCPUtoSound;
		}
		
		case 0x1020:
			return DrvInput[5];
		
		case 0x1030:
		case 0x1031: {
			UINT8 Res = 0x30;
			if (DrvCPUtoSoundReady) Res ^= 0x80;
			if (DrvSoundtoCPUReady) Res ^= 0x40;
			if (tms5220_ready()) Res ^= 0x20; // tms5220 ready status
			if (!(DrvDip[0] & 0x08)) Res ^= 0x10;
			return Res;
		}
		
		case 0x1811:
			return BurnYM2151Read();
	}

	return 0;
}

static void GauntletSoundWrite(UINT16 Address, UINT8 Data)
{
	if ((Address & 0xd830) == 0x1800) {
		pokey1_w(Address & 0xf, Data);
		return;
	}

	switch (Address & ~0xf)
	{
		case 0x1000: // 1000-100f
		{
			DrvSoundtoCPU = Data;
			DrvSoundtoCPUReady = 1;
			if (SekGetActive() == -1) {
				SekOpen(0);
				SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
				SekClose();
			} else {
				SekSetIRQLine(6, CPU_IRQSTATUS_ACK);
			}
			return;
		}

		case 0x1020:
		return; // sound mixer

		case 0x1030: // 1030-103f
		{
			switch (Address & 7)
			{
				case 0:
					if (~Data&0x80) BurnYM2151Reset();
					break;

				case 1:	// speech write, bit D7, active low
					if (((Data ^ last_speech_write) & 0x80) && (Data & 0x80))
						tms5220_write(speech_val);
					last_speech_write = Data;
					break;

				case 2:	// speech reset, bit D7, active low
					if (((Data ^ last_speech_write) & 0x80) && (Data & 0x80))
						tms5220_reset();
					break;

				case 3:	// speech squeak, bit D7
					Data = 5 | ((Data >> 6) & 2);
					tms5220_set_frequency(14318180/2 / (16 - Data));
					break;
			}
			return;
		}
		
		case 0x1810: // 1810 & 1811
			BurnYM2151Write(Address & 1, Data);
		return;
		
		case 0x1820: // 1820-182f
			speech_val = Data;
		return;
		
		case 0x1830:
			M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_NONE);
		return;
	}
}

static tilemap_callback( tx )
{
	UINT16 data = *((UINT16*)(DrvAlphaRam + (offs * 2)));

	INT32 color = ((data >> 10) & 0xf) | ((data >> 9) & 0x20);

	TILE_SET_INFO(0, data, color, (data >> 15) ? TILE_OPAQUE : 0);
}

static tilemap_callback( bg )
{
	UINT16 data = *((UINT16*)(DrvPlayfieldRam + (offs * 2)));

	INT32 code = (DrvTileBank * 0x1000) + (data & 0xfff);

	TILE_SET_INFO(2, code, data >> 12, TILE_FLIPYX(data >> 15));
}

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(RamStart, 0, RamEnd - RamStart);
	}

	SekOpen(0);
	SekReset();
	SekClose();
	
	M6502Open(0);
	M6502Reset();
	M6502Close();

	BurnWatchdogReset();

	BurnYM2151Reset();
	tms5220_reset();
	tms5220_set_frequency(14318180/2/11);

	AtariSlapsticReset();
	AtariEEPROMReset();

	DrvSoundResetVal = 1;
	DrvSoundCPUHalt = 1;
	DrvCPUtoSoundReady = 0;
	DrvSoundtoCPUReady = 0;
	DrvCPUtoSound = 0;
	DrvSoundtoCPU = 0;
	last_speech_write = 0x80;
	speech_val = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = Mem;

	Drv68KRom              = Next; Next += 0x080000;
	DrvM6502Rom            = Next; Next += 0x010000;

	DrvChars               = Next; Next += 0x0100000;
	DrvMotionObjectTiles   = Next; Next += 0x1800000;

	DrvPalette             = (UINT32*)Next; Next += 0x00400 * sizeof(UINT32);

	RamStart               = Next;

	Drv68KRam              = Next; Next += 0x003000;
	DrvM6502Ram            = Next; Next += 0x001000;
	DrvPlayfieldRam        = Next; Next += 0x002000;
	DrvMOSpriteRam         = Next; Next += 0x002000;
	DrvAlphaRam            = Next; Next += 0x000f80;
	atarimo_0_slipram	   = (UINT16*)Next;
	DrvMOSlipRam           = Next; Next += 0x000080;
	DrvPaletteRam          = Next; Next += 0x000800;
	
	RamEnd                 = Next;

	MemEnd                 = Next;

	return 0;
}

static void atarigen_swap_mem(void *ptr1, void *ptr2, INT32 bytes)
{
	UINT8 *p1 = (UINT8 *)ptr1;
	UINT8 *p2 = (UINT8 *)ptr2;

	while (bytes--) {
		INT32 temp = *p1;
		*p1++ = *p2;
		*p2++ = temp;
	}
}

static void DrvGfxDecode(INT32 mosize)
{
	INT32 CharPlaneOffsets[2]     = { 0, 4 };
	INT32 CharXOffsets[8]         = { STEP4(0,1), STEP4(8,1) };
	INT32 CharYOffsets[8]         = { STEP8(0,16) };
	INT32 MOPlaneOffsets[4]       = { 3*8*(mosize/4), 2*8*(mosize/4), 1*8*(mosize/4), 0*8*(mosize/4) };
	INT32 MOXOffsets[8]           = { STEP8(0,1) };
	INT32 MOYOffsets[8]           = { STEP8(0,8) };

	UINT8 *tmp = (UINT8 *)BurnMalloc(0xc0000);

	memcpy (tmp, DrvChars, 0x4000);

	GfxDecode(0x0400, 2, 8, 8, CharPlaneOffsets, CharXOffsets, CharYOffsets, 0x80, tmp, DrvChars);

	for (UINT32 i = 0; i < 0xc0000; i++) tmp[i] = DrvMotionObjectTiles[i] ^ 0xff; // copy & invert

	GfxDecode((mosize * 2) / 0x40, 4, 8, 8, MOPlaneOffsets, MOXOffsets, MOYOffsets, 0x40, tmp, DrvMotionObjectTiles);

	BurnFree (tmp);

	{
		tmp = BurnMalloc(0x180000);

		for (INT32 i = 0; i < 0x180000; i++)
		{
			tmp[i] = DrvMotionObjectTiles[i ^ (0x800*0x40)];
		}

		memcpy (DrvMotionObjectTiles, tmp, 0x180000);

		BurnFree (tmp);
	}
}

static INT32 CommonInit(INT32 game_select, INT32 slapstic_num)
{
	static const struct atarimo_desc modesc =
	{
		1,					// index to which gfx system
		1,					// number of motion object banks
		1,					// are the entries linked?
		1,					// are the entries split?
		0,					// render in reverse order?
		0,					// render in swapped X/Y order?
		0,					// does the neighbor bit affect the next object?
		8,					// pixels per SLIP entry (0 for no-slip)
		1,					// pixel offset for SLIPs
		0,					// maximum number of links to visit/scanline (0=all)

		0x100,				// base palette entry
		0x100,				// maximum number of colors
		0,					// transparent pen index

		{{ 0,0,0,0x03ff }},	// mask for the link
		{{ 0 }},			// mask for the graphics bank
		{{ 0x7fff,0,0,0 }},	// mask for the code index
		{{ 0 }},			// mask for the upper code index
		{{ 0,0x000f,0,0 }},	// mask for the color
		{{ 0,0xff80,0,0 }},	// mask for the X position
		{{ 0,0,0xff80,0 }},	// mask for the Y position
		{{ 0,0,0x0038,0 }},	// mask for the width, in tiles*/
		{{ 0,0,0x0007,0 }},	// mask for the height, in tiles
		{{ 0,0,0x0040,0 }},	// mask for the horizontal flip
		{{ 0 }},			// mask for the vertical flip
		{{ 0 }},			// mask for the priority
		{{ 0 }},			// mask for the neighbor
		{{ 0 }},			// mask for absolute coordinates

		{{ 0 }},			// mask for the special value
		0,					// resulting value to indicate "special"
		0					// callback routine for special entries
	};

	Mem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((Mem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(Mem, 0, nLen);
	MemIndex();

	if (game_select == GAME_GAUNTLET)
	{
		if (BurnLoadRom(Drv68KRom + 0x00001, 0, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x00000, 1, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x38001, 2, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x38000, 3, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x40001, 4, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x40000, 5, 2)) return 1;

		if (BurnLoadRom(DrvM6502Rom + 0x0000, 6, 1)) return 1;
		if (BurnLoadRom(DrvM6502Rom + 0x4000, 7, 1)) return 1;

		if (BurnLoadRom(DrvChars, 8, 1)) return 1;

		if (BurnLoadRom(DrvMotionObjectTiles + 0x00000,  9, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x08000, 10, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x10000, 11, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x18000, 12, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x20000, 13, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x28000, 14, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x30000, 15, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x38000, 16, 1)) return 1;

		DrvGfxDecode(0x40000);
	}
	else if (game_select == GAME_GAUNTLET2)
	{
		if (BurnLoadRom(Drv68KRom + 0x00001, 0, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x00000, 1, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x38001, 2, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x38000, 3, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x40001, 4, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x40000, 5, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x50001, 6, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x50000, 7, 2)) return 1;

		if (BurnLoadRom(DrvM6502Rom + 0x0000, 8, 1)) return 1;
		if (BurnLoadRom(DrvM6502Rom + 0x4000, 9, 1)) return 1;

		if (BurnLoadRom(DrvChars, 10, 1)) return 1;

		if (BurnLoadRom(DrvMotionObjectTiles + 0x00000, 11, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x08000, 12, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x10000, 13, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x14000, 13, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x18000, 14, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x20000, 15, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x28000, 16, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x2c000, 16, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x30000, 17, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x38000, 18, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x40000, 19, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x44000, 19, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x48000, 20, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x50000, 21, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x58000, 22, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x5c000, 22, 1)) return 1;

		DrvGfxDecode(0x60000);
	}
	else if (game_select == GAME_VINDCTR2)
	{
		if (BurnLoadRom(Drv68KRom + 0x00001,  0, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x00000,  1, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x38001,  2, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x38000,  3, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x40001,  4, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x40000,  5, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x50001,  6, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x50000,  7, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x60001,  8, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x60000,  9, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x70001, 10, 2)) return 1;
		if (BurnLoadRom(Drv68KRom + 0x70000, 11, 2)) return 1;

		if (BurnLoadRom(DrvM6502Rom + 0x0000, 12, 1)) return 1;
		if (BurnLoadRom(DrvM6502Rom + 0x4000, 13, 1)) return 1;

		if (BurnLoadRom(DrvChars, 14, 1)) return 1;

		if (BurnLoadRom(DrvMotionObjectTiles + 0x00000, 15, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x08000, 16, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x10000, 17, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x18000, 18, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x20000, 19, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x28000, 20, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x30000, 21, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x38000, 22, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x40000, 23, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x48000, 24, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x50000, 25, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x58000, 26, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x60000, 27, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x68000, 28, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x70000, 29, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x78000, 30, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x80000, 31, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x88000, 32, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x90000, 33, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0x98000, 34, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0xa0000, 35, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0xa8000, 36, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0xb0000, 37, 1)) return 1;
		if (BurnLoadRom(DrvMotionObjectTiles + 0xb8000, 38, 1)) return 1;

		// rom located at 88000 has scrambling
		{
			memcpy (DrvMotionObjectTiles + 0xc0000, DrvMotionObjectTiles + 0x88000, 0x8000);

			for (INT32 i = 0; i < 0x8000; i++)
			{
				INT32 srcoffs = (i & 0x4000) | ((i << 11) & 0x3800) | ((i >> 3) & 0x07ff);
				DrvMotionObjectTiles[0x88000 + i] = DrvMotionObjectTiles[0xc0000 + srcoffs];
			}
		}

		DrvGfxDecode(0xc0000);
	}

	atarigen_swap_mem(Drv68KRom + 0x000000, Drv68KRom + 0x008000, 0x8000);
	atarigen_swap_mem(Drv68KRom + 0x040000, Drv68KRom + 0x048000, 0x8000);
	atarigen_swap_mem(Drv68KRom + 0x050000, Drv68KRom + 0x058000, 0x8000);
	atarigen_swap_mem(Drv68KRom + 0x060000, Drv68KRom + 0x068000, 0x8000);
	atarigen_swap_mem(Drv68KRom + 0x070000, Drv68KRom + 0x078000, 0x8000);

	SekInit(0, 0x68010);
	SekOpen(0);
	SekMapMemory(Drv68KRom, 			0x000000, 0x037fff, MAP_ROM);
	SekMapMemory(Drv68KRom + 0x40000, 	0x040000, 0x07ffff, MAP_ROM);
	SekMapMemory(Drv68KRam + 0x0000, 	0x800000, 0x801fff, MAP_RAM);
	SekMapMemory(DrvPlayfieldRam, 		0x900000, 0x901fff, MAP_RAM);
	SekMapMemory(DrvMOSpriteRam, 		0x902000, 0x903fff, MAP_ROM);
	SekMapMemory(Drv68KRam + 0x2000, 	0x904000, 0x904fff, MAP_RAM);
	SekMapMemory(DrvAlphaRam, 			0x905000, 0x905f7f, MAP_RAM);
	SekMapMemory(DrvMOSlipRam, 			0x905f80, 0x905fff, MAP_RAM);
	SekMapMemory(DrvPaletteRam, 		0x910000, 0x9107ff, MAP_RAM);
	SekMapMemory(DrvPlayfieldRam, 		0x920000, 0x921fff, MAP_RAM);
	SekSetReadByteHandler(0, 			Gauntlet68KReadByte);
	SekSetWriteByteHandler(0, 			Gauntlet68KWriteByte);
	SekSetReadWordHandler(0, 			Gauntlet68KReadWord);
	SekSetWriteWordHandler(0, 			Gauntlet68KWriteWord);

	AtariEEPROMInit(0x1000);
	AtariEEPROMInstallMap(1, 			0x802000, 0x802fff);

	AtariSlapsticInit(Drv68KRom + 0x038000, slapstic_num);
	AtariSlapsticInstallMap(2, 0x038000);

	SekClose();

	BurnWatchdogInit(DrvDoReset, 180);
	
	M6502Init(0, TYPE_M6502);
	M6502Open(0);
	M6502MapMemory(DrvM6502Ram, 		0x0000, 0x0fff, MAP_RAM);
	M6502MapMemory(DrvM6502Rom, 		0x4000, 0xffff, MAP_ROM);
	M6502SetReadHandler(GauntletSoundRead);
	M6502SetWriteHandler(GauntletSoundWrite);
	M6502Close();

	BurnYM2151Init(14318180 / 4);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_1, 0.48, BURN_SND_ROUTE_RIGHT);
	BurnYM2151SetRoute(BURN_SND_YM2151_YM2151_ROUTE_2, 0.48, BURN_SND_ROUTE_LEFT);

	PokeyInit(14000000/8, 2, 1.00, 1);

	tms5220_init();

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_COLS, bg_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(1, TILEMAP_SCAN_ROWS, tx_map_callback, 8, 8, 64, 32);
	GenericTilemapSetTransparent(1, 0);
	GenericTilemapSetGfx(0, DrvChars, 2, 8, 8, 0x10000, 0x000, 0x3f);

	switch (game_select)
	{
		case GAME_GAUNTLET2:
			GenericTilemapSetGfx(1, DrvMotionObjectTiles, 4, 8, 8, 0x0c0000, 0x100, 0x1f);
			GenericTilemapSetGfx(2, DrvMotionObjectTiles, 4, 8, 8, 0x0c0000, 0x280, 0x07);
		break;

		case GAME_VINDCTR2:
			GenericTilemapSetGfx(1, DrvMotionObjectTiles, 4, 8, 8, 0x180000, 0x100, 0x1f);
			GenericTilemapSetGfx(2, DrvMotionObjectTiles, 4, 8, 8, 0x180000, 0x200, 0x07);
		break;

		default:
		case GAME_GAUNTLET:
			GenericTilemapSetGfx(1, DrvMotionObjectTiles, 4, 8, 8, 0x080000, 0x100, 0x1f);
			GenericTilemapSetGfx(2, DrvMotionObjectTiles, 4, 8, 8, 0x180000, 0x280, 0x07);
		break;
	}

	AtariMoInit(0, &modesc);

	DrvGameType = game_select;

	DrvDoReset(1);

	return 0;
}

static INT32 DrvInit()
{
	return CommonInit(GAME_GAUNTLET,  104);
}

static INT32 Gaunt2pInit()
{
	return CommonInit(GAME_GAUNTLET,  107);
}

static INT32 Gaunt2Init()
{
	return CommonInit(GAME_GAUNTLET2, 106);
}

static INT32 Vindctr2Init()
{
	return CommonInit(GAME_VINDCTR2,  118);
}

static INT32 DrvExit()
{
	SekExit();
	M6502Exit();
	
	BurnYM2151Exit();
	tms5220_exit();

	PokeyExit();
	
	GenericTilesExit();
	
	AtariEEPROMExit();
	AtariSlapsticExit();
	AtariMoExit();

	BurnFree(Mem);

	DrvVBlank = 0;
	DrvSoundResetVal = 0;
	DrvSoundCPUHalt = 0;
	DrvGameType = 0;

	return 0;
}

static void copy_sprites()
{
	INT32 minx, maxx, miny, maxy;
	GenericTilesGetClip(&minx, &maxx, &miny, &maxy);

	INT32 prio1 = (DrvGameType != GAME_VINDCTR2);

	for (INT32 y = miny; y < maxy; y++)
	{
		UINT16 *mo = BurnBitmapGetPosition(31, 0, y);
		UINT16 *pf = BurnBitmapGetPosition(0, 0, y);

		for (INT32 x = minx; x < maxx; x++)
		{
			if (mo[x] != 0xffff)
			{
				if ((mo[x] & 0x0f) == 1)
				{
					if (prio1 || (mo[x] & 0xf0) != 0)
						pf[x] ^= 0x80;
				}
				else
					pf[x] = mo[x] & 0x3ff;

				mo[x] = 0xffff; // clear buffer
			}
		}
	}
}

static INT32 DrvDraw()
{
	AtariPaletteUpdate4IRGB(DrvPaletteRam, DrvPalette, 0x800);

	AtariMoRender(0);

	{
//		BurnTransferClear();

		GenericTilemapSetScrollX(0, DrvScrollX);
		atarimo_set_xscroll(0, DrvScrollX);

		for (INT32 i = 0; i < nScreenHeight; i++)
		{
			GenericTilesSetClip(-1, -1, i, i+1);

			DrvTileBank = DrvScrollY[i] & 3;

			GenericTilemapSetScrollY(0, DrvScrollY[i] >> 7);
			atarimo_set_yscroll(0, DrvScrollY[i] >> 7);
			GenericTilemapDraw(0, pTransDraw, 0);
			GenericTilesClearClip();
		}
	}

	copy_sprites();

	GenericTilemapDraw(1, pTransDraw, 0);
	
	BurnTransferCopy(DrvPalette);

	return 0;
}

static inline void DrvClearOpposites(UINT8* nJoystickInputs)
{
	if (DrvGameType == GAME_VINDCTR2)
	{
		if ((*nJoystickInputs & 0x50) == 0x00) {
			*nJoystickInputs |= 0x50;
		}	
		if ((*nJoystickInputs & 0xa0) == 0x00) {
			*nJoystickInputs |= 0xa0;
		}
	}
	else
	{
		if ((*nJoystickInputs & 0x30) == 0x00) {
			*nJoystickInputs |= 0x30;
		}	
		if ((*nJoystickInputs & 0xc0) == 0x00) {
			*nJoystickInputs |= 0xc0;
		}
	}
}

static INT32 DrvFrame()
{
	BurnWatchdogUpdate();

	if (DrvReset) {
		DrvDoReset(1);
	}

	SekNewFrame();

	{
		DrvInput[0] = 0xff;
		DrvInput[1] = 0xff;
		DrvInput[2] = 0xff;
		DrvInput[3] = 0xff;
		DrvInput[4] = DrvDip[0]; // 0x40 VBLANK, 0x08 Diagnostics (active low)
		DrvInput[5] = 0xff;

		for (INT32 i = 0; i < 8; i++) {
			DrvInput[0] ^= (DrvInputPort0[i] & 1) << i;
			DrvInput[1] ^= (DrvInputPort1[i] & 1) << i;
			DrvInput[2] ^= (DrvInputPort2[i] & 1) << i;
			DrvInput[3] ^= (DrvInputPort3[i] & 1) << i;
			DrvInput[5] ^= (DrvInputPort5[i] & 1) << i;
		}

		// Clear Opposites
		DrvClearOpposites(&DrvInput[0]);
		DrvClearOpposites(&DrvInput[1]);
		DrvClearOpposites(&DrvInput[2]);
		DrvClearOpposites(&DrvInput[3]);
	}

	INT32 nMult = 2;
	INT32 nInterleave = 262*nMult;
	INT32 nSoundBufferPos = 0;

	nCyclesTotal[0] = (14318180 / 2) / 60;
	nCyclesTotal[1] = (14318180 / 8) / 60;
	nCyclesDone[0] = nCyclesDone[1] = 0;
	
	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nCurrentCPU, nNext;

		nCurrentCPU = 0;
		SekOpen(0);
		nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
		nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
		nCyclesDone[nCurrentCPU] += SekRun(nCyclesSegment);
		if (i == 11*nMult) DrvVBlank = 0;
		if (i == 250*nMult) DrvVBlank = 1;
		if (i == 261*nMult) SekSetIRQLine(4, CPU_IRQSTATUS_ACK);
		SekClose();
		
		if (!DrvSoundCPUHalt) {
			M6502Open(0);
			nCurrentCPU = 1;
			nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
			nCyclesSegment = nNext - nCyclesDone[nCurrentCPU];
			nCyclesDone[nCurrentCPU] += M6502Run(nCyclesSegment);

			if ((i%nMult)==(nMult-1) && ((i/nMult) & 0x1f) == 0)
			{
				if ((i/nMult) & 0x20)
					M6502SetIRQLine(M6502_IRQ_LINE, CPU_IRQSTATUS_ACK);
			}
			M6502Close();
		} else {
			nCurrentCPU = 1;
			nNext = (i + 1) * nCyclesTotal[nCurrentCPU] / nInterleave;
			nCyclesDone[nCurrentCPU] += nNext; // idle skip
		}
		
		if (pBurnSoundOut) {
			INT32 nSegmentLength = nBurnSoundLen / nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			BurnYM2151Render(pSoundBuf, nSegmentLength);
			nSoundBufferPos += nSegmentLength;
		}

		if ((i % nMult) == (nMult - 1)) {
			UINT16 *AlphaRam = (UINT16*)DrvAlphaRam;
			DrvScrollY[i/nMult] = BURN_ENDIAN_SWAP_INT16(AlphaRam[0xf6e >> 1]);
		}
	}
	
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);

		if (nSegmentLength) {
			BurnYM2151Render(pSoundBuf, nSegmentLength);
		}
		pokey_update(pBurnSoundOut, nBurnSoundLen);
		tms5220_update(pBurnSoundOut, nBurnSoundLen);
	}
	
	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;
	
	if (pnMin != NULL) {			// Return minimum compatible version
		*pnMin = 0x029607;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = RamStart;
		ba.nLen	  = RamEnd-RamStart;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		SekScan(nAction);
		M6502Scan(nAction);

		BurnYM2151Scan(nAction, pnMin);
		pokey_scan(nAction, pnMin);
		tms5220_scan(nAction, pnMin);

		AtariSlapsticScan(nAction, pnMin);
		AtariMoScan(nAction, pnMin);

		BurnWatchdogScan(nAction);

		SCAN_VAR(DrvVBlank);
		SCAN_VAR(DrvSoundResetVal);
		SCAN_VAR(DrvSoundCPUHalt);
		SCAN_VAR(DrvCPUtoSoundReady);
		SCAN_VAR(DrvSoundtoCPUReady);
		SCAN_VAR(DrvCPUtoSound);
		SCAN_VAR(DrvSoundtoCPU);
		SCAN_VAR(speech_val);
		SCAN_VAR(last_speech_write);
	}

	AtariEEPROMScan(nAction, pnMin);

	return 0;
}


// Gauntlet (rev 14)

static struct BurnRomInfo GauntletRomDesc[] = {
	{ "136037-1307.9a",       0x08000, 0x46fe8743, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1308.9b",       0x08000, 0x276e15c4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-1409.7a",       0x08000, 0x6fb8419c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-1410.7b",       0x08000, 0x931bd2a0, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT},           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlet)
STD_ROM_FN(Gauntlet)

struct BurnDriver BurnDrvGauntlet = {
	"gauntlet", NULL, NULL, NULL, "1985",
	"Gauntlet (rev 14)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, GauntletRomInfo, GauntletRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (Spanish, rev 15)

static struct BurnRomInfo GauntletsRomDesc[] = {
	{ "136037-1507.9a",       0x08000, 0xb5183228, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1508.9b",       0x08000, 0xafd3c501, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-1509.7a",       0x08000, 0x69e50ae9, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-1510.7b",       0x08000, 0x54e2692c, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlets)
STD_ROM_FN(Gauntlets)

struct BurnDriver BurnDrvGauntlets = {
	"gauntlets", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (Spanish, rev 15)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, GauntletsRomInfo, GauntletsRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (Japanese, rev 13)

static struct BurnRomInfo GauntletjRomDesc[] = {
	{ "136037-1307.9a",       0x08000, 0x46fe8743, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1308.9b",       0x08000, 0x276e15c4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-1309.7a",       0x08000, 0xe8ba39d8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-1310.7b",       0x08000, 0xa204d997, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletj)
STD_ROM_FN(Gauntletj)

struct BurnDriver BurnDrvGauntletj = {
	"gauntletj", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (Japanese, rev 13)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, GauntletjRomInfo, GauntletjRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (German, rev 10)

static struct BurnRomInfo GauntletgRomDesc[] = {
	{ "136037-1007.9a",       0x08000, 0x6a224cea, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1008.9b",       0x08000, 0xfa391dab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-1009.7a",       0x08000, 0x75d1f966, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-1010.7b",       0x08000, 0x28a4197b, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletg)
STD_ROM_FN(Gauntletg)

struct BurnDriver BurnDrvGauntletg = {
	"gauntletg", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (German, rev 10)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, GauntletgRomInfo, GauntletgRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (Japanese, rev 12)

static struct BurnRomInfo Gauntletj12RomDesc[] = {
	{ "136037-1207.9a",       0x08000, 0x6dc0610d, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1208.9b",       0x08000, 0xfaa306eb, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-1109.7a",       0x08000, 0x500194fb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-1110.7b",       0x08000, 0xb2969076, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletj12)
STD_ROM_FN(Gauntletj12)

struct BurnDriver BurnDrvGauntletj12 = {
	"gauntletj12", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (Japanese, rev 12)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletj12RomInfo, Gauntletj12RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (rev 9)

static struct BurnRomInfo Gauntletr9RomDesc[] = {
	{ "136037-907.9a",        0x08000, 0xc13a6399, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-908.9b",        0x08000, 0x417607d9, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-909.7a",        0x08000, 0xfb1cdc1c, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-910.7b",        0x08000, 0xf188e7b3, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletr9)
STD_ROM_FN(Gauntletr9)

struct BurnDriver BurnDrvGauntletr9 = {
	"gauntletr9", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (rev 9)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletr9RomInfo, Gauntletr9RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (German, rev 8)

static struct BurnRomInfo Gauntletgr8RomDesc[] = {
	{ "136037-807.9a",        0x08000, 0x671c0bc2, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-808.9b",        0x08000, 0xf2842af4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-809.7a",        0x08000, 0x05642d60, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-810.7b",        0x08000, 0x36d295e3, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletgr8)
STD_ROM_FN(Gauntletgr8)

struct BurnDriver BurnDrvGauntletgr8 = {
	"gauntletgr8", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (German, rev 8)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletgr8RomInfo, Gauntletgr8RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (rev 7)

static struct BurnRomInfo Gauntletr7RomDesc[] = {
	{ "136037-207.9a",        0x08000, 0xfd871f81, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-208.9b",        0x08000, 0xbcb2fb1d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-709.7a",        0x08000, 0x73e1ad79, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-710.7b",        0x08000, 0xfd248cea, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletr7)
STD_ROM_FN(Gauntletr7)

struct BurnDriver BurnDrvGauntletr7 = {
	"gauntletr7", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (rev 7)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletr7RomInfo, Gauntletr7RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (German, rev 6)

static struct BurnRomInfo Gauntletgr6RomDesc[] = {
	{ "136037-307.9a",        0x08000, 0x759827c9, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-308.9b",        0x08000, 0xd71262d1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-609.7a",        0x08000, 0xcd3381de, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-610.7b",        0x08000, 0x2cff932a, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletgr6)
STD_ROM_FN(Gauntletgr6)

struct BurnDriver BurnDrvGauntletgr6 = {
	"gauntletgr6", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (German, rev 6)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletgr6RomInfo, Gauntletgr6RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (rev 5)

static struct BurnRomInfo Gauntletr5RomDesc[] = {
	{ "136037-207.9a",        0x08000, 0xfd871f81, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-208.9b",        0x08000, 0xbcb2fb1d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-409.7a",        0x08000, 0xc57377b3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-510.7b",        0x08000, 0x1cac2071, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletr5)
STD_ROM_FN(Gauntletr5)

struct BurnDriver BurnDrvGauntletr5 = {
	"gauntletr5", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (rev 5)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletr5RomInfo, Gauntletr5RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (rev 4)

static struct BurnRomInfo Gauntletr4RomDesc[] = {
	{ "136037-207.9a",        0x08000, 0xfd871f81, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-208.9b",        0x08000, 0xbcb2fb1d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-409.7a",        0x08000, 0xc57377b3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-410.7b",        0x08000, 0x6b971a27, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletr4)
STD_ROM_FN(Gauntletr4)

struct BurnDriver BurnDrvGauntletr4 = {
	"gauntletr4", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (rev 4)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletr4RomInfo, Gauntletr4RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (German, rev 3)

static struct BurnRomInfo Gauntletgr3RomDesc[] = {
	{ "136037-307.9a",        0x08000, 0x759827c9, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-308.9b",        0x08000, 0xd71262d1, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-309.7a",        0x08000, 0x7f03696b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-310.7b",        0x08000, 0x8d7197fc, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletgr3)
STD_ROM_FN(Gauntletgr3)

struct BurnDriver BurnDrvGauntletgr3 = {
	"gauntletgr3", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (German, rev 3)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletgr3RomInfo, Gauntletgr3RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (rev 2)

static struct BurnRomInfo Gauntletr2RomDesc[] = {
	{ "136037-207.9a",        0x08000, 0xfd871f81, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-208.9b",        0x08000, 0xbcb2fb1d, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-209.7a",        0x08000, 0xd810a7dc, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-210.7b",        0x08000, 0xfbba7290, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletr2)
STD_ROM_FN(Gauntletr2)

struct BurnDriver BurnDrvGauntletr2 = {
	"gauntletr2", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (rev 2)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletr2RomInfo, Gauntletr2RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (rev 1)

static struct BurnRomInfo Gauntletr1RomDesc[] = {
	{ "136037-107.9a",        0x08000, 0xa5885e14, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-108.9b",        0x08000, 0x0087f1ab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-105.10a",       0x04000, 0x4642cd95, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-106.10b",       0x04000, 0xc8df945e, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136037-109.7a",        0x08000, 0x55d87198, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136037-110.7b",        0x08000, 0xf84ad06d, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntletr1)
STD_ROM_FN(Gauntletr1)

struct BurnDriver BurnDrvGauntletr1 = {
	"gauntletr1", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (rev 1)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntletr1RomInfo, Gauntletr1RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (2 Players, rev 6)

static struct BurnRomInfo Gauntlet2pRomDesc[] = {
	{ "136041-507.9a",        0x08000, 0x8784133f, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136041-508.9b",        0x08000, 0x2843bde3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136041-609.7a",        0x08000, 0x5b4ee415, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136041-610.7b",        0x08000, 0x41f5c9e2, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlet2p)
STD_ROM_FN(Gauntlet2p)

struct BurnDriver BurnDrvGauntlet2p = {
	"gauntlet2p", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (2 Players, rev 6)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntlet2pRomInfo, Gauntlet2pRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (2 Players, Japanese, rev 5)

static struct BurnRomInfo Gauntlet2pjRomDesc[] = {
	{ "136041-507.9a",        0x08000, 0x8784133f, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136041-508.9b",        0x08000, 0x2843bde3, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136041-509.7a",        0x08000, 0xfb2ef226, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136041-510.7b",        0x08000, 0xa69be8da, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlet2pj)
STD_ROM_FN(Gauntlet2pj)

struct BurnDriver BurnDrvGauntlet2pj = {
	"gauntlet2pj", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (2 Players, Japanese, rev 5)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntlet2pjRomInfo, Gauntlet2pjRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (2 Players, German, rev 4)

static struct BurnRomInfo Gauntlet2pgRomDesc[] = {
	{ "136041-407.9a",        0x08000, 0xcde72140, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136041-408.9b",        0x08000, 0x4ab1af62, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136041-409.7a",        0x08000, 0x44e01459, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136041-410.7b",        0x08000, 0xb58d96d3, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlet2pg)
STD_ROM_FN(Gauntlet2pg)

struct BurnDriver BurnDrvGauntlet2pg = {
	"gauntlet2pg", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (2 Players, German, rev 4)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntlet2pgRomInfo, Gauntlet2pgRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (2 Players, rev 3)

static struct BurnRomInfo Gauntlet2pr3RomDesc[] = {
	{ "136041-207.9a",        0x08000, 0x0e1af1b4, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136041-208.9b",        0x08000, 0xbf51a238, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136041-309.7a",        0x08000, 0x5acbcd2b, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136041-310.7b",        0x08000, 0x1889ab77, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlet2pr3)
STD_ROM_FN(Gauntlet2pr3)

struct BurnDriver BurnDrvGauntlet2pr3 = {
	"gauntlet2pr3", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (2 Players, rev 3)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntlet2pr3RomInfo, Gauntlet2pr3RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (2 Players, Japanese rev 2)

static struct BurnRomInfo Gauntlet2pj2RomDesc[] = {
	{ "136041-207.9a",        0x08000, 0x0e1af1b4, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136041-208.9b",        0x08000, 0xbf51a238, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136041-209.7a",        0x08000, 0xddc9b56f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136041-210.7b",        0x08000, 0xffe78a4f, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlet2pj2)
STD_ROM_FN(Gauntlet2pj2)

struct BurnDriver BurnDrvGauntlet2pj2 = {
	"gauntlet2pj2", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (2 Players, Japanese rev 2)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntlet2pj2RomInfo, Gauntlet2pj2RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet (2 Players, German, rev 1)

static struct BurnRomInfo Gauntlet2pg1RomDesc[] = {
	{ "136041-107.9a",        0x08000, 0x3faf74d8, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136041-108.9b",        0x08000, 0xf1e6d815, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136037-205.10a",       0x04000, 0x6d99ed51, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136037-206.10b",       0x04000, 0x545ead91, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136041-109.7a",        0x08000, 0x56d0c5b8, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136041-110.7b",        0x08000, 0x3b9ae397, 1 | BRF_PRG | BRF_ESS }, //  5
	
	{ "136037-120.16r",       0x04000, 0x6ee7f3cc, 2 | BRF_PRG | BRF_ESS }, //  6	M6502 Program 
	{ "136037-119.16s",       0x08000, 0xfa19861f, 2 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136037-104.6p",        0x04000, 0x6c276a1d, 3 | BRF_GRA },           //  8	Characters
	
	{ "136037-111.1a",        0x08000, 0x91700f33, 4 | BRF_GRA },           //  9	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  10
	{ "136037-113.1l",        0x08000, 0xd497d0a8, 4 | BRF_GRA },           //  11
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  12
	{ "136037-115.2a",        0x08000, 0x9510b898, 4 | BRF_GRA },           //  13
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  14
	{ "136037-117.2l",        0x08000, 0x29a5db41, 4 | BRF_GRA },           //  15
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  16
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  18	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  19	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           //  20	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gauntlet2pg1)
STD_ROM_FN(Gauntlet2pg1)

struct BurnDriver BurnDrvGauntlet2pg1 = {
	"gauntlet2pg1", "gauntlet", NULL, NULL, "1985",
	"Gauntlet (2 Players, German, rev 1)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gauntlet2pg1RomInfo, Gauntlet2pg1RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2pInit, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet II

static struct BurnRomInfo Gaunt2RomDesc[] = {
	{ "136037-1307.9a",       0x08000, 0x46fe8743, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1308.9b",       0x08000, 0x276e15c4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136043-1105.10a",      0x04000, 0x45dfda47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136043-1106.10b",      0x04000, 0x343c029c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136043-1109.7a",       0x08000, 0x58a0a9a3, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136043-1110.7b",       0x08000, 0x658f0da8, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136043-1121.6a",       0x08000, 0xae301bba, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136043-1122.6b",       0x08000, 0xe94aaa8a, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136043-1120.16r",      0x04000, 0x5c731006, 2 | BRF_PRG | BRF_ESS }, //  8	M6502 Program 
	{ "136043-1119.16s",      0x08000, 0xdc3591e7, 2 | BRF_PRG | BRF_ESS }, //  9
	
	{ "136043-1104.6p",       0x04000, 0xbddc3dfc, 3 | BRF_GRA },           //  10	Characters
	
	{ "136043-1111.1a",       0x08000, 0x09df6e23, 4 | BRF_GRA },           //  11	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  12
	{ "136043-1123.1c",       0x04000, 0xe4c98f01, 4 | BRF_GRA },           //  13
	{ "136043-1113.1l",       0x08000, 0x33cb476e, 4 | BRF_GRA },           //  14	
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  15
	{ "136043-1124.1p",       0x04000, 0xc4857879, 4 | BRF_GRA },           //  16
	{ "136043-1115.2a",       0x08000, 0xf71e2503, 4 | BRF_GRA },           //  17
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  18
	{ "136043-1125.2c",       0x04000, 0xd9c2c2d1, 4 | BRF_GRA },           //  19
	{ "136043-1117.2l",       0x08000, 0x9e30b2e9, 4 | BRF_GRA },           //  20
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  21
	{ "136043-1126.2p",       0x04000, 0xa32c732a, 4 | BRF_GRA },           //  22
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  23	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  24	PROM (Motion Flip Control)
	{ "82s129-136043-1103.4r",0x00100, 0x32ae1fa9, 0 | BRF_OPT },           //  25	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gaunt2)
STD_ROM_FN(Gaunt2)

struct BurnDriver BurnDrvGaunt2 = {
	"gaunt2", NULL, NULL, NULL, "1986",
	"Gauntlet II\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gaunt2RomInfo, Gaunt2RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet II (German)

static struct BurnRomInfo Gaunt2gRomDesc[] = {
	{ "136037-1007.9a",       0x08000, 0x6a224cea, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1008.9b",       0x08000, 0xfa391dab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136043-1105.10a",      0x04000, 0x45dfda47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136043-1106.10b",      0x04000, 0x343c029c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136043-2209.7a",       0x08000, 0x577f4101, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136043-2210.7b",       0x08000, 0x03254cf4, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136043-2221.6a",       0x08000, 0xc8adcf1a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136043-2222.6b",       0x08000, 0x7788ff84, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136043-1120.16r",      0x04000, 0x5c731006, 2 | BRF_PRG | BRF_ESS }, //  8	M6502 Program 
	{ "136043-1119.16s",      0x08000, 0xdc3591e7, 2 | BRF_PRG | BRF_ESS }, //  9
	
	{ "136043-1104.6p",       0x04000, 0xbddc3dfc, 3 | BRF_GRA },           //  10	Characters
	
	{ "136043-1111.1a",       0x08000, 0x09df6e23, 4 | BRF_GRA },           //  11	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  12
	{ "136043-1123.1c",       0x04000, 0xe4c98f01, 4 | BRF_GRA },           //  13
	{ "136043-1113.1l",       0x08000, 0x33cb476e, 4 | BRF_GRA },           //  14	
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  15
	{ "136043-1124.1p",       0x04000, 0xc4857879, 4 | BRF_GRA },           //  16
	{ "136043-1115.2a",       0x08000, 0xf71e2503, 4 | BRF_GRA },           //  17
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  18
	{ "136043-1125.2c",       0x04000, 0xd9c2c2d1, 4 | BRF_GRA },           //  19
	{ "136043-1117.2l",       0x08000, 0x9e30b2e9, 4 | BRF_GRA },           //  20
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  21
	{ "136043-1126.2p",       0x04000, 0xa32c732a, 4 | BRF_GRA },           //  22
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  23	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  24	PROM (Motion Flip Control)
	{ "82s129-136043-1103.4r",0x00100, 0x32ae1fa9, 0 | BRF_OPT },           //  25	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gaunt2g)
STD_ROM_FN(Gaunt2g)

struct BurnDriver BurnDrvGaunt2g = {
	"gaunt2g", "gaunt2", NULL, NULL, "1986",
	"Gauntlet II (German)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 4, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gaunt2gRomInfo, Gaunt2gRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet II (2 Players, rev 2)

static struct BurnRomInfo Gaunt22pRomDesc[] = {
	{ "136037-1307.9a",       0x08000, 0x46fe8743, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1308.9b",       0x08000, 0x276e15c4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136043-1105.10a",      0x04000, 0x45dfda47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136043-1106.10b",      0x04000, 0x343c029c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136044-2109.7a",       0x08000, 0x1102ab96, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136044-2110.7b",       0x08000, 0xd2203a2b, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136044-2121.6a",       0x08000, 0x753982d7, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136044-2122.6b",       0x08000, 0x879149ea, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136043-1120.16r",      0x04000, 0x5c731006, 2 | BRF_PRG | BRF_ESS }, //  8	M6502 Program 
	{ "136043-1119.16s",      0x08000, 0xdc3591e7, 2 | BRF_PRG | BRF_ESS }, //  9
	
	{ "136043-1104.6p",       0x04000, 0xbddc3dfc, 3 | BRF_GRA },           //  10	Characters
	
	{ "136043-1111.1a",       0x08000, 0x09df6e23, 4 | BRF_GRA },           //  11	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  12
	{ "136043-1123.1c",       0x04000, 0xe4c98f01, 4 | BRF_GRA },           //  13
	{ "136043-1113.1l",       0x08000, 0x33cb476e, 4 | BRF_GRA },           //  14	
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  15
	{ "136043-1124.1p",       0x04000, 0xc4857879, 4 | BRF_GRA },           //  16
	{ "136043-1115.2a",       0x08000, 0xf71e2503, 4 | BRF_GRA },           //  17
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  18
	{ "136043-1125.2c",       0x04000, 0xd9c2c2d1, 4 | BRF_GRA },           //  19
	{ "136043-1117.2l",       0x08000, 0x9e30b2e9, 4 | BRF_GRA },           //  20
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  21
	{ "136043-1126.2p",       0x04000, 0xa32c732a, 4 | BRF_GRA },           //  22
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  23	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  24	PROM (Motion Flip Control)
	{ "82s129-136043-1103.4r",0x00100, 0x32ae1fa9, 0 | BRF_OPT },           //  25	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gaunt22p)
STD_ROM_FN(Gaunt22p)

struct BurnDriver BurnDrvGaunt22p = {
	"gaunt22p", "gaunt2", NULL, NULL, "1986",
	"Gauntlet II (2 Players, rev 2)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gaunt22pRomInfo, Gaunt22pRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet II (2 Players, rev 1)

static struct BurnRomInfo Gaunt22p1RomDesc[] = {
	{ "136037-1307.9a",       0x08000, 0x46fe8743, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1308.9b",       0x08000, 0x276e15c4, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136043-1105.10a",      0x04000, 0x45dfda47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136043-1106.10b",      0x04000, 0x343c029c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136044-1109.7a",       0x08000, 0x31f805eb, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136044-1110.7b",       0x08000, 0x5285c0e2, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136044-1121.6a",       0x08000, 0xd1f3b32a, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136044-1122.6b",       0x08000, 0x3485785f, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136043-1120.16r",      0x04000, 0x5c731006, 2 | BRF_PRG | BRF_ESS }, //  8	M6502 Program 
	{ "136043-1119.16s",      0x08000, 0xdc3591e7, 2 | BRF_PRG | BRF_ESS }, //  9
	
	{ "136043-1104.6p",       0x04000, 0xbddc3dfc, 3 | BRF_GRA },           //  10	Characters
	
	{ "136043-1111.1a",       0x08000, 0x09df6e23, 4 | BRF_GRA },           //  11	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  12
	{ "136043-1123.1c",       0x04000, 0xe4c98f01, 4 | BRF_GRA },           //  13
	{ "136043-1113.1l",       0x08000, 0x33cb476e, 4 | BRF_GRA },           //  14	
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  15
	{ "136043-1124.1p",       0x04000, 0xc4857879, 4 | BRF_GRA },           //  16
	{ "136043-1115.2a",       0x08000, 0xf71e2503, 4 | BRF_GRA },           //  17
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  18
	{ "136043-1125.2c",       0x04000, 0xd9c2c2d1, 4 | BRF_GRA },           //  19
	{ "136043-1117.2l",       0x08000, 0x9e30b2e9, 4 | BRF_GRA },           //  20
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  21
	{ "136043-1126.2p",       0x04000, 0xa32c732a, 4 | BRF_GRA },           //  22
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  23	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  24	PROM (Motion Flip Control)
	{ "82s129-136043-1103.4r",0x00100, 0x32ae1fa9, 0 | BRF_OPT },           //  25	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gaunt22p1)
STD_ROM_FN(Gaunt22p1)

struct BurnDriver BurnDrvGaunt22p1 = {
	"gaunt22p1", "gaunt2", NULL, NULL, "1986",
	"Gauntlet II (2 Players, rev 1)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gaunt22p1RomInfo, Gaunt22p1RomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Gauntlet II (2 Players, German)

static struct BurnRomInfo Gaunt22pgRomDesc[] = {
	{ "136037-1007.9a",       0x08000, 0x6a224cea, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136037-1008.9b",       0x08000, 0xfa391dab, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136043-1105.10a",      0x04000, 0x45dfda47, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136043-1106.10b",      0x04000, 0x343c029c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136044-2209.7a",       0x08000, 0x9da52ecd, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136044-2210.7b",       0x08000, 0x63d0f6a7, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136044-2221.6a",       0x08000, 0x8895b31b, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136044-2222.6b",       0x08000, 0xa4456cc7, 1 | BRF_PRG | BRF_ESS }, //  7
	
	{ "136043-1120.16r",      0x04000, 0x5c731006, 2 | BRF_PRG | BRF_ESS }, //  8	M6502 Program 
	{ "136043-1119.16s",      0x08000, 0xdc3591e7, 2 | BRF_PRG | BRF_ESS }, //  9
	
	{ "136043-1104.6p",       0x04000, 0xbddc3dfc, 3 | BRF_GRA },           //  10	Characters
	
	{ "136043-1111.1a",       0x08000, 0x09df6e23, 4 | BRF_GRA },           //  11	Motion Objects
	{ "136037-112.1b",        0x08000, 0x869330be, 4 | BRF_GRA },           //  12
	{ "136043-1123.1c",       0x04000, 0xe4c98f01, 4 | BRF_GRA },           //  13
	{ "136043-1113.1l",       0x08000, 0x33cb476e, 4 | BRF_GRA },           //  14	
	{ "136037-114.1mn",       0x08000, 0x29ef9882, 4 | BRF_GRA },           //  15
	{ "136043-1124.1p",       0x04000, 0xc4857879, 4 | BRF_GRA },           //  16
	{ "136043-1115.2a",       0x08000, 0xf71e2503, 4 | BRF_GRA },           //  17
	{ "136037-116.2b",        0x08000, 0x11e0ac5b, 4 | BRF_GRA },           //  18
	{ "136043-1125.2c",       0x04000, 0xd9c2c2d1, 4 | BRF_GRA },           //  19
	{ "136043-1117.2l",       0x08000, 0x9e30b2e9, 4 | BRF_GRA },           //  20
	{ "136037-118.2mn",       0x08000, 0x8bf3b263, 4 | BRF_GRA },           //  21
	{ "136043-1126.2p",       0x04000, 0xa32c732a, 4 | BRF_GRA },           //  22
		
	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           //  23	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           //  24	PROM (Motion Flip Control)
	{ "82s129-136043-1103.4r",0x00100, 0x32ae1fa9, 0 | BRF_OPT },           //  25	PROM (Motion Position/Size)
};

STD_ROM_PICK(Gaunt22pg)
STD_ROM_FN(Gaunt22pg)

struct BurnDriver BurnDrvGaunt22pg = {
	"gaunt22pg", "gaunt2", NULL, NULL, "1986",
	"Gauntlet II (2 Players, German)\0", NULL, "Atari Games", "Atari Gauntlet",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_MAZE | GBF_RUNGUN, 0,
	NULL, Gaunt22pgRomInfo, Gaunt22pgRomName, NULL, NULL, NULL, NULL , GauntletInputInfo, DrvDIPInfo,
	Gaunt2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Vindicators Part II (rev 3)

static struct BurnRomInfo vindctr2RomDesc[] = {
	{ "136059-1186.9a",		  0x08000, 0xaf138263, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136059-1187.9b",		  0x08000, 0x44baff64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-1196.10a",	  0x04000, 0xc92bf6dd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-1197.10b",	  0x04000, 0xd7ace347, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-3188.7a",		  0x08000, 0x10f558d2, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-3189.7b",		  0x08000, 0x302e24b6, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136059-2190.6a",		  0x08000, 0xe7dc2b74, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136059-2191.6b",		  0x08000, 0xed8ed86e, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136059-2192.5a",		  0x08000, 0xeec2c93d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136059-2193.5b",		  0x08000, 0x3fbee9aa, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136059-1194.3a",		  0x08000, 0xe6bcf458, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136059-1195.3b",		  0x08000, 0xb9bf245d, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136059-1160.16s",	  0x04000, 0xeef0a003, 2 | BRF_PRG | BRF_ESS }, // 12	M6502 Program 
	{ "136059-1161.16r",	  0x08000, 0x68c74337, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "136059-1198.6p",		  0x04000, 0xf99b631a, 3 | BRF_GRA },           // 14	Characters

	{ "136059-1162.1a",		  0x08000, 0xdd3833ad, 4 | BRF_GRA },           // 15	Motion Objects
	{ "136059-1166.1b",		  0x08000, 0xe2db50a0, 4 | BRF_GRA },           // 16
	{ "136059-1170.1c",		  0x08000, 0xf050ab43, 4 | BRF_GRA },           // 17
	{ "136059-1174.1d",		  0x08000, 0xb6704bd1, 4 | BRF_GRA },           // 18
	{ "136059-1178.1ef",	  0x08000, 0xd3006f05, 4 | BRF_GRA },           // 19
	{ "136059-1182.1j",		  0x08000, 0x9046e985, 4 | BRF_GRA },           // 20
	{ "136059-1163.1l",		  0x08000, 0xd505b04a, 4 | BRF_GRA },           // 21
	{ "136059-1167.1mn",  	  0x08000, 0x1869c76d, 4 | BRF_GRA },           // 22
	{ "136059-1171.1p",		  0x08000, 0x1b229c2b, 4 | BRF_GRA },           // 23
	{ "136059-1175.1r",		  0x08000, 0x73c41aca, 4 | BRF_GRA },           // 24
	{ "136059-1179.1st",	  0x08000, 0x9b7cb0ef, 4 | BRF_GRA },           // 25
	{ "136059-1183.1u",		  0x08000, 0x393bba42, 4 | BRF_GRA },           // 26
	{ "136059-1164.2a",		  0x08000, 0x50e76162, 4 | BRF_GRA },           // 27
	{ "136059-1168.2b",		  0x08000, 0x35c78469, 4 | BRF_GRA },           // 28
	{ "136059-1172.2c",		  0x08000, 0x314ac268, 4 | BRF_GRA },           // 29
	{ "136059-1176.2d",		  0x08000, 0x061d79db, 4 | BRF_GRA },           // 30
	{ "136059-1180.2ef",	  0x08000, 0x89c1fe16, 4 | BRF_GRA },           // 31
	{ "136059-1184.2j",	  	  0x08000, 0x541209d3, 4 | BRF_GRA },           // 32
	{ "136059-1165.2l",		  0x08000, 0x9484ba65, 4 | BRF_GRA },           // 33
	{ "136059-1169.2mn",	  0x08000, 0x132d3337, 4 | BRF_GRA },           // 34
	{ "136059-1173.2p",		  0x08000, 0x98de2426, 4 | BRF_GRA },           // 35
	{ "136059-1177.2r",		  0x08000, 0x9d0824f8, 4 | BRF_GRA },           // 36
	{ "136059-1181.2st",	  0x08000, 0x9e62b27c, 4 | BRF_GRA },           // 37
	{ "136059-1185.2u",		  0x08000, 0x9d62f6b7, 4 | BRF_GRA },           // 38

	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           // 39	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           // 40	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           // 41	PROM (Motion Position/Size)
};

STD_ROM_PICK(vindctr2)
STD_ROM_FN(vindctr2)

struct BurnDriver BurnDrvVindctr2 = {
	"vindctr2", NULL, NULL, NULL, "1988",
	"Vindicators Part II (rev 3)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, vindctr2RomInfo, vindctr2RomName, NULL, NULL, NULL, NULL, Vindctr2InputInfo, Vindctr2DIPInfo,
	Vindctr2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Vindicators Part II (rev 2)

static struct BurnRomInfo vindctr2r2RomDesc[] = {
	{ "136059-1186.9a",		  0x08000, 0xaf138263, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136059-1187.9b",		  0x08000, 0x44baff64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-1196.10a",	  0x04000, 0xc92bf6dd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-1197.10b",	  0x04000, 0xd7ace347, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-2188.7a",		  0x08000, 0xd4e0ef1f, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-2189.7b",		  0x08000, 0xdcbbe2aa, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136059-2190.6a",		  0x08000, 0xe7dc2b74, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136059-2191.6b",		  0x08000, 0xed8ed86e, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136059-2192.5a",		  0x08000, 0xeec2c93d, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136059-2193.5b",		  0x08000, 0x3fbee9aa, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136059-1194.3a",		  0x08000, 0xe6bcf458, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136059-1195.3b",		  0x08000, 0xb9bf245d, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136059-1160.16s",	  0x04000, 0xeef0a003, 2 | BRF_PRG | BRF_ESS }, // 12	M6502 Program 
	{ "136059-1161.16r",	  0x08000, 0x68c74337, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "136059-1198.6p",		  0x04000, 0xf99b631a, 3 | BRF_GRA },           // 14	Characters

	{ "136059-1162.1a",		  0x08000, 0xdd3833ad, 4 | BRF_GRA },           // 15	Motion Objects
	{ "136059-1166.1b",		  0x08000, 0xe2db50a0, 4 | BRF_GRA },           // 16
	{ "136059-1170.1c",		  0x08000, 0xf050ab43, 4 | BRF_GRA },           // 17
	{ "136059-1174.1d",		  0x08000, 0xb6704bd1, 4 | BRF_GRA },           // 18
	{ "136059-1178.1ef",	  0x08000, 0xd3006f05, 4 | BRF_GRA },           // 19
	{ "136059-1182.1j",		  0x08000, 0x9046e985, 4 | BRF_GRA },           // 20
	{ "136059-1163.1l",		  0x08000, 0xd505b04a, 4 | BRF_GRA },           // 21
	{ "136059-1167.1mn",	  0x08000, 0x1869c76d, 4 | BRF_GRA },           // 22
	{ "136059-1171.1p",		  0x08000, 0x1b229c2b, 4 | BRF_GRA },           // 23
	{ "136059-1175.1r",		  0x08000, 0x73c41aca, 4 | BRF_GRA },           // 24
	{ "136059-1179.1st",	  0x08000, 0x9b7cb0ef, 4 | BRF_GRA },           // 25
	{ "136059-1183.1u",		  0x08000, 0x393bba42, 4 | BRF_GRA },           // 26
	{ "136059-1164.2a",		  0x08000, 0x50e76162, 4 | BRF_GRA },           // 27
	{ "136059-1168.2b",		  0x08000, 0x35c78469, 4 | BRF_GRA },           // 28
	{ "136059-1172.2c",		  0x08000, 0x314ac268, 4 | BRF_GRA },           // 29
	{ "136059-1176.2d",	 	  0x08000, 0x061d79db, 4 | BRF_GRA },           // 30
	{ "136059-1180.2ef", 	  0x08000, 0x89c1fe16, 4 | BRF_GRA },           // 31
	{ "136059-1184.2j",		  0x08000, 0x541209d3, 4 | BRF_GRA },           // 32
	{ "136059-1165.2l",		  0x08000, 0x9484ba65, 4 | BRF_GRA },           // 33
	{ "136059-1169.2mn",	  0x08000, 0x132d3337, 4 | BRF_GRA },           // 34
	{ "136059-1173.2p",		  0x08000, 0x98de2426, 4 | BRF_GRA },           // 35
	{ "136059-1177.2r",		  0x08000, 0x9d0824f8, 4 | BRF_GRA },           // 36
	{ "136059-1181.2st",	  0x08000, 0x9e62b27c, 4 | BRF_GRA },           // 37
	{ "136059-1185.2u",		  0x08000, 0x9d62f6b7, 4 | BRF_GRA },           // 38

	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           // 39	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           // 40	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           // 41	PROM (Motion Position/Size)
};

STD_ROM_PICK(vindctr2r2)
STD_ROM_FN(vindctr2r2)

struct BurnDriver BurnDrvVindctr2r2 = {
	"vindctr2r2", "vindctr2", NULL, NULL, "1988",
	"Vindicators Part II (rev 2)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, vindctr2r2RomInfo, vindctr2r2RomName, NULL, NULL, NULL, NULL, Vindctr2InputInfo, Vindctr2DIPInfo,
	Vindctr2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};


// Vindicators Part II (rev 1)

static struct BurnRomInfo vindctr2r1RomDesc[] = {
	{ "136059-1186.9a",		  0x08000, 0xaf138263, 1 | BRF_PRG | BRF_ESS }, //  0	68000 Program Code
	{ "136059-1187.9b",		  0x08000, 0x44baff64, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "136059-1196.10a",	  0x04000, 0xc92bf6dd, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "136059-1197.10b",	  0x04000, 0xd7ace347, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "136059-1188.7a",		  0x08000, 0x52294cad, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "136059-1189.7b",		  0x08000, 0x577a705f, 1 | BRF_PRG | BRF_ESS }, //  5
	{ "136059-1190.6a",		  0x08000, 0x7be01bb1, 1 | BRF_PRG | BRF_ESS }, //  6
	{ "136059-1191.6b",		  0x08000, 0x91922a02, 1 | BRF_PRG | BRF_ESS }, //  7
	{ "136059-1192.5a",		  0x08000, 0xe4f59d72, 1 | BRF_PRG | BRF_ESS }, //  8
	{ "136059-1193.5b",		  0x08000, 0xe901c618, 1 | BRF_PRG | BRF_ESS }, //  9
	{ "136059-1194.3a",		  0x08000, 0xe6bcf458, 1 | BRF_PRG | BRF_ESS }, // 10
	{ "136059-1195.3b",		  0x08000, 0xb9bf245d, 1 | BRF_PRG | BRF_ESS }, // 11

	{ "136059-1160.16s",	  0x04000, 0xeef0a003, 2 | BRF_PRG | BRF_ESS }, // 12	M6502 Program 
	{ "136059-1161.16r",	  0x08000, 0x68c74337, 2 | BRF_PRG | BRF_ESS }, // 13

	{ "136059-1198.6p",		  0x04000, 0xf99b631a, 3 | BRF_GRA },           // 14	Characters

	{ "136059-1162.1a",		  0x08000, 0xdd3833ad, 4 | BRF_GRA },           // 15	Motion Objects
	{ "136059-1166.1b",		  0x08000, 0xe2db50a0, 4 | BRF_GRA },           // 16
	{ "136059-1170.1c",		  0x08000, 0xf050ab43, 4 | BRF_GRA },           // 17
	{ "136059-1174.1d",		  0x08000, 0xb6704bd1, 4 | BRF_GRA },           // 18
	{ "136059-1178.1ef",	  0x08000, 0xd3006f05, 4 | BRF_GRA },           // 19
	{ "136059-1182.1j",		  0x08000, 0x9046e985, 4 | BRF_GRA },           // 20
	{ "136059-1163.1l",		  0x08000, 0xd505b04a, 4 | BRF_GRA },           // 21
	{ "136059-1167.1mn",	  0x08000, 0x1869c76d, 4 | BRF_GRA },           // 22
	{ "136059-1171.1p",		  0x08000, 0x1b229c2b, 4 | BRF_GRA },           // 23
	{ "136059-1175.1r",		  0x08000, 0x73c41aca, 4 | BRF_GRA },           // 24
	{ "136059-1179.1st",	  0x08000, 0x9b7cb0ef, 4 | BRF_GRA },           // 25
	{ "136059-1183.1u",		  0x08000, 0x393bba42, 4 | BRF_GRA },           // 26
	{ "136059-1164.2a",		  0x08000, 0x50e76162, 4 | BRF_GRA },           // 27
	{ "136059-1168.2b",		  0x08000, 0x35c78469, 4 | BRF_GRA },           // 28
	{ "136059-1172.2c",		  0x08000, 0x314ac268, 4 | BRF_GRA },           // 29
	{ "136059-1176.2d",		  0x08000, 0x061d79db, 4 | BRF_GRA },           // 30
	{ "136059-1180.2ef",	  0x08000, 0x89c1fe16, 4 | BRF_GRA },           // 31
	{ "136059-1184.2j",		  0x08000, 0x541209d3, 4 | BRF_GRA },           // 32
	{ "136059-1165.2l",		  0x08000, 0x9484ba65, 4 | BRF_GRA },           // 33
	{ "136059-1169.2mn",	  0x08000, 0x132d3337, 4 | BRF_GRA },           // 34
	{ "136059-1173.2p",		  0x08000, 0x98de2426, 4 | BRF_GRA },           // 35
	{ "136059-1177.2r",		  0x08000, 0x9d0824f8, 4 | BRF_GRA },           // 36
	{ "136059-1181.2st",	  0x08000, 0x9e62b27c, 4 | BRF_GRA },           // 37
	{ "136059-1185.2u",		  0x08000, 0x9d62f6b7, 4 | BRF_GRA },           // 38

	{ "74s472-136037-101.7u", 0x00200, 0x2964f76f, 0 | BRF_OPT },           // 39	PROM (Motion Timing)
	{ "74s472-136037-102.5l", 0x00200, 0x4d4fec6c, 0 | BRF_OPT },           // 40	PROM (Motion Flip Control)
	{ "74s287-136037-103.4r", 0x00100, 0x6c5ccf08, 0 | BRF_OPT },           // 41	PROM (Motion Position/Size)
};

STD_ROM_PICK(vindctr2r1)
STD_ROM_FN(vindctr2r1)

struct BurnDriver BurnDrvVindctr2r1 = {
	"vindctr2r1", "vindctr2", NULL, NULL, "1988",
	"Vindicators Part II (rev 1)\0", NULL, "Atari Games", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, vindctr2r1RomInfo, vindctr2r1RomName, NULL, NULL, NULL, NULL, Vindctr2InputInfo, Vindctr2DIPInfo,
	Vindctr2Init, DrvExit, DrvFrame, DrvDraw, DrvScan, NULL,
	0x400, 336, 240, 4, 3
};
