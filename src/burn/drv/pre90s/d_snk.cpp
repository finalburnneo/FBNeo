// FB Alpha SNK triple Z80 driver module
// Based on MAME driver by Marco Cassili, Ernesto Corvi, Tim Lindquist, Carlos A. Lozano, Bryan McPhail, Jarek Parchanski, Nicola Salmoria, Tomasz Slanina, Phil Stroffolino, Acho A. Tang, Victor Trucco

//
// Todo:
//  x tdfever, fsoccer - hook up inputs & trackball(?)
//  x tnk3's second player can't use the new rotate-button feature, probably needs flipscreen & coctail mode to work.

#include "tiles_generic.h"
#include "z80_intf.h"
#include "burn_ym3526.h"
#include "burn_ym3812.h"
#include "burn_y8950.h"
#include "driver.h"
extern "C" {
#include "ay8910.h"
}

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvGfxROM2;
static UINT8 *DrvGfxROM3;
static UINT8 *DrvGfxROM4;
static UINT8 *DrvSndROM0;
static UINT8 *DrvColPROM;
static UINT8 *DrvZ80RAM2;
static UINT8 *DrvBgVRAM;
static UINT8 *DrvFgVRAM;

static UINT8 *DrvShareRAM;
static UINT8 *DrvSprRAM;
static UINT8 *DrvTxtRAM;
static UINT8 *DrvTransTab;

static INT16 *pAY8910Buffer[6];

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 sound_status;
static UINT8 soundlatch;
static UINT8 flipscreen;
static UINT16 sp16_scrolly;
static UINT16 sp16_scrollx;
static UINT16 sp32_scrolly;
static UINT16 sp32_scrollx;
static UINT16 bg_scrollx;
static UINT16 bg_scrolly;
static UINT16 fg_scrollx;
static UINT16 fg_scrolly;
static UINT16 txt_palette_offset;
static UINT16 txt_tile_offset;
static UINT16 bg_tile_offset;
static UINT16 bg_palette_offset;
static UINT16 fg_palette_offset;
static UINT8 sprite_split_point;
static UINT16 tc16_posy = 0;
static UINT16 tc16_posx = 0;
static UINT16 tc32_posy = 0;
static UINT16 tc32_posx = 0;

static INT32 DrvGfxMask[5] = { ~0, ~0, ~0, ~0, ~0 };
static INT32 nSampleLen = 0;
static INT32 game_select = 0;
static INT32 video_y_scroll_mask = 0x1ff;
static INT32 video_sprite_number = 50;
static INT32 hal21mode = 0;
static INT32 bonus_dip_config = 0; // xxyy yy = dip0 bonus, xx = dip1 bonus
static INT32 ikarijoy = 0;

static UINT8  DrvJoy1[8];
static UINT8  DrvJoy2[8];
static UINT8  DrvJoy3[8];
static UINT8  DrvJoy4[8];
static UINT8  DrvJoy5[8];
static UINT8  DrvJoy6[8];
static UINT8  DrvDips[3];
static UINT8  DrvInputs[6];
static UINT8  DrvReset;

static UINT8  DrvFakeInput[6] = {0, 0, 0, 0, 0, 0};
static INT32  nRotate[2]      = {0, 0};
static INT32  nRotateTarget[2]      = {0, 0};
static INT32  nRotateTry[2]      = {0, 0};
static UINT32 nRotateTime[2]  = {0, 0};
static UINT8  game_rotates = 0;
static UINT8  gwar_rot_last[2] = {0, 0};
static UINT8  gwar_rot_cnt[2] = {0, 0};


static struct BurnInputInfo PsychosInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"		},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Tilt",		BIT_DIGITAL,    DrvJoy1 + 2,    "tilt"          },
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
};

STDINPUTINFO(Psychos)

static struct BurnInputInfo GwarInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0, "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1, "p1 rotate right" },
	{"P1 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 4, "p1 fire 3" },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 2"},
	{"P2 Rotate Left"  , BIT_DIGITAL  , DrvFakeInput + 2, "p2 rotate left" },
	{"P2 Rotate Right" , BIT_DIGITAL  , DrvFakeInput + 3, "p2 rotate right" },
	{"P2 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 5, "p2 fire 3" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Gwar)

static struct BurnInputInfo GwarbInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Gwarb)

static struct BurnInputInfo MarvinsInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Marvins)

static struct BurnInputInfo Vangrd2InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Vangrd2)

static struct BurnInputInfo MadcrashInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Madcrash)

static struct BurnInputInfo JcrossInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip c",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Jcross)

static struct BurnInputInfo SgladiatInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 2"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Sgladiat)

static struct BurnInputInfo Hal21InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Hal21)

static struct BurnInputInfo AthenaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Athena)

static struct BurnInputInfo AsoInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Aso)

static struct BurnInputInfo Tnk3InputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0, "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1, "p1 rotate right" },
	{"P1 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 4, "p1 fire 3" },

  	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 2,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 2"},
	{"P2 Rotate Left"  , BIT_DIGITAL  , DrvFakeInput + 2, "p2 rotate left" },
	{"P2 Rotate Right" , BIT_DIGITAL  , DrvFakeInput + 3, "p2 rotate right" },
	{"P2 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 5, "p2 fire 3" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Tnk3)

static struct BurnInputInfo IkariInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0, "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1, "p1 rotate right" },
	{"P1 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 4, "p1 fire 3" },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 2"},
	{"P2 Rotate Left"  , BIT_DIGITAL  , DrvFakeInput + 2, "p2 rotate left" },
	{"P2 Rotate Right" , BIT_DIGITAL  , DrvFakeInput + 3, "p2 rotate right" },
	{"P2 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 5, "p2 fire 3" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Ikari)

static struct BurnInputInfo IkariaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0, "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1, "p1 rotate right" },
	{"P1 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 4, "p1 fire 3" },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 2"},
	{"P2 Rotate Left"  , BIT_DIGITAL  , DrvFakeInput + 2, "p2 rotate left" },
	{"P2 Rotate Right" , BIT_DIGITAL  , DrvFakeInput + 3, "p2 rotate right" },
	{"P2 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 5, "p2 fire 3" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Ikaria)

static struct BurnInputInfo VictroadInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0, "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1, "p1 rotate right" },
	{"P1 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 4, "p1 fire 3" },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 2"},
	{"P2 Rotate Left"  , BIT_DIGITAL  , DrvFakeInput + 2, "p2 rotate left" },
	{"P2 Rotate Right" , BIT_DIGITAL  , DrvFakeInput + 3, "p2 rotate right" },
	{"P2 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 5, "p2 fire 3" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Victroad)

static struct BurnInputInfo FitegolfInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Fitegolf)

static struct BurnInputInfo AlphamisInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 2"},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 2"},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 3"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Alphamis)

static struct BurnInputInfo ChopperInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 7,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Chopper)

static struct BurnInputInfo ChopperaInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Choppera)

static struct BurnInputInfo BermudatInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p1 fire 2"},
	{"P1 Rotate Left"       , BIT_DIGITAL  , DrvFakeInput + 0, "p1 rotate left" },
	{"P1 Rotate Right"      , BIT_DIGITAL  , DrvFakeInput + 1, "p1 rotate right" },
	{"P1 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 4, "p1 fire 3" },

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 3,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 fire 2"},
	{"P2 Rotate Left"  , BIT_DIGITAL  , DrvFakeInput + 2, "p2 rotate left" },
	{"P2 Rotate Right" , BIT_DIGITAL  , DrvFakeInput + 3, "p2 rotate right" },
	{"P2 Button 3 (rotate)"      , BIT_DIGITAL  , DrvFakeInput + 5, "p2 fire 3" },

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 1,	"service"},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 2,	"tilt"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
	{"Dip C",		BIT_DIPSWITCH,	DrvDips + 2,	"dip"},
};

STDINPUTINFO(Bermudat)

#if 0 // someday we might hook these up....
static struct BurnInputInfo TdfeverInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 2"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy5 + 7,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy5 + 6,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy5 + 5,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy5 + 4,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy5 + 1,	"p3 fire 2"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy6 + 7,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy6 + 6,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy6 + 5,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy6 + 4,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy6 + 0,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy6 + 1,	"p4 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Tdfever)
#endif

static struct BurnInputInfo FsoccerInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy3 + 7,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy3 + 6,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy3 + 5,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy3 + 4,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy3 + 0,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy3 + 1,	"p1 fire 2"},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 5,	"p2 coin"},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 start"},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy4 + 7,	"p2 up"},
	{"P2 Down",		BIT_DIGITAL,	DrvJoy4 + 6,	"p2 down"},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy4 + 5,	"p2 left"},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy4 + 4,	"p2 right"},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy4 + 0,	"p2 fire 1"},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy4 + 1,	"p2 fire 2"},

	{"P3 Start",		BIT_DIGITAL,	DrvJoy2 + 6,	"p3 start"},
	{"P3 Up",		BIT_DIGITAL,	DrvJoy5 + 7,	"p3 up"},
	{"P3 Down",		BIT_DIGITAL,	DrvJoy5 + 6,	"p3 down"},
	{"P3 Left",		BIT_DIGITAL,	DrvJoy5 + 5,	"p3 left"},
	{"P3 Right",		BIT_DIGITAL,	DrvJoy5 + 4,	"p3 right"},
	{"P3 Button 1",		BIT_DIGITAL,	DrvJoy5 + 0,	"p3 fire 1"},
	{"P3 Button 2",		BIT_DIGITAL,	DrvJoy5 + 1,	"p3 fire 2"},

	{"P4 Start",		BIT_DIGITAL,	DrvJoy2 + 7,	"p4 start"},
	{"P4 Up",		BIT_DIGITAL,	DrvJoy6 + 7,	"p4 up"},
	{"P4 Down",		BIT_DIGITAL,	DrvJoy6 + 6,	"p4 down"},
	{"P4 Left",		BIT_DIGITAL,	DrvJoy6 + 5,	"p4 left"},
	{"P4 Right",		BIT_DIGITAL,	DrvJoy6 + 4,	"p4 right"},
	{"P4 Button 1",		BIT_DIGITAL,	DrvJoy6 + 0,	"p4 fire 1"},
	{"P4 Button 2",		BIT_DIGITAL,	DrvJoy6 + 1,	"p4 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy1 + 2,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"},
};

STDINPUTINFO(Fsoccer)

#if 0
static struct BurnDIPInfo TdfeverDIPList[]=
{
	{0x20, 0xff, 0xff, 0x32, NULL		},
	{0x21, 0xff, 0xff, 0xfb, NULL		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x20, 0x01, 0x01, 0x01, "No"		},
	{0x20, 0x01, 0x01, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x20, 0x01, 0x02, 0x02, "2 Player Upright"		},
	{0x20, 0x01, 0x02, 0x00, "4 Player Cocktail"		},

	{0   , 0xfe, 0   ,    4, "1st Down Bonus Time"		},
	{0x20, 0x01, 0x0c, 0x00, "Every 1st Down"		},
	{0x20, 0x01, 0x0c, 0x04, "Every 4 1st Downs"		},
	{0x20, 0x01, 0x0c, 0x08, "Every 6 1st Downs"		},
	{0x20, 0x01, 0x0c, 0x0c, "Every 8 1st Downs"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x20, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x20, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x20, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x20, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x20, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x20, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x20, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x20, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x21, 0x01, 0x03, 0x03, "Easy"		},
	{0x21, 0x01, 0x03, 0x02, "Normal"		},
	{0x21, 0x01, 0x03, 0x01, "Hard"		},
	{0x21, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x21, 0x01, 0x0c, 0x0c, "Demo Sound Off"		},
	{0x21, 0x01, 0x0c, 0x08, "Demo Sound On"		},
	{0x21, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x21, 0x01, 0x0c, 0x04, "Never Finish (Cheat)"		},

	{0   , 0xfe, 0   ,    8, "Play Time (Type A)"		},
	{0x21, 0x01, 0x70, 0x70, "1:00"		},
	{0x21, 0x01, 0x70, 0x60, "1:10"		},
	{0x21, 0x01, 0x70, 0x50, "1:20"		},
	{0x21, 0x01, 0x70, 0x40, "1:30"		},
	{0x21, 0x01, 0x70, 0x30, "1:40"		},
	{0x21, 0x01, 0x70, 0x20, "1:50"		},
	{0x21, 0x01, 0x70, 0x10, "2:00"		},
	{0x21, 0x01, 0x70, 0x00, "2:10"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x21, 0x01, 0x80, 0x80, "Off"		},
	{0x21, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Tdfever)
#endif

static struct BurnDIPInfo FsoccerDIPList[]=
{
	{0x20, 0xff, 0xff, 0x37, NULL		},
	{0x21, 0xff, 0xff, 0x7f, NULL		},

	{0   , 0xfe, 0   ,    4, "Cabinet"		},
	{0x20, 0x01, 0x03, 0x03, "Upright (With VS)"		},
	{0x20, 0x01, 0x03, 0x02, "Upright (Without VS)"		},
	{0x20, 0x01, 0x03, 0x00, "Cocktail (2 Players)"		},
	{0x20, 0x01, 0x03, 0x01, "Cocktail (4 Players)"		},

	{0   , 0xfe, 0   ,    3, "Version"		},
	{0x20, 0x01, 0x0c, 0x04, "Europe"		},
	{0x20, 0x01, 0x0c, 0x00, "USA"		},
	{0x20, 0x01, 0x0c, 0x08, "Japan"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x20, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x20, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x20, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x20, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x20, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x20, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x20, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x20, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x21, 0x01, 0x01, 0x00, "No"		},
	{0x21, 0x01, 0x01, 0x01, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x21, 0x01, 0x02, 0x02, "Off"		},
	{0x21, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x21, 0x01, 0x0c, 0x08, "Demo Sound Off"		},
	{0x21, 0x01, 0x0c, 0x0c, "Demo Sound On"		},
	{0x21, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x21, 0x01, 0x0c, 0x04, "Win Match Against CPU (Cheat)"		},

	{0   , 0xfe, 0   ,    8, "Play Time (Type A)"		},
	{0x21, 0x01, 0x70, 0x10, "1:00"		},
	{0x21, 0x01, 0x70, 0x60, "1:10"		},
	{0x21, 0x01, 0x70, 0x50, "1:20"		},
	{0x21, 0x01, 0x70, 0x40, "1:30"		},
	{0x21, 0x01, 0x70, 0x30, "1:40"		},
	{0x21, 0x01, 0x70, 0x20, "1:50"		},
	{0x21, 0x01, 0x70, 0x70, "2:00"		},
	{0x21, 0x01, 0x70, 0x00, "2:10"		},
};

STDDIPINFO(Fsoccer)

static struct BurnDIPInfo PsychosDIPList[]=
{
	{0x12, 0xff, 0xff, 0xff, NULL			},
	{0x13, 0xff, 0xff, 0xff, NULL			},
	{0x14, 0xff, 0xff, 0x34, NULL			},

	{0   , 0xfe, 0   ,    2, "Service Mode"		},
	{0x12, 0x01, 0x01, 0x01, "Off"			},
	{0x12, 0x01, 0x01, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x02, 0x02, "Off"			},
	{0x12, 0x01, 0x02, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x08, 0x08, "3"			},
	{0x12, 0x01, 0x08, 0x00, "5"			},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x30, 0x00, "4 Coins 1 Credit"	},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credit"	},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credit"	},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credit"	},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"	},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"	},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"	},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"	},

	{0   , 0xfe, 0   ,    4, "Service Mode"		},
	{0x13, 0x01, 0x03, 0x02, "Easy"			},
	{0x13, 0x01, 0x03, 0x03, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"			},
	{0x13, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x04, 0x04, "Off"			},
	{0x13, 0x01, 0x04, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x13, 0x01, 0x08, 0x08, "Off"			},
	{0x13, 0x01, 0x08, 0x00, "On"			},

	{0   , 0xfe, 0   ,    2, "Allow Continue"	},
	{0x13, 0x01, 0x40, 0x40, "No"			},
	{0x13, 0x01, 0x40, 0x00, "Yes"			},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x14, 0x01, 0x34, 0x30, "50k 100k 100k+"	},
	{0x14, 0x01, 0x34, 0x20, "60k 120k 120k+"	},
	{0x14, 0x01, 0x34, 0x10, "100k 200k 200k+"	},
	{0x14, 0x01, 0x34, 0x00, "50k"			},
	{0x14, 0x01, 0x34, 0x01, "60k"			},
	{0x14, 0x01, 0x34, 0x00, "100k"			},
	{0x14, 0x01, 0x34, 0x00, "None"			},
};

STDDIPINFO(Psychos)

static struct BurnDIPInfo GwarDIPList[]=
{
	{0x19, 0xff, 0xff, 0x3b, NULL		},
	{0x1a, 0xff, 0xff, 0x0a, NULL		},
	{0x1b, 0xff, 0xff, 0x30, NULL		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x19, 0x01, 0x01, 0x00, "No"		},
	{0x19, 0x01, 0x01, 0x01, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x19, 0x01, 0x02, 0x02, "Off"		},
	{0x19, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x19, 0x01, 0x08, 0x08, "3"		},
	{0x19, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x19, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x19, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x19, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x19, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x19, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x1a, 0x01, 0x03, 0x03, "Easy"		},
	{0x1a, 0x01, 0x03, 0x02, "Normal"		},
	{0x1a, 0x01, 0x03, 0x01, "Hard"		},
	{0x1a, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x1a, 0x01, 0x0c, 0x0c, "Demo Sounds Off"		},
	{0x1a, 0x01, 0x0c, 0x08, "Demo Sounds On"		},
	{0x1a, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x1a, 0x01, 0x0c, 0x04, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x1b, 0x01, 0x34, 0x30, "30k 60k 60k+"		},
	{0x1b, 0x01, 0x34, 0x20, "40k 80k 80k+"		},
	{0x1b, 0x01, 0x34, 0x10, "50k 100k 100k+"		},
	{0x1b, 0x01, 0x34, 0x34, "30k 60k"		},
	{0x1b, 0x01, 0x34, 0x24, "40k 80k"		},
	{0x1b, 0x01, 0x34, 0x14, "50k 100k"		},
	{0x1b, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Gwar)

static struct BurnDIPInfo GwarbDIPList[]=
{
	{0x13, 0xff, 0xff, 0x3b, NULL		},
	{0x14, 0xff, 0xff, 0x0a, NULL		},
	{0x15, 0xff, 0xff, 0x30, NULL		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x01, 0x00, "No"		},
	{0x13, 0x01, 0x01, 0x01, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"		},
	{0x13, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x08, 0x08, "3"		},
	{0x13, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x03, "Easy"		},
	{0x14, 0x01, 0x03, 0x02, "Normal"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"		},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x14, 0x01, 0x0c, 0x0c, "Demo Sounds Off"		},
	{0x14, 0x01, 0x0c, 0x08, "Demo Sounds On"		},
	{0x14, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x14, 0x01, 0x0c, 0x04, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x15, 0x01, 0x34, 0x30, "30k 60k 60k+"		},
	{0x15, 0x01, 0x34, 0x20, "40k 80k 80k+"		},
	{0x15, 0x01, 0x34, 0x10, "50k 100k 100k+"		},
	{0x15, 0x01, 0x34, 0x34, "30k 60k"		},
	{0x15, 0x01, 0x34, 0x24, "40k 80k"		},
	{0x15, 0x01, 0x34, 0x14, "50k 100k"		},
	{0x15, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Gwarb)

static struct BurnDIPInfo MarvinsDIPList[]=
{
	{0x10, 0xff, 0xff, 0xc6, NULL		},
	{0x11, 0xff, 0xff, 0xa8, NULL		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0x03, 0x00, "1"		},
	{0x10, 0x01, 0x03, 0x01, "2"		},
	{0x10, 0x01, 0x03, 0x02, "3"		},
	{0x10, 0x01, 0x03, 0x03, "5"		},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"		},
	{0x10, 0x01, 0x04, 0x04, "Off"		},
	{0x10, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x10, 0x01, 0x38, 0x38, "5 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x30, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x28, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x00, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x38, 0x08, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x38, 0x18, "1 Coin  5 Credits"		},
	{0x10, 0x01, 0x38, 0x20, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x10, 0x01, 0x40, 0x40, "Off"		},
	{0x10, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x10, 0x01, 0x80, 0x80, "Off"		},
	{0x10, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    8, "1st Bonus Life"		},
	{0x11, 0x01, 0x07, 0x00, "10000"		},
	{0x11, 0x01, 0x07, 0x01, "20000"		},
	{0x11, 0x01, 0x07, 0x02, "30000"		},
	{0x11, 0x01, 0x07, 0x03, "40000"		},
	{0x11, 0x01, 0x07, 0x04, "50000"		},
	{0x11, 0x01, 0x07, 0x05, "60000"		},
	{0x11, 0x01, 0x07, 0x06, "70000"		},
	{0x11, 0x01, 0x07, 0x07, "80000"		},

	{0   , 0xfe, 0   ,    4, "2nd Bonus Life"		},
	{0x11, 0x01, 0x18, 0x08, "1st bonus*2"		},
	{0x11, 0x01, 0x18, 0x10, "1st bonus*3"		},
	{0x11, 0x01, 0x18, 0x18, "1st bonus*4"		},
	{0x11, 0x01, 0x18, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x20, 0x00, "Off"		},
	{0x11, 0x01, 0x20, 0x20, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x40, 0x00, "Upright"		},
	{0x11, 0x01, 0x40, 0x40, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Marvins)

static struct BurnDIPInfo Vangrd2DIPList[]=
{
	{0x10, 0xff, 0xff, 0xbf, NULL		},
	{0x11, 0xff, 0xff, 0xee, NULL		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x10, 0x01, 0x07, 0x00, "6 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x01, "5 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x02, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x03, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x07, 0x07, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x07, 0x06, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x07, 0x05, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x07, 0x04, "1 Coin  5 Credits"		},

	{0   , 0xfe, 0   ,    8, "Bonus Life"		},
	{0x10, 0x01, 0x38, 0x38, "30000"		},
	{0x10, 0x01, 0x38, 0x30, "40000"		},
	{0x10, 0x01, 0x38, 0x28, "50000"		},
	{0x10, 0x01, 0x38, 0x20, "60000"		},
	{0x10, 0x01, 0x38, 0x18, "70000"		},
	{0x10, 0x01, 0x38, 0x10, "80000"		},
	{0x10, 0x01, 0x38, 0x08, "90000"		},
	{0x10, 0x01, 0x38, 0x00, "100000"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x10, 0x01, 0xc0, 0x00, "1"		},
	{0x10, 0x01, 0xc0, 0x40, "2"		},
	{0x10, 0x01, 0xc0, 0x80, "3"		},
	{0x10, 0x01, 0xc0, 0xc0, "5"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x11, 0x01, 0x01, 0x01, "Off"		},
	{0x11, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x11, 0x01, 0x02, 0x02, "Off"		},
	{0x11, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x04, 0x04, "Upright"		},
	{0x11, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x11, 0x01, 0x08, 0x08, "English"		},
	{0x11, 0x01, 0x08, 0x00, "Japanese"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life Occurrence"		},
	{0x11, 0x01, 0x10, 0x00, "Every bonus"		},
	{0x11, 0x01, 0x10, 0x10, "Bonus only"		},

	{0   , 0xfe, 0   ,    2, "Infinite Lives (Cheat)"		},
	{0x11, 0x01, 0x20, 0x20, "Off"		},
	{0x11, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Free Play"		},
	{0x11, 0x01, 0x40, 0x40, "Off"		},
	{0x11, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},
};

STDDIPINFO(Vangrd2)

static struct BurnDIPInfo MadcrashDIPList[]=
{
	{0x12, 0xff, 0xff, 0xfd, NULL		},
	{0x13, 0xff, 0xff, 0x74, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x02, 0x00, "Upright"		},
	{0x12, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x04, 0x04, "3"		},
	{0x12, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x12, 0x01, 0x38, 0x10, "5 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x12, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x12, 0x01, 0xc0, 0xc0, "20000 60000"		},
	{0x12, 0x01, 0xc0, 0x80, "40000 90000"		},
	{0x12, 0x01, 0xc0, 0x40, "50000 120000"		},
	{0x12, 0x01, 0xc0, 0x00, "None"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life Occurrence"		},
	{0x13, 0x01, 0x01, 0x01, "1st, 2nd, then every 2nd"		},
	{0x13, 0x01, 0x01, 0x00, "1st and 2nd only"		},

	{0   , 0xfe, 0   ,    4, "Scroll Speed"		},
	{0x13, 0x01, 0x06, 0x06, "Slow"		},
	{0x13, 0x01, 0x06, 0x04, "Normal"		},
	{0x13, 0x01, 0x06, 0x02, "Fast"		},
	{0x13, 0x01, 0x06, 0x00, "Faster"		},

	{0   , 0xfe, 0   ,    4, "Game mode"		},
	{0x13, 0x01, 0x18, 0x18, "Demo Sounds Off"		},
	{0x13, 0x01, 0x18, 0x10, "Demo Sounds On"		},
	{0x13, 0x01, 0x18, 0x00, "Freeze"		},
	{0x13, 0x01, 0x18, 0x08, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x20, 0x20, "Off"		},
	{0x13, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Difficulty"		},
	{0x13, 0x01, 0x40, 0x40, "Easy"		},
	{0x13, 0x01, 0x40, 0x00, "Hard"		},
};

STDDIPINFO(Madcrash)

static struct BurnDIPInfo JcrossDIPList[]=
{
	{0x10, 0xff, 0xff, 0x3d, NULL		},
	{0x11, 0xff, 0xff, 0xb4, NULL		},
	{0x12, 0xff, 0xff, 0xc1, NULL		},

	{0   , 0xfe, 0   ,    2, "Upright Controls"		},
	{0x10, 0x01, 0x01, 0x01, "Single"		},
	{0x10, 0x01, 0x01, 0x00, "Dual"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x10, 0x01, 0x02, 0x00, "Upright"		},
	{0x10, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x10, 0x01, 0x04, 0x04, "3"		},
	{0x10, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    6, "Coinage"		},
	{0x10, 0x01, 0x38, 0x28, "3 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x10, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x10, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x10, 0x01, 0x38, 0x20, "1 Coin  3 Credits"		},
	{0x10, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x11, 0x01, 0x06, 0x06, "Easy"		},
	{0x11, 0x01, 0x06, 0x04, "Normal"		},
	{0x11, 0x01, 0x06, 0x02, "Hard"		},
	{0x11, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game mode"		},
	{0x11, 0x01, 0x18, 0x18, "Demo Sounds Off"		},
	{0x11, 0x01, 0x18, 0x10, "Demo Sounds On"		},
	{0x11, 0x01, 0x18, 0x00, "Freeze"		},
	{0x11, 0x01, 0x18, 0x08, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x11, 0x01, 0x20, 0x20, "Off"		},
	{0x11, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "No BG Collision (Cheat)"		},
	{0x11, 0x01, 0x80, 0x80, "Off"		},
	{0x11, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x12, 0x01, 0xc1, 0xc1, "20k 60k 40k+"		},
	{0x12, 0x01, 0xc1, 0x81, "40k 120k 80k+"		},
	{0x12, 0x01, 0xc1, 0x41, "60k 160k 100k+"		},
	{0x12, 0x01, 0xc1, 0xc0, "20k"		},
	{0x12, 0x01, 0xc1, 0x80, "40k"		},
	{0x12, 0x01, 0xc1, 0x40, "60k"		},
	{0x12, 0x01, 0xc1, 0x00, "None"		},
};

STDDIPINFO(Jcross)

static struct BurnDIPInfo SgladiatDIPList[]=
{
	{0x11, 0xff, 0xff, 0x3d, NULL		},
	{0x12, 0xff, 0xff, 0xf6, NULL		},
	{0x13, 0xff, 0xff, 0xc1, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x02, 0x00, "Upright"		},
	{0x11, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x04, 0x04, "3"		},
	{0x11, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    7, "Coinage"		},
	{0x11, 0x01, 0x38, 0x10, "5 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    2, "Time"		},
	{0x12, 0x01, 0x02, 0x02, "More"		},
	{0x12, 0x01, 0x02, 0x00, "Less"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x12, 0x01, 0x18, 0x18, "Demo Sounds Off"		},
	{0x12, 0x01, 0x18, 0x10, "Demo Sounds On"		},
	{0x12, 0x01, 0x18, 0x00, "Freeze"		},
	{0x12, 0x01, 0x18, 0x08, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x20, 0x20, "Off"		},
	{0x12, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Unknown"		},
	{0x12, 0x01, 0x40, 0x40, "Off"		},
	{0x12, 0x01, 0x40, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "No Opponents (Cheat)"		},
	{0x12, 0x01, 0x80, 0x80, "Off"		},
	{0x12, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x13, 0x01, 0xc1, 0xc1, "20k 60k 60k+"		},
	{0x13, 0x01, 0xc1, 0x81, "40k 90k 90k+"		},
	{0x13, 0x01, 0xc1, 0x41, "50k 120k 120k+"		},
	{0x13, 0x01, 0xc1, 0xc0, "20k 60k"		},
	{0x13, 0x01, 0xc1, 0x80, "40k 90k"		},
	{0x13, 0x01, 0xc1, 0x40, "50k 120k"		},
	{0x13, 0x01, 0xc1, 0x00, "None"		},
};

STDDIPINFO(Sgladiat)

static struct BurnDIPInfo Hal21DIPList[]=
{
	{0x11, 0xff, 0xff, 0x3f, NULL		},
	{0x12, 0xff, 0xff, 0xf6, NULL		},
	{0x13, 0xff, 0xff, 0xc1, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x11, 0x01, 0x02, 0x02, "Upright"		},
	{0x11, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x11, 0x01, 0x04, 0x04, "3"		},
	{0x11, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    6, "Coinage"		},
	{0x11, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x11, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x11, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x11, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x11, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x12, 0x01, 0x06, 0x06, "Easy"		},
	{0x12, 0x01, 0x06, 0x04, "Normal"		},
	{0x12, 0x01, 0x06, 0x02, "Hard"		},
	{0x12, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game mode"		},
	{0x12, 0x01, 0x18, 0x18, "Demo Sounds Off"		},
	{0x12, 0x01, 0x18, 0x10, "Demo Sounds On"		},
	{0x12, 0x01, 0x18, 0x00, "Freeze"		},
	{0x12, 0x01, 0x18, 0x08, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x12, 0x01, 0x20, 0x20, "Off"		},
	{0x12, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x12, 0x01, 0x80, 0x80, "No"		},
	{0x12, 0x01, 0x80, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x13, 0x01, 0xc1, 0xc1, "20k 60k 60k+"		},
	{0x13, 0x01, 0xc1, 0x81, "40k 90k 90k+"		},
	{0x13, 0x01, 0xc1, 0x41, "50k 120k 120k+"		},
	{0x13, 0x01, 0xc1, 0xc0, "20k 60k"		},
	{0x13, 0x01, 0xc1, 0x80, "40k 90k"		},
	{0x13, 0x01, 0xc1, 0x40, "50k 120k"		},
	{0x13, 0x01, 0xc1, 0x00, "None"		},
};

STDDIPINFO(Hal21)

static struct BurnDIPInfo AsoDIPList[]=
{
	{0x13, 0xff, 0xff, 0x3c, NULL		},
	{0x14, 0xff, 0xff, 0xf7, NULL		},
	{0x14, 0xff, 0xff, 0xc1, NULL		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x13, 0x01, 0x01, 0x01, "No"		},
	{0x13, 0x01, 0x01, 0x00, "3 Times"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x02, 0x00, "Upright"		},
	{0x13, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x04, 0x04, "3"		},
	{0x13, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    8, "Coinage"		},
	{0x13, 0x01, 0x38, 0x20, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x28, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x30, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x13, 0x01, 0x38, 0x18, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0x38, 0x10, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0x38, 0x08, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0x38, 0x00, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x06, 0x06, "Easy"		},
	{0x14, 0x01, 0x06, 0x04, "Normal"		},
	{0x14, 0x01, 0x06, 0x02, "Hard"		},
	{0x14, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x14, 0x01, 0x08, 0x08, "Off"		},
	{0x14, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "All Ships at Start (Cheat)"		},
	{0x14, 0x01, 0x10, 0x10, "Off"		},
	{0x14, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x14, 0x01, 0x20, 0x20, "Off"		},
	{0x14, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Start Area"		},
	{0x14, 0x01, 0xc0, 0xc0, "1"		},
	{0x14, 0x01, 0xc0, 0x80, "2"		},
	{0x14, 0x01, 0xc0, 0x40, "3"		},
	{0x14, 0x01, 0xc0, 0x00, "4"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x15, 0x01, 0xc1, 0xc1, "50k 100k 100k+"		},
	{0x15, 0x01, 0xc1, 0x81, "60k 120k 120k+"		},
	{0x15, 0x01, 0xc1, 0x41, "100k 200k 200k+"		},
	{0x15, 0x01, 0xc1, 0xc0, "50k 100k"		},
	{0x15, 0x01, 0xc1, 0x80, "60k 120k"		},
	{0x15, 0x01, 0xc1, 0x40, "100k 200k"		},
	{0x15, 0x01, 0xc1, 0x00, "None"		},
};

STDDIPINFO(Aso)

static struct BurnDIPInfo AthenaDIPList[]=
{
	{0x12, 0xff, 0xff, 0x39, NULL		},
	{0x13, 0xff, 0xff, 0xcb, NULL		},
	{0x14, 0xff, 0xff, 0x34, NULL		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x12, 0x01, 0x02, 0x00, "Upright"		},
	{0x12, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x12, 0x01, 0x08, 0x08, "3"		},
	{0x12, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x12, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x12, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x12, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x12, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x12, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x12, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x12, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x12, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x13, 0x01, 0x03, 0x03, "Easy"		},
	{0x13, 0x01, 0x03, 0x02, "Normal"		},
	{0x13, 0x01, 0x03, 0x01, "Hard"		},
	{0x13, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x13, 0x01, 0x04, 0x04, "Off"		},
	{0x13, 0x01, 0x04, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Freeze"		},
	{0x13, 0x01, 0x08, 0x08, "Off"		},
	{0x13, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Energy"		},
	{0x13, 0x01, 0x80, 0x80, "12"		},
	{0x13, 0x01, 0x80, 0x00, "14"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x14, 0x01, 0x34, 0x34, "50k 100k 100k+"		},
	{0x14, 0x01, 0x34, 0x24, "60k 120k 120k+"		},
	{0x14, 0x01, 0x34, 0x14, "100k 200k 200k+"		},
	{0x14, 0x01, 0x34, 0x30, "50k 100k"		},
	{0x14, 0x01, 0x34, 0x20, "60k 120k"		},
	{0x14, 0x01, 0x34, 0x10, "100k 200k"		},
	{0x14, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Athena)

static struct BurnDIPInfo Tnk3DIPList[]=
{
	{0x17, 0xff, 0xff, 0x3d, NULL		},
	{0x18, 0xff, 0xff, 0x34, NULL		},
	{0x19, 0xff, 0xff, 0xc1, NULL		},

	{0   , 0xfe, 0   ,    2, "No BG Collision (Cheat)"		},
	{0x17, 0x01, 0x01, 0x01, "Off"		},
	{0x17, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x17, 0x01, 0x02, 0x00, "Upright"		},
	{0x17, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x17, 0x01, 0x04, 0x04, "3"		},
	{0x17, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    6, "Coinage"		},
	{0x17, 0x01, 0x38, 0x20, "3 Coins 1 Credits"		},
	{0x17, 0x01, 0x38, 0x18, "2 Coins 1 Credits"		},
	{0x17, 0x01, 0x38, 0x38, "1 Coin  1 Credits"		},
	{0x17, 0x01, 0x38, 0x30, "1 Coin  2 Credits"		},
	{0x17, 0x01, 0x38, 0x28, "1 Coin  3 Credits"		},
	{0x17, 0x01, 0x38, 0x00, "Free Play"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x18, 0x01, 0x06, 0x06, "Easy"		},
	{0x18, 0x01, 0x06, 0x04, "Normal"		},
	{0x18, 0x01, 0x06, 0x02, "Hard"		},
	{0x18, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x18, 0x01, 0x18, 0x18, "Demo Sounds Off"		},
	{0x18, 0x01, 0x18, 0x10, "Demo Sounds On"		},
	{0x18, 0x01, 0x18, 0x00, "Freeze"		},
	{0x18, 0x01, 0x18, 0x08, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x18, 0x01, 0x20, 0x20, "Off"		},
	{0x18, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x18, 0x01, 0x80, 0x80, "No"		},
	{0x18, 0x01, 0x80, 0x00, "5 Times"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x19, 0x01, 0xc1, 0xc1, "20k 60k 60k+"		},
	{0x19, 0x01, 0xc1, 0x81, "40k 90k 90k+"		},
	{0x19, 0x01, 0xc1, 0x41, "50k 120k 120k+"		},
	{0x19, 0x01, 0xc1, 0xc0, "20k 60k"		},
	{0x19, 0x01, 0xc1, 0x80, "40k 90k"		},
	{0x19, 0x01, 0xc1, 0x40, "50k 120k"		},
	{0x19, 0x01, 0xc1, 0x00, "None"		},
};

STDDIPINFO(Tnk3)

static struct BurnDIPInfo IkariDIPList[]=
{
	{0x18, 0xff, 0xff, 0x3b, NULL		},
	{0x19, 0xff, 0xff, 0x0a, NULL		},
	{0x1a, 0xff, 0xff, 0x34, NULL		},

	{0   , 0xfe, 0   ,    2, "Allow killing each other"		},
	{0x18, 0x01, 0x01, 0x01, "No"		},
	{0x18, 0x01, 0x01, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "P1 & P2 Fire Buttons"		},
	{0x18, 0x01, 0x02, 0x02, "Separate"		},
	{0x18, 0x01, 0x02, 0x00, "Common"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x18, 0x01, 0x08, 0x08, "3"		},
	{0x18, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x18, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x18, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x18, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x18, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x18, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x18, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x18, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x18, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x19, 0x01, 0x03, 0x03, "Easy"		},
	{0x19, 0x01, 0x03, 0x02, "Normal"		},
	{0x19, 0x01, 0x03, 0x01, "Hard"		},
	{0x19, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x19, 0x01, 0x0c, 0x0c, "Demo Sounds Off"		},
	{0x19, 0x01, 0x0c, 0x08, "Demo Sounds On"		},
	{0x19, 0x01, 0x0c, 0x04, "Freeze"		},
	{0x19, 0x01, 0x0c, 0x00, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x19, 0x01, 0x80, 0x80, "No"		},
	{0x19, 0x01, 0x80, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x1a, 0x01, 0x34, 0x34, "50k 100k 100k+"		},
	{0x1a, 0x01, 0x34, 0x24, "60k 120k 120k+"		},
	{0x1a, 0x01, 0x34, 0x14, "100k 200k 200k+"		},
	{0x1a, 0x01, 0x34, 0x30, "50k 100k"		},
	{0x1a, 0x01, 0x34, 0x20, "60k 120k"		},
	{0x1a, 0x01, 0x34, 0x10, "100k 200k"		},
	{0x1a, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Ikari)

static struct BurnDIPInfo VictroadDIPList[]=
{
	{0x19, 0xff, 0xff, 0x3b, NULL		},
	{0x1a, 0xff, 0xff, 0x8b, NULL		},
	{0x1b, 0xff, 0xff, 0x34, NULL		},

	{0   , 0xfe, 0   ,    2, "Kill friend & walk everywhere (Cheat)"		},
	{0x19, 0x01, 0x01, 0x01, "No"		},
	{0x19, 0x01, 0x01, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "P1 & P2 Fire Buttons"		},
	{0x19, 0x01, 0x02, 0x02, "Separate"		},
	{0x19, 0x01, 0x02, 0x00, "Common"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x19, 0x01, 0x08, 0x08, "3"		},
	{0x19, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x19, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x19, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x19, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x19, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x19, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x1a, 0x01, 0x03, 0x03, "Easy"		},
	{0x1a, 0x01, 0x03, 0x02, "Normal"		},
	{0x1a, 0x01, 0x03, 0x01, "Hard"		},
	{0x1a, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x1a, 0x01, 0x0c, 0x0c, "Demo Sounds Off"		},
	{0x1a, 0x01, 0x0c, 0x08, "Demo Sounds On"		},
	{0x1a, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x1a, 0x01, 0x0c, 0x04, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x1a, 0x01, 0x40, 0x40, "No"		},
	{0x1a, 0x01, 0x40, 0x00, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Credits Buy Lives During Play"		},
	{0x1a, 0x01, 0x80, 0x00, "No"		},
	{0x1a, 0x01, 0x80, 0x80, "Yes"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x1b, 0x01, 0x34, 0x34, "50k 100k 100k+"		},
	{0x1b, 0x01, 0x34, 0x24, "60k 120k 120k+"		},
	{0x1b, 0x01, 0x34, 0x14, "100k 200k 200k+"		},
	{0x1b, 0x01, 0x34, 0x30, "50k 100k"		},
	{0x1b, 0x01, 0x34, 0x20, "60k 120k"		},
	{0x1b, 0x01, 0x34, 0x10, "100k 200k"		},
	{0x1b, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Victroad)

static struct BurnDIPInfo FitegolfDIPList[]=
{
	{0x13, 0xff, 0xff, 0x37, NULL		},
	{0x14, 0xff, 0xff, 0xff, NULL		},

	{0   , 0xfe, 0   ,    2, "Language"		},
	{0x13, 0x01, 0x01, 0x01, "English"		},
	{0x13, 0x01, 0x01, 0x00, "Japanese"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x02, 0x02, "Off"		},
	{0x13, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x04, 0x04, "Upright"		},
	{0x13, 0x01, 0x04, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Gameplay"		},
	{0x13, 0x01, 0x08, 0x00, "Basic Player"		},
	{0x13, 0x01, 0x08, 0x08, "Avid Golfer"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    2, "Shot Time"		},
	{0x14, 0x01, 0x01, 0x00, "Short (10 sec)"		},
	{0x14, 0x01, 0x01, 0x01, "Long (12 sec)"		},

	{0   , 0xfe, 0   ,    2, "Bonus Holes"		},
	{0x14, 0x01, 0x02, 0x02, "More (Par 1,Birdie 2,Eagle 3)"		},
	{0x14, 0x01, 0x02, 0x00, "Less (Par 0,Birdie 1,Eagle 2)"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x14, 0x01, 0x0c, 0x04, "Demo Sounds Off"		},
	{0x14, 0x01, 0x0c, 0x0c, "Demo Sounds On"		},
	{0x14, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x14, 0x01, 0x0c, 0x08, "Infinite Holes (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Play Holes"		},
	{0x14, 0x01, 0x30, 0x30, "2"		},
	{0x14, 0x01, 0x30, 0x20, "3"		},
	{0x14, 0x01, 0x30, 0x10, "4"		},
	{0x14, 0x01, 0x30, 0x00, "5"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x40, 0x00, "No"		},
	{0x14, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Service mode"		},
	{0x14, 0x01, 0x80, 0x80, "Off"		},
	{0x14, 0x01, 0x80, 0x00, "On"		},

};

STDDIPINFO(Fitegolf)

static struct BurnDIPInfo AlphamisDIPList[]=
{
	{0x14, 0xff, 0xff, 0x9c, NULL		},
	{0x15, 0xff, 0xff, 0xf6, NULL		},
	{0x16, 0xff, 0xff, 0x01, NULL		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x01, 0x01, "No"		},
	{0x14, 0x01, 0x01, 0x00, "3 Times"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x14, 0x01, 0x02, 0x00, "Upright"		},
	{0x14, 0x01, 0x02, 0x02, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x14, 0x01, 0x04, 0x04, "3"		},
	{0x14, 0x01, 0x04, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x14, 0x01, 0x18, 0x00, "4 Coins 1 Credits"		},
	{0x14, 0x01, 0x18, 0x08, "3 Coins 1 Credits"		},
	{0x14, 0x01, 0x18, 0x10, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x18, 0x18, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x14, 0x01, 0x60, 0x00, "1 Coin  2 Credits"		},
	{0x14, 0x01, 0x60, 0x40, "1 Coin  3 Credits"		},
	{0x14, 0x01, 0x60, 0x20, "1 Coin  4 Credits"		},
	{0x14, 0x01, 0x60, 0x60, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x15, 0x01, 0x06, 0x06, "Easy"		},
	{0x15, 0x01, 0x06, 0x04, "Normal"		},
	{0x15, 0x01, 0x06, 0x02, "Hard"		},
	{0x15, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x15, 0x01, 0x08, 0x08, "Off"		},
	{0x15, 0x01, 0x08, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "All Ships at Start (Cheat)"		},
	{0x15, 0x01, 0x10, 0x10, "Off"		},
	{0x15, 0x01, 0x10, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x15, 0x01, 0x20, 0x20, "Off"		},
	{0x15, 0x01, 0x20, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Start Area"		},
	{0x15, 0x01, 0xc0, 0xc0, "1"		},
	{0x15, 0x01, 0xc0, 0x80, "2"		},
	{0x15, 0x01, 0xc0, 0x40, "3"		},
	{0x15, 0x01, 0xc0, 0x00, "4"		},

	{0   , 0xfe, 0   ,    2, "Bonus Life"		},
	{0x16, 0x01, 0x01, 0x01, "50k 100k 100k+"		},
	{0x16, 0x01, 0x01, 0x00, "50k 100k"		},
};

STDDIPINFO(Alphamis)

static struct BurnDIPInfo ChopperDIPList[]=
{
	{0x13, 0xff, 0xff, 0x3b, NULL		},
	{0x14, 0xff, 0xff, 0xcf, NULL		},
	{0x15, 0xff, 0xff, 0x30, NULL		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"		},
	{0x13, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x02, 0x02, "Upright"		},
	{0x13, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x08, 0x08, "3"		},
	{0x13, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"		},
	{0x14, 0x01, 0x03, 0x03, "Normal"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"		},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x14, 0x01, 0x0c, 0x08, "Demo Sounds Off"		},
	{0x14, 0x01, 0x0c, 0x0c, "Demo Sounds On"		},
	{0x14, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x14, 0x01, 0x0c, 0x04, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x40, 0x00, "No"		},
	{0x14, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x14, 0x01, 0x80, 0x80, "Off"		},
	{0x14, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x15, 0x01, 0x34, 0x30, "50k 100k 100k+"		},
	{0x15, 0x01, 0x34, 0x20, "75k 150k 150k+"		},
	{0x15, 0x01, 0x34, 0x10, "100k 200k 200k+"		},
	{0x15, 0x01, 0x34, 0x34, "50k 100k"		},
	{0x15, 0x01, 0x34, 0x24, "75k 150k"		},
	{0x15, 0x01, 0x34, 0x14, "100k 200k"		},
	{0x15, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Chopper)

static struct BurnDIPInfo ChopperaDIPList[]=
{
	{0x13, 0xff, 0xff, 0x3b, NULL		},
	{0x14, 0xff, 0xff, 0xcf, NULL		},
	{0x15, 0xff, 0xff, 0x30, NULL		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x13, 0x01, 0x01, 0x01, "Off"		},
	{0x13, 0x01, 0x01, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Cabinet"		},
	{0x13, 0x01, 0x02, 0x02, "Upright"		},
	{0x13, 0x01, 0x02, 0x00, "Cocktail"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x13, 0x01, 0x08, 0x08, "3"		},
	{0x13, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x13, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x13, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x13, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x13, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x13, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x13, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x14, 0x01, 0x03, 0x02, "Easy"		},
	{0x14, 0x01, 0x03, 0x03, "Normal"		},
	{0x14, 0x01, 0x03, 0x01, "Hard"		},
	{0x14, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x14, 0x01, 0x0c, 0x08, "Demo Sounds Off"		},
	{0x14, 0x01, 0x0c, 0x0c, "Demo Sounds On"		},
	{0x14, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x14, 0x01, 0x0c, 0x04, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    2, "Allow Continue"		},
	{0x14, 0x01, 0x40, 0x00, "No"		},
	{0x14, 0x01, 0x40, 0x40, "Yes"		},

	{0   , 0xfe, 0   ,    2, "Invulnerability (Cheat)"		},
	{0x14, 0x01, 0x80, 0x80, "Off"		},
	{0x14, 0x01, 0x80, 0x00, "On"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x15, 0x01, 0x34, 0x30, "50k 100k 100k+"		},
	{0x15, 0x01, 0x34, 0x20, "75k 150k 150k+"		},
	{0x15, 0x01, 0x34, 0x10, "100k 200k 200k+"		},
	{0x15, 0x01, 0x34, 0x34, "50k 100k"		},
	{0x15, 0x01, 0x34, 0x24, "75k 150k"		},
	{0x15, 0x01, 0x34, 0x14, "100k 200k"		},
	{0x15, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Choppera)

static struct BurnDIPInfo BermudatDIPList[]=
{
	{0x19, 0xff, 0xff, 0x3a, NULL		},
	{0x1a, 0xff, 0xff, 0x8a, NULL		},
	{0x1b, 0xff, 0xff, 0x34, NULL		},

	{0   , 0xfe, 0   ,    2, "Flip Screen"		},
	{0x19, 0x01, 0x02, 0x02, "Off"		},
	{0x19, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    2, "Lives"		},
	{0x19, 0x01, 0x08, 0x08, "3"		},
	{0x19, 0x01, 0x08, 0x00, "5"		},

	{0   , 0xfe, 0   ,    4, "Coin A"		},
	{0x19, 0x01, 0x30, 0x00, "4 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x10, "3 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x19, 0x01, 0x30, 0x30, "1 Coin  1 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"		},
	{0x19, 0x01, 0xc0, 0x00, "1 Coin  2 Credits"		},
	{0x19, 0x01, 0xc0, 0x40, "1 Coin  3 Credits"		},
	{0x19, 0x01, 0xc0, 0x80, "1 Coin  4 Credits"		},
	{0x19, 0x01, 0xc0, 0xc0, "1 Coin  6 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x1a, 0x01, 0x03, 0x03, "Easy"		},
	{0x1a, 0x01, 0x03, 0x02, "Normal"		},
	{0x1a, 0x01, 0x03, 0x01, "Hard"		},
	{0x1a, 0x01, 0x03, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Game Mode"		},
	{0x1a, 0x01, 0x0c, 0x0c, "Demo Sounds Off"		},
	{0x1a, 0x01, 0x0c, 0x08, "Demo Sounds On"		},
	{0x1a, 0x01, 0x0c, 0x00, "Freeze"		},
	{0x1a, 0x01, 0x0c, 0x04, "Infinite Lives (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Game Style"		},
	{0x1a, 0x01, 0xc0, 0xc0, "Normal without continue"		},
	{0x1a, 0x01, 0xc0, 0x80, "Normal with continue"		},
	{0x1a, 0x01, 0xc0, 0x40, "Time attack 3 minutes"		},
	{0x1a, 0x01, 0xc0, 0x00, "Time attack 5 minutes"		},

	{0   , 0xfe, 0   ,    7, "Bonus Life"		},
	{0x1b, 0x01, 0x34, 0x34, "50k 100k 100k+"		},
	{0x1b, 0x01, 0x34, 0x24, "60k 120k 120k+"		},
	{0x1b, 0x01, 0x34, 0x14, "100k 200k 200k+"		},
	{0x1b, 0x01, 0x34, 0x30, "50k 100k"		},
	{0x1b, 0x01, 0x34, 0x20, "60k 120k"		},
	{0x1b, 0x01, 0x34, 0x10, "100k 200k"		},
	{0x1b, 0x01, 0x34, 0x00, "None"		},
};

STDDIPINFO(Bermudat)

// SNKWAVE code

#define CLOCK_SHIFT 8
#define SNKWAVE_WAVEFORM_LENGTH 16

// data about the sound system
UINT32 snkwave_frequency;
UINT32 snkwave_counter;
INT32 snkwave_waveform_position;
// decoded waveform table
INT16 snkwave_waveform[SNKWAVE_WAVEFORM_LENGTH];
double snkwave_volume = 1.00;

static void snkwave_reset()
{
	/* reset all the voices */
	snkwave_frequency = 0;
	snkwave_counter = 0;
	snkwave_waveform_position = 0;
	memset(&snkwave_waveform, 0, sizeof(snkwave_waveform));
}

static void snkwave_render(INT16 *buffer, INT32 samples)
{
	/* if no sound, we're done */
	if (snkwave_frequency == 0xfff)
		return;

	/* generate sound into buffer while updating the counter */
	while (samples-- > 0)
	{
		INT32 loops;
		INT16 out = 0;

		loops = 1 << CLOCK_SHIFT;
		while (loops > 0)
		{
			int steps = 0x1000 - snkwave_counter;

			if (steps <= loops)
			{
				out += snkwave_waveform[snkwave_waveform_position] * steps;
				snkwave_counter = snkwave_frequency;
				snkwave_waveform_position = (snkwave_waveform_position + 1) & (SNKWAVE_WAVEFORM_LENGTH-1);
				loops -= steps;
			}
			else
			{
				out += snkwave_waveform[snkwave_waveform_position] * loops;
				snkwave_counter += loops;
				loops = 0;
			}
		}

		*buffer = BURN_SND_CLIP(*buffer + (INT16)(out * snkwave_volume));
		buffer++;
		*buffer = BURN_SND_CLIP(*buffer + (INT16)(out * snkwave_volume));
		buffer++;
	}
}

static void snkwave_update_waveform(UINT32 offset, UINT8 data)
{
	snkwave_waveform[offset * 2]     = ((data & 0x38) >> 3) << (12-CLOCK_SHIFT);
	snkwave_waveform[offset * 2 + 1] = ((data & 0x07) >> 0) << (12-CLOCK_SHIFT);
	snkwave_waveform[SNKWAVE_WAVEFORM_LENGTH-2 - offset * 2] = ~snkwave_waveform[offset * 2 + 1];
	snkwave_waveform[SNKWAVE_WAVEFORM_LENGTH-1 - offset * 2] = ~snkwave_waveform[offset * 2];
}

static void snkwave_w(UINT32 offset, UINT8 data)
{
	data &= 0x3f; // all registers are 6-bit

	if (offset == 0)
		snkwave_frequency = (snkwave_frequency & 0x03f) | (data << 6);
	else if (offset == 1)
		snkwave_frequency = (snkwave_frequency & 0xfc0) | data;
	else if (offset <= 5)
		snkwave_update_waveform(offset - 2, data);
}

#undef CLOCK_SHIFT
#undef SNKWAVE_WAVEFORM_LENGTH

// Rotation-handler code

static void RotateReset() {
	for (INT32 playernum = 0; playernum < 2; playernum++) {
		nRotate[playernum] = 0; // start out pointing straight up (0=up)
		nRotateTarget[playernum] = -1;
		nRotateTime[playernum] = 0;
	}
}

static UINT32 RotationTimer(void) {
    return nCurrentFrame;
}

static void RotateRight(INT32 *v) {
    (*v)--;
    if (*v < 0) *v = 11;
}

static void RotateLeft(INT32 *v) {
    (*v)++;
    if (*v > 11) *v = 0;
}

static UINT8 Joy2Rotate(UINT8 *joy) { // ugly code, but the effect is awesome. -dink
	if (joy[0] && joy[2]) return 7;    // up left
	if (joy[0] && joy[3]) return 1;    // up right

	if (joy[1] && joy[2]) return 5;    // down left
	if (joy[1] && joy[3]) return 3;    // down right

	if (joy[0]) return 0;    // up
	if (joy[1]) return 4;    // down
	if (joy[2]) return 6;    // left
	if (joy[3]) return 2;    // right

	return 0xff;
}

static int dialRotation(INT32 playernum) {
    // p1 = 0, p2 = 1
	UINT8 player[2] = { 0, 0 };
	static UINT8 lastplayer[2][2] = { { 0, 0 }, { 0, 0 } };

    if ((playernum != 0) && (playernum != 1)) {
        bprintf(PRINT_NORMAL, _T("Strange Rotation address => %06X\n"), playernum);
        return 0;
    }
    if (playernum == 0) {
        player[0] = DrvFakeInput[0]; player[1] = DrvFakeInput[1];
    }
    if (playernum == 1) {
        player[0] = DrvFakeInput[2]; player[1] = DrvFakeInput[3];
    }

    if (player[0] && (player[0] != lastplayer[playernum][0] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
		RotateLeft(&nRotate[playernum]);
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Left => %06X\n"), playernum+1, nRotate[playernum]);
		nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
    }

	if (player[1] && (player[1] != lastplayer[playernum][1] || (RotationTimer() > nRotateTime[playernum]+0xf))) {
        RotateRight(&nRotate[playernum]);
        //bprintf(PRINT_NORMAL, _T("Player %d Rotate Right => %06X\n"), playernum+1, nRotate[playernum]);
        nRotateTime[playernum] = RotationTimer();
		nRotateTarget[playernum] = -1;
	}

	lastplayer[playernum][0] = player[0];
	lastplayer[playernum][1] = player[1];

    return (nRotate[playernum]);
}

static UINT8 *rotate_gunpos[2] = {NULL, NULL};
static UINT8 rotate_gunpos_multiplier = 1;

// Gun-rotation memory locations - do not remove this tag. - dink :)
// game      p1   p2    clockwise value in memory
// victroad: fdb6 fe06  0 2 4 6 8 a c e
// ikari   : fdb6 fe06  0 2 4 6 8 a c e
// tnk3    : fd43 fd89  0 1 2 3 4 5 6 7   // fd47 fd8d?
// gwar    : e3d3 e437  0 2 4 6 8 a c e
// bermudat: e041 e055  0 1 2 3 4 5 6 7

static void RotateSetGunPosRAM(UINT8 *p1, UINT8 *p2, UINT8 multiplier) {
	rotate_gunpos[0] = p1;
	rotate_gunpos[1] = p2;
	rotate_gunpos_multiplier = multiplier;
}

static INT32 get_distance(INT32 from, INT32 to) {
// this function finds the easiest way to get from "from" to "to", wrapping at 0 and 7
	INT32 countA = 0;
	INT32 countB = 0;
	INT32 fromtmp = from / rotate_gunpos_multiplier;
	INT32 totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp++;
		countA++;
		if(fromtmp>7) fromtmp = 0;
		if(fromtmp == totmp || countA > 32) break;
	}

	fromtmp = from / rotate_gunpos_multiplier;
	totmp = to / rotate_gunpos_multiplier;

	while (1) {
		fromtmp--;
		countB++;
		if(fromtmp<0) fromtmp = 7;
		if(fromtmp == totmp || countB > 32) break;
	}

	if (countA > countB) {
		return 1; // go negative
	} else {
		return 0; // go positive
	}
}

static void RotateDoTick() {
	// since the game only allows for 1 rotation every other frame, we have to
	// do this.
	if (nCurrentFrame&1) return;

	for (INT32 i = 0; i < 2; i++) {
		if (rotate_gunpos[i] && (nRotateTarget[i] != -1) && (nRotateTarget[i] != (*rotate_gunpos[i] & 0x0f))) {
			if (get_distance(nRotateTarget[i], *rotate_gunpos[i] & 0x0f)) {
				RotateRight(&nRotate[i]); // --
			} else {
				RotateLeft(&nRotate[i]);  // ++
			}
			bprintf(0, _T("p%X target %X mempos %X nRotate %X.\n"), i, nRotateTarget[0], *rotate_gunpos[0] & 0x0f, nRotate[0]);
			nRotateTry[i]++;
			if (nRotateTry[i] > 10) nRotateTarget[i] = -1; // don't get stuck in a loop if something goes horribly wrong here.
		} else {
			nRotateTarget[i] = -1;
		}
	}
}

static void SuperJoy2Rotate() {
	for (INT32 i = 0; i < 2; i++) { // p1 = 0, p2 = 1
		if (DrvFakeInput[4 + i]) { //  rotate-button had been pressed
			UINT8 rot = Joy2Rotate(((!i) ? &DrvJoy2[0] : &DrvJoy3[0]));

			if (rot != 0xff) {
				nRotateTarget[i] = rot * rotate_gunpos_multiplier;
			}
			DrvInputs[1 + i] &= 0xc; // cancel out directionals since they are used to rotate here.
			nRotateTry[i] = 0;
		}

		DrvInputs[1 + i] = (DrvInputs[1 + i] & 0x0f) + (dialRotation(i) << 4);
	}

	RotateDoTick();
}

// end Rotation-handler

enum
{
	YM1IRQ_ASSERT=0,
	YM1IRQ_CLEAR,
	YM2IRQ_ASSERT,
	YM2IRQ_CLEAR,
	CMDIRQ_BUSY_ASSERT,
	BUSY_CLEAR,
	CMDIRQ_CLEAR
};

static void sgladiat_sndirq_update_callback(INT32 param)
{
	switch (param)
	{
		case CMDIRQ_BUSY_ASSERT:
			sound_status |= 8|4;
			break;

		case BUSY_CLEAR:
			sound_status &= ~4;
			break;

		case CMDIRQ_CLEAR:
			sound_status &= ~8;
			break;
	}

	ZetSetIRQLine(0x20, (sound_status & 0x08) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static void sndirq_update_callback(INT32 param)
{
	switch (param)
	{
		case YM1IRQ_ASSERT:
			sound_status |= 1;
			break;

		case YM1IRQ_CLEAR:
			sound_status &= ~1;
			break;

		case YM2IRQ_ASSERT:
			sound_status |= 2;
			break;

		case YM2IRQ_CLEAR:
			sound_status &= ~2;
			break;

		case CMDIRQ_BUSY_ASSERT:
			sound_status |= 8|4;
			break;

		case BUSY_CLEAR:
			sound_status &= ~4;
			break;

		case CMDIRQ_CLEAR:
			sound_status &= ~8;
			break;
	}

	ZetSetIRQLine(0, (sound_status & 0xb) ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
}

static inline UINT8 nmi_trigger_read()
{
	INT32 nActive = ZetGetActive();

	ZetClose();
	ZetOpen(nActive^1);
	ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
	ZetClose();
	ZetOpen(nActive);

	return 0xff;
}

static inline UINT8 common_read(UINT16 offset, UINT16 real_address, INT32 sound_status_val, INT32 sound_status_select, UINT16 hi_dips)
{
	if (hi_dips) hi_dips = 0x0100;

	if ((offset + 0x0000) == real_address) {
		return (DrvInputs[0] & ~sound_status_val) | ((sound_status & sound_status_select) ? sound_status_val : 0);
	}

	if ((offset + 0x0100) == real_address) return DrvInputs[1];
	if ((offset + 0x0200) == real_address) return DrvInputs[2];
	if ((offset + 0x0300) == real_address) return DrvInputs[3];
	if ((offset + 0x0400 + hi_dips) == real_address) {
		INT32 dip = DrvDips[0] & (~(bonus_dip_config & 0xff));
		return dip | (DrvDips[2] & (bonus_dip_config & 0xff));
	}
	if ((offset + 0x0500 + hi_dips) == real_address) {
		INT32 dip = DrvDips[1] & (~(bonus_dip_config >> 8));
		return dip | (DrvDips[2] & (bonus_dip_config >> 8));
	}
	if ((offset + 0x0700) == real_address) return nmi_trigger_read();

	return 0;
}

static void __fastcall bermudat_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("%d, W: %4.4x, %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xca00:
			tc16_posy = (tc16_posy & ~0xff) | data;
		return;

		case 0xca40:
			tc16_posx = (tc16_posx & ~0xff) | data;
		return;

		case 0xcc00:
			tc32_posy = (tc32_posy & ~0xff) | data;
		return;

		case 0xcc40:
			tc32_posx = (tc32_posx & ~0xff) | data;
		return;

		case 0xcc80:
		{
			tc16_posx = (tc16_posx & 0xff) | ((data & 0x80) << 1);
			tc16_posy = (tc16_posy & 0xff) | ((data & 0x40) << 2);
			tc32_posx = (tc32_posx & 0xff) | ((data & 0x80) << 1);
			tc32_posy = (tc32_posy & 0xff) | ((data & 0x40) << 2);
		}
		return;

		case 0xc300:
			// coin counter
		return;

		case 0xc400:
		{
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xc800:
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xc840:
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;

		case 0xc880:
		{
			flipscreen = data & 0x10;
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x02) << 7);
			bg_scrolly = (bg_scrolly & 0xff) | ((data & 0x01) << 8);
		}
		return;

		case 0xc8c0:
		{
			txt_palette_offset = (data & 0xf) << 4;
			txt_tile_offset = (data & 0x30) << 4;
			if (game_select == 1) bg_palette_offset = (data & 0x80);		// psychos only!!!!
		}
		return;

		case 0xc900:
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
		return;

		case 0xc940:
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
		return;

		case 0xc980:
			sp32_scrolly = (sp32_scrolly & 0x100) | data;
		return;

		case 0xc9c0:
			sp32_scrollx = (sp32_scrollx & 0x100) | data;
		return;

		case 0xca80:
		{
			sp32_scrollx = (sp32_scrollx & 0xff) | ((data & 0x20) << 3);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x10) << 4);
			sp32_scrolly = (sp32_scrolly & 0xff) | ((data & 0x08) << 5);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x04) << 6);
		}
		return;

		case 0xcac0:
			sprite_split_point = data;
		return;
	}
}

static INT32 turbofront_check(int small, int num)
{
	const UINT8 *sr = &DrvSprRAM[0x800*small + 4*num];
	INT32 x = sr[2] + ((sr[3] & 0x80) << 1);
	INT32 y = sr[0] + ((sr[3] & 0x10) << 4);

	INT32 dx = (x - (small ? tc16_posx : tc32_posx)) & 0x1ff;
	INT32 dy = (y - (small ? tc16_posy : tc32_posy)) & 0x1ff;

	if (dx > 0x20 && dx <= 0x1e0 && dy > 0x20 && dy <= 0x1e0)
		return 0;
	else
		return 1;
}

static INT32 turbofront_check8(int small, int num)
{
	return	(turbofront_check(small, num + 0) << 0) |
		(turbofront_check(small, num + 1) << 1) |
		(turbofront_check(small, num + 2) << 2) |
		(turbofront_check(small, num + 3) << 3) |
		(turbofront_check(small, num + 4) << 4) |
		(turbofront_check(small, num + 5) << 5) |
		(turbofront_check(small, num + 6) << 6) |
		(turbofront_check(small, num + 7) << 7);
}

static UINT8 __fastcall bermudat_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	if ((address & 0xff8f) == 0xcb00) {
		return turbofront_check8(1, ((address >> 4) & 7)*8);
	}

	if ((address & 0xffcf) == 0xccc0) {
		return turbofront_check8(0, ((address >> 4) & 3)*8);
	}

	return common_read(0xc000, address, 0x01, 0x04, 1);
}
		
static void __fastcall bermudat_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc300: // these should be written by the main cpu only!
		case 0xc400:
		case 0xcac0:
		return;
	}

	bermudat_main_write(address, data);
}


static UINT8 __fastcall bermudat_sub_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	return common_read(0xc000, address, 0x01, 0x04, 1);
}

static void __fastcall gwar_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("%d, W: %4.4x, %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xc300:
			// coin counter
		return;

		case 0xc400:
		{
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE); // nmi_ack
		return;

		case 0xc800:
		case 0xf800: // gwara
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xc840:
		case 0xf840: // gwara
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;

		case 0xc880: // gwar only!
		{
			flipscreen = data & 0x04;

			sp32_scrollx = (sp32_scrollx & 0xff) | ((data & 0x80) << 1);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x40) << 2);
			sp32_scrolly = (sp32_scrolly & 0xff) | ((data & 0x20) << 3);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x10) << 4);
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x02) << 7);
			bg_scrolly = (bg_scrolly & 0xff) | ((data & 0x01) << 8);
		}
		return;

		case 0xc8c0:
		case 0xf8c0: // gwara
		{
			txt_palette_offset = (data & 0xf) << 4;
			txt_tile_offset = (data & 0x30) << 4;
			if (game_select == 1) bg_palette_offset = (data & 0x80);		// psychos only!!!!
		}
		return;

		case 0xc900:
		case 0xf900: // gwara
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
		return;

		case 0xc940:
		case 0xf940: // gwara
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
		return;

		case 0xc980:
		case 0xf980: // gwara
			sp32_scrolly = (sp32_scrolly & 0x100) | data;
		return;

		case 0xc9c0:
		case 0xf9c0: // gwara
			sp32_scrollx = (sp32_scrollx & 0x100) | data;
		return;

		case 0xcac0:
		case 0xfac0: // gwara
			sprite_split_point = data;
		return;

		case 0xf880: // gwara only!
			flipscreen = data & 0x10;
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x02) << 7);
			bg_scrolly = (bg_scrolly & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xfa80: // gwara only!
			sp32_scrollx = (sp32_scrollx & 0xff) | ((data & 0x20) << 3);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x10) << 4);
			sp32_scrolly = (sp32_scrolly & 0xff) | ((data & 0x08) << 5);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x04) << 6);
		return;
	}
}

static void __fastcall gwar_sub_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xc000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xc8c0:
		case 0xf8c0: // gwara
			gwar_main_write(address,data);
		return;
	}
}

static UINT8 __fastcall gwar_sub_read(UINT16 address)
{
	switch (address)
	{
		case 0xc000:
			return nmi_trigger_read();
	}

	return 0;
}

static void __fastcall tnk3_main_write(UINT16 address, UINT8 data)
{
	//bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xc300:
			// coin counter
		return;

		case 0xc400:
		{
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE); // nmi_ack
		return;

		case 0xc800:
		{
			flipscreen = data & 0x80;
			txt_tile_offset = ((data & 0x40) << 2);
			//bprintf(0, _T("0x20 = %X."), data & 0x20);
			//bprintf(0, _T("txttile = %X."), txt_tile_offset);

			bg_scrolly   = (bg_scrolly   & 0xff) | ((data & 0x10) << 4);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x08) << 5);
			bg_scrollx   = (bg_scrollx   & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		}
		return;

		case 0xc900:
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
			//if (sp16_scrolly == 0)
			   // bprintf(0, _T("sp16sy %X."), sp16_scrolly);
		return;

		case 0xca00:
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
			//if (sp16_scrollx == 0)
			 //   bprintf(0, _T("sp16sx %X."), sp16_scrollx);
		return;

		case 0xcb00:
			bg_scrolly = (bg_scrolly & 0x100) | data;
			//if (bg_scrolly == 0) bprintf(0, _T("bsy %X. "), bg_scrolly);
		return;

		case 0xcc00:
			bg_scrollx = (bg_scrollx & 0x100) | data;
			//if (bg_scrollx == 0) bprintf(0, _T("bsx %X. "), bg_scrollx);
		return;
	}
}

static UINT8 __fastcall tnk3_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	return common_read(0xc000, address, 0x20, 0x04, 1);
}

static UINT8 __fastcall fitegolf_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	return common_read(0xc000, address, 0x01, 0x04, 1);
}

static UINT8 __fastcall hal21_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	return common_read(0xc000, address, 0x20, 0x04, 0);
}


static UINT8 __fastcall athena_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	return common_read(0xc000, address, 0x01, 0x04, 1);
}

static void __fastcall tnk3_sub_write(UINT16 address, UINT8 /*data*/)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);


	switch (address)
	{
		case 0xc000:
		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;
	}
}


static UINT8 __fastcall tnk3_sub_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	switch (address)
	{
		case 0xc000:
		case 0xc700:
			return nmi_trigger_read();
	}

	return 0;
}

static void __fastcall aso_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xc400:
		{
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE); // nmi_ack
		return;

		case 0xc800:
		{
			flipscreen = data & 0x20;

			bg_scrolly   = (bg_scrolly   & 0xff) | ((data & 0x10) << 4);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x08) << 5);
			bg_scrollx   = (bg_scrollx   & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		}
		return;

		case 0xc900:
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
		return;

		case 0xca00:
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
		return;

		case 0xcb00:
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xcc00:
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;

		case 0xcf00:
			bg_palette_offset = ((data&0xf)^8)<<4;
			bg_tile_offset = ((data & 0x30) << 4);
		return;
	}
}


static void  __fastcall marvins_ab_write(UINT16 address, UINT8 data)
{
	switch (address & ~0xff)
	{
	// CPU A only
		case 0x6000: // palette_bank
			bg_palette_offset = (data & 0x70);
			fg_palette_offset = (data & 0x07) << 4;	
		return;

		case 0x8300:
			if (ZetGetActive() == 0) {
				sound_status = 1;
				soundlatch = data;
				ZetClose();
				ZetOpen(2);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
				ZetClose();
				ZetOpen(0);
			}
		return;

		case 0x8600:
			flipscreen = data & 0x01;
		return;

		case 0x8700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

	// shared
		case 0xf800:
			sp16_scrolly = (sp16_scrolly & 0xff00) | data;
		return;

		case 0xf900:
			sp16_scrollx = (sp16_scrollx & 0xff00) | data;
		return;

		case 0xfa00:
			fg_scrolly = (fg_scrolly & 0xff00) | data;
		return;

		case 0xfb00:
			fg_scrollx = (fg_scrollx & 0xff00) | data;
		return;

		case 0xfc00:
			bg_scrolly = (bg_scrolly & 0xff00) | data;
		return;

		case 0xfd00:
			bg_scrollx = (bg_scrollx & 0xff00) | data;
		return;

		case 0xfe00:
			sprite_split_point = data;
		return;

		case 0xff00:
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x04) << 6);
			fg_scrollx = (fg_scrollx & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		return;
	}
}

static UINT8 __fastcall marvins_ab_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	return common_read(0x8000, address, 0x60, 0xff, 0);
}


static void __fastcall madcrash_main_write(UINT16 address, UINT8 data)
{
	switch (address & ~0xff)
	{
		case 0x8300:
			if (ZetGetActive() == 0) {
				sound_status = 1;
				soundlatch = data;
				ZetClose();
				ZetOpen(2);
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
				ZetClose();
				ZetOpen(0);
			}
		return;

		case 0x8600:
			flipscreen = data & 0x01;
		return;

		case 0x8700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xc800: // palette_bank
			bg_palette_offset = (data & 0x70);
			fg_palette_offset = (data & 0x07) << 4;	
		return;

		case 0xf800:
			bg_scrolly = (bg_scrolly & 0xff00) | data;
		return;

		case 0xf900:
			bg_scrollx = (bg_scrollx & 0xff00) | data;
		return;

		case 0xfa00:
			sprite_split_point = data;
		return;

		case 0xfb00:
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x04) << 6);
			fg_scrollx = (fg_scrollx & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xfc00:
			sp16_scrolly = (sp16_scrolly & 0xff00) | data;
		return;

		case 0xfd00:
			sp16_scrollx = (sp16_scrollx & 0xff00) | data;
		return;

		case 0xfe00:
			fg_scrolly = (fg_scrolly & 0xff00) | data;
		return;

		case 0xff00:
			fg_scrollx = (fg_scrollx & 0xff00) | data;
		return;
	}
}

static void __fastcall madcrash_sub_write(UINT16 address, UINT8 data)
{
	switch (address & ~0xff)
	{
		case 0x8700: // vangrd2
		case 0xa000: // madcrash
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xd800:
			bg_scrolly = (bg_scrolly & 0xff00) | data;
		return;

		case 0xd900:
			bg_scrollx = (bg_scrollx & 0xff00) | data;
		return;

		case 0xda00:
			sprite_split_point = data;
		return;

		case 0xdb00:
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x04) << 6);
			fg_scrollx = (fg_scrollx & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xdc00:
			sp16_scrolly = (sp16_scrolly & 0xff00) | data;
		return;

		case 0xdd00:
			sp16_scrollx = (sp16_scrollx & 0xff00) | data;
		return;

		case 0xde00:
			fg_scrolly = (fg_scrolly & 0xff00) | data;
		return;

		case 0xdf00:
			fg_scrollx = (fg_scrollx & 0xff00) | data;
		return;
	}
}

static void __fastcall madcrush_ab_write(UINT16 address, UINT8 data)
{
	switch (address & ~0xff)
	{
		case 0x8300:
			if (ZetGetActive() == 0) {
				sound_status = 1;
				soundlatch = data;
				ZetClose();
				ZetOpen(2);
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
				ZetClose();
				ZetOpen(0);
			}
		return;

		case 0x8600:
			flipscreen = data & 0x01;
		return;

		case 0x8700:
		case 0xa000: // sub
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xc800: // palette_bank
			bg_palette_offset = (data & 0x70);
			fg_palette_offset = (data & 0x07) << 4;	
		return;

		case 0xf800:
			sp16_scrolly = (sp16_scrolly & 0xff00) | data;
		return;

		case 0xf900:
			sp16_scrollx = (sp16_scrollx & 0xff00) | data;
		return;

		case 0xfa00:
			fg_scrolly = (fg_scrolly & 0xff00) | data;
		return;

		case 0xfb00:
			fg_scrollx = (fg_scrollx & 0xff00) | data;
		return;

		case 0xfc00:
			bg_scrolly = (bg_scrolly & 0xff00) | data;
		return;

		case 0xfd00:
			bg_scrollx = (bg_scrollx & 0xff00) | data;
		return;

		case 0xfe00:
			sprite_split_point = data;
		return;

		case 0xff00:
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x04) << 6);
			fg_scrollx = (fg_scrollx & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		return;
	}
}

static UINT8 __fastcall jcross_main_read(UINT16 address)
{
//	bprintf (0, _T("R C%d, A: %4.4x\n"), ZetGetActive(), address);

	return common_read(0xa000, address, 0x20, 0xff, 0);
}

static void __fastcall jcross_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xa300:
			if (ZetGetActive() != 0) return;
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sgladiat_sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xa600:
			flipscreen = data & 0x80;
			bg_palette_offset = ((data & 0xf) ^ 8) << 4;
		return;

		case 0xa700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xd300:
			bg_scrolly =   (bg_scrolly   & 0xff) | ((data & 0x10) << 4);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x08) << 5);
			bg_scrollx =   (bg_scrollx   & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xd400:
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
		return;

		case 0xd500:
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
		return;

		case 0xd600:
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xd700:
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;
	}
}

static void __fastcall sgladiat_sub_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xa000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xa600:
			flipscreen = data & 0x80;
			bg_palette_offset = ((data & 0xf) ^ 8) << 4;
		return;

		case 0xdb00:
			bg_scrolly =   (bg_scrolly   & 0xff) | ((data & 0x10) << 4);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x08) << 5);
			bg_scrollx =   (bg_scrollx   & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xdc00:
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
		return;

		case 0xdd0:
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
		return;

		case 0xde00:
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xdf00:
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;
	}
}

static void __fastcall hal21_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xc300:
			if (ZetGetActive() != 0) return;
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sgladiat_sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		return;

		case 0xc600:
			flipscreen = data & 0x80;
			bg_tile_offset = ((data & 0x20) << 3);
			bg_palette_offset = (((data & 0xf) ^ 8) << 4);
		return;

		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xd300:
			bg_scrolly =   (bg_scrolly   & 0xff) | ((data & 0x10) << 4);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x08) << 5);
			bg_scrollx =   (bg_scrollx   & 0xff) | ((data & 0x02) << 7);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xd400:
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
		return;

		case 0xd500:
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
		return;

		case 0xd600:
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xd700:
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;
	}
}

static void __fastcall hal21_sub_write(UINT16 address, UINT8 /*data*/)
{
	switch (address)
	{
		case 0xa000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;
	}
}

static void __fastcall ikari_main_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xc300:
			// coin counter
		return;

		case 0xc400:
		{
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xc800:
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xc880:
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;

		case 0xc900:
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x02) << 7);
			bg_scrolly = (bg_scrolly & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xc980:
			txt_palette_offset = ((data & 0x01) << 4);
			txt_tile_offset = ((data & 0x10) << 4);
		return;

		case 0xca00:
			sp16_scrolly = (sp16_scrolly & 0x100) | data;
		return;

		case 0xca80:
			sp16_scrollx = (sp16_scrollx & 0x100) | data;
		return;

		case 0xcb00:
			sp32_scrolly = (sp32_scrolly & 0x100) | data;
		return;

		case 0xcb80:
			sp32_scrollx = (sp32_scrollx & 0x100) | data;
		return;


		case 0xcc00:
			tc16_posy = (tc16_posy & ~0xff) | data;
		return;

		case 0xcc80:
			tc16_posx = (tc16_posx & ~0xff) | data;
		return;

		case 0xcd00:
			sp32_scrollx = (sp32_scrollx & 0xff) | ((data & 0x20) << 3);
			sp16_scrollx = (sp16_scrollx & 0xff) | ((data & 0x10) << 4);
			sp32_scrolly = (sp32_scrolly & 0xff) | ((data & 0x08) << 5);
			sp16_scrolly = (sp16_scrolly & 0xff) | ((data & 0x04) << 6);
		return;

		case 0xcd80:
		{
			tc16_posx = (tc16_posx & 0xff) | ((data & 0x80) << 1);
			tc16_posy = (tc16_posy & 0xff) | ((data & 0x40) << 2);
		}
		return;
	}
}

static UINT8 __fastcall ikari_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	switch (address)
	{
		case 0xce00:
			return turbofront_check8(1,0*8);

		case 0xce20:
			return turbofront_check8(1,1*8);

		case 0xce40:
			return turbofront_check8(1,2*8);

		case 0xce60:
			return turbofront_check8(1,3*8);

		case 0xce80:
			return turbofront_check8(1,4*8);

		case 0xcea0:
			return turbofront_check8(1,5*8);

		case 0xcee0:
			return (turbofront_check8(1,6*8) << 0) | (turbofront_check8(1,6*8) << 4) | (turbofront_check8(1,6*8+1) << 1) | (turbofront_check8(1,6*8+1) << 5);
	}

	return common_read(0xc000, address, 0x01, 0x04, 1);
}

static UINT8 __fastcall ikaria_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	switch (address)
	{
		case 0xce00:
			return turbofront_check8(1,0*8);

		case 0xce20:
			return turbofront_check8(1,1*8);

		case 0xce40:
			return turbofront_check8(1,2*8);

		case 0xce60:
			return turbofront_check8(1,3*8);

		case 0xce80:
			return turbofront_check8(1,4*8);

		case 0xcea0:
			return turbofront_check8(1,5*8);

		case 0xcee0:
			return (turbofront_check8(1,6*8) << 0) | (turbofront_check8(1,6*8) << 4) | (turbofront_check8(1,6*8+1) << 1) | (turbofront_check8(1,6*8+1) << 5);
	}

	return common_read(0xc000, address, 0x20, 0x04, 1);
}

static void __fastcall ikari_sub_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xc000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xc980:
			txt_palette_offset = ((data & 0x01) << 4);
			txt_tile_offset = ((data & 0x10) << 4);
		return;

		case 0xcc00:
			tc16_posy = (tc16_posy & ~0xff) | data;
		return;

		case 0xcc80:
			tc16_posx = (tc16_posx & ~0xff) | data;
		return;

		case 0xcd80:
		{
			tc16_posx = (tc16_posx & 0xff) | ((data & 0x80) << 1);
			tc16_posy = (tc16_posy & 0xff) | ((data & 0x40) << 2);
		}
		return;
	}
}

static UINT8 __fastcall ikari_sub_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	switch (address)
	{
		case 0xc000:
			return nmi_trigger_read();

		case 0xce00:
			return turbofront_check8(1,0*8);

		case 0xce20:
			return turbofront_check8(1,1*8);

		case 0xce40:
			return turbofront_check8(1,2*8);

		case 0xce60:
			return turbofront_check8(1,3*8);

		case 0xce80:
			return turbofront_check8(1,4*8);

		case 0xcea0:
			return turbofront_check8(1,5*8);

		case 0xcee0:
			return (turbofront_check8(1,6*8) << 0) | (turbofront_check8(1,6*8) << 4) | (turbofront_check8(1,6*8+1) << 1) | (turbofront_check8(1,6*8+1) << 5);
	}

	return 0;
}

static void __fastcall tdfever_ab_write(UINT16 address, UINT8 data)
{
//	bprintf (0, _T("W C%d, A: %4.4x, D: %2.2x\n"), ZetGetActive(), address, data);

	switch (address)
	{
		case 0xc500:
		{
			soundlatch = data;
			ZetClose();
			ZetOpen(2);
			sndirq_update_callback(CMDIRQ_BUSY_ASSERT);
			ZetClose();
			ZetOpen(0);
		}
		return;

		case 0xc600:
		return;		// coin counter

		case 0xc000:
		case 0xc700:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
		return;

		case 0xc800:
			bg_scrolly = (bg_scrolly & 0x100) | data;
		return;

		case 0xc840:
			bg_scrollx = (bg_scrollx & 0x100) | data;
		return;

		case 0xc880:
			flipscreen = data & 0x10;
			bg_scrollx = (bg_scrollx & 0xff) | ((data & 0x02) << 7);
			bg_scrolly = (bg_scrolly & 0xff) | ((data & 0x01) << 8);
		return;

		case 0xc8c0:
			txt_palette_offset = ((data & 0xf) << 4);
			txt_tile_offset = ((data & 0x30) << 4);
		return;

		case 0xc900:
			sp32_scrolly = (sp32_scrolly & 0xff) | ((data & 0x80) << 1);
			sp32_scrollx = (sp32_scrollx & 0xff) | ((data & 0x40) << 2);
		return;

		case 0xc980:
			sp32_scrolly = (sp32_scrolly & 0x100) | data;
		return;

		case 0xc9c0:
			sp32_scrollx = (sp32_scrollx & 0x100) | data;
		return;
	}
}

static UINT8 __fastcall tdfever_main_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	switch (address)
	{
		case 0xc000:
			return (DrvInputs[0] & ~0x08) | ((sound_status & 0x04) ? 0x08 : 0);

		case 0xc080: return DrvInputs[0];
		case 0xc100: return DrvInputs[1];
		case 0xc180: return DrvInputs[2];
		case 0xc200: return DrvInputs[3];
		case 0xc280: return DrvInputs[4];
		case 0xc300: return DrvInputs[5];
		case 0xc380:
		case 0xc400:
		case 0xc480:
			return 0xff; // inputs...

		case 0xc580:
			return DrvDips[0];

		case 0xc600:
			return DrvDips[1];

		case 0xc700:
			return nmi_trigger_read();
	}

	return 0;
}

static UINT8 __fastcall tdfever_sub_read(UINT16 address)
{
//	bprintf (0, _T("%d R: %4.4x\n"), ZetGetActive(), address);

	switch (address)
	{
		case 0xc000:
		case 0xc700:
			return nmi_trigger_read();
	}

	return 0;
}



static void __fastcall ym3526_y8950_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
			BurnYM3526Write(0, data);
		return;

		case 0xec00:
			BurnYM3526Write(1, data);
		return;

		case 0xf000:
			BurnY8950Write(0, 0, data);
		return;

		case 0xf400:
			BurnY8950Write(0, 1, data);
		return;

		case 0xf800:
		{
			if (~data & 0x10) sndirq_update_callback(YM1IRQ_CLEAR);
			if (~data & 0x20) sndirq_update_callback(YM2IRQ_CLEAR);
			if (~data & 0x40) sndirq_update_callback(BUSY_CLEAR);
			if (~data & 0x80) sndirq_update_callback(CMDIRQ_CLEAR);
		}
		return;
	}
}

static UINT8 __fastcall ym3526_y8950_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return soundlatch;

		case 0xe800:
			return BurnYM3526Read(0);

		case 0xf000:
			return BurnY8950Read(0, 0);

		case 0xf800:
			return sound_status;
	}

	return 0;
}

static void __fastcall ym3526_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
			BurnYM3526Write(0, data);
		return;

		case 0xe001:
			BurnYM3526Write(1, data);
		return;
	}
}

static UINT8 __fastcall ym3526_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			return soundlatch;

		case 0xc000:
			soundlatch = 0;
			sndirq_update_callback(BUSY_CLEAR);
			return 0xff;

		case 0xe000:
		case 0xe001:
			return BurnYM3526Read(0);

		case 0xe004:
			sndirq_update_callback(CMDIRQ_CLEAR);
			return 0xff;

		case 0xe006:
			sndirq_update_callback(YM1IRQ_CLEAR);
			return 0xff;
	}

	return 0;
}

static void __fastcall aso_ym3526_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xf000:
			BurnYM3526Write(0, data);
		return;

		case 0xf001:
			BurnYM3526Write(1, data);
		return;
	}
}

static UINT8 __fastcall aso_ym3526_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xd000:
			return soundlatch;

		case 0xe000:
			sndirq_update_callback(BUSY_CLEAR);
			soundlatch = 0;
			return 0xff;

		case 0xf000:
		case 0xf001:
			return BurnYM3526Read(0);

		case 0xf004:
			sndirq_update_callback(CMDIRQ_CLEAR);
			return 0xff;

		case 0xf006:
			sndirq_update_callback(YM1IRQ_CLEAR);
			return 0xff;
	}

	return 0;
}

static void __fastcall marvins_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0x8000:
		case 0x8001:
		case 0x8008:
		case 0x8009:
			AY8910Write((address >> 3) & 1, address & 1, data);
		return;

		case 0x8002:
		case 0x8003:
		case 0x8004:
		case 0x8005:
		case 0x8006:
		case 0x8007:
			snkwave_w(address - 0x8002, data);
		return;
	}
}

static UINT8 __fastcall marvins_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0x4000:
			sound_status = 0;
			//ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return soundlatch;

		case 0xa000:
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_NONE);
			return 0xff;
	}

	return 0;
}

static void __fastcall jcross_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe000:
		case 0xe001:
		case 0xe004:
		case 0xe005:
			AY8910Write((address >> 2) & 1, address & 1, data);
		return;

		case 0xe008:
		case 0xe009:
			if (hal21mode) {
				AY8910Write(1, address & 1, data);
			}
		return;
	}
}

static UINT8 __fastcall jcross_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xa000:
			sgladiat_sndirq_update_callback(BUSY_CLEAR);
			return soundlatch;

		case 0xc000:
			sgladiat_sndirq_update_callback(CMDIRQ_CLEAR);
			return 0xff;
	}

	return 0;
}

static UINT8 __fastcall jcross_sound_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x00:
			ZetSetIRQLine(0, CPU_IRQSTATUS_NONE);
			return 0xff;
	}

	return 0;
}

static void __fastcall ym3812_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
			BurnYM3812Write(0, 0, data);
		return;

		case 0xec00:
			BurnYM3812Write(0, 1, data);
		return;

		case 0xf800:
			if (~data & 0x10) sndirq_update_callback(YM1IRQ_CLEAR);
			if (~data & 0x20) sndirq_update_callback(YM2IRQ_CLEAR);
			if (~data & 0x40) sndirq_update_callback(BUSY_CLEAR);
			if (~data & 0x80) sndirq_update_callback(CMDIRQ_CLEAR);
		return;
	}
}

static UINT8 __fastcall ym3812_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return soundlatch;

		case 0xe800:
			return BurnYM3812Read(0, 0);

		case 0xf800:
			return sound_status;
	}

	return 0;
}


static void __fastcall ym3812_y8950_sound_write(UINT16 address, UINT8 data)
{
	switch (address)
	{
		case 0xe800:
			BurnYM3812Write(0, 0, data);
		return;

		case 0xec00:
			BurnYM3812Write(0, 1, data);
		return;

		case 0xf000:
			BurnY8950Write(0, 0, data);
		return;

		case 0xf400:
			BurnY8950Write(0, 1, data);
		return;

		case 0xf800:
		{
			if (~data & 0x10) sndirq_update_callback(YM1IRQ_CLEAR);
			if (~data & 0x20) sndirq_update_callback(YM2IRQ_CLEAR);
			if (~data & 0x40) sndirq_update_callback(BUSY_CLEAR);
			if (~data & 0x80) sndirq_update_callback(CMDIRQ_CLEAR);
		}
		return;
	}
}

static UINT8 __fastcall ym3812_y8950_sound_read(UINT16 address)
{
	switch (address)
	{
		case 0xe000:
			return soundlatch;

		case 0xe800:
			return BurnYM3812Read(0, 0);

		case 0xf000:
			return BurnY8950Read(0, 0);

		case 0xf800:
			return sound_status;
	}

	return 0;
}

static void DrvFMIRQHandler(INT32, INT32 nStatus)
{
	if (nStatus) {
		INT32 nActive = ZetGetActive();

		if (nActive != 2) {
			ZetClose();
			ZetOpen(2);
		}

		sndirq_update_callback((nActive == 2) ? YM2IRQ_ASSERT : YM1IRQ_ASSERT);

		if (nActive != 2) {
			ZetClose();
			ZetOpen(nActive);
		}
	}
}

static void DrvFMIRQHandler_CB1(INT32, INT32 nStatus)
{
	if (nStatus) {
		INT32 nActive = ZetGetActive();

		if (nActive != 2) {
			ZetClose();
			ZetOpen(2);
		}

		sndirq_update_callback(YM1IRQ_ASSERT);

		if (nActive != 2) {
			ZetClose();
			ZetOpen(nActive);
		}
	}
}

static void DrvFMIRQHandler_CB2(INT32, INT32 nStatus)
{
	if (nStatus) {
		INT32 nActive = ZetGetActive();

		if (nActive != 2) {
			ZetClose();
			ZetOpen(2);
		}

		sndirq_update_callback(YM2IRQ_ASSERT);

		if (nActive != 2) {
			ZetClose();
			ZetOpen(nActive);
		}
	}
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	INT32 nActive = ZetGetActive(), nCycles = 0;
	if (nActive != 2) {
		ZetClose();
		ZetOpen(2);
		nCycles = ZetTotalCycles();
		ZetClose();
		ZetOpen(nActive);
	} else {
		nCycles = ZetTotalCycles();
	}

	return (INT64)nCycles * nSoundRate / 4000000;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	ZetOpen(2);
	ZetReset();
	if (game_select == 5) {
		AY8910Reset(0);
		AY8910Reset(1);
	} else if (game_select == 7) {
		BurnYM3812Reset();
	} else if (game_select == 9) {
		BurnY8950Reset();
		BurnYM3812Reset();
	} else {
		BurnYM3526Reset();
		if (game_select != 4) BurnY8950Reset();
	}
	ZetClose();

	snkwave_reset(); // can be run on reset for all games, no big deal.

	HiscoreReset();

	sound_status = 0;
	soundlatch = 0;
	flipscreen = 0;
	sp16_scrolly = 0;
	sp16_scrollx = 0;
	sp32_scrolly = 0;
	sp32_scrollx = 0;
	bg_scrollx = 0;
	bg_scrolly = 0;
	fg_scrollx = 0;
	fg_scrolly = 0;
	txt_palette_offset = 0;
	txt_tile_offset = 0;
	bg_palette_offset = 0;
	bg_tile_offset = 0;
	sprite_split_point = 0;
	tc16_posy = 0;
	tc16_posx = 0;
	tc32_posy = 0;
	tc32_posx = 0;

	RotateReset();

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x010000;
	DrvZ80ROM1		= Next; Next += 0x010000;
	DrvZ80ROM2		= Next; Next += 0x010000;

	DrvGfxROM0		= Next; Next += 0x010000;
	DrvGfxROM1		= Next; Next += 0x080000 + 0x100;
	DrvGfxROM2		= Next; Next += 0x080000;
	DrvGfxROM3		= Next; Next += 0x100000;
	DrvGfxROM4		= Next; Next += 0x004000;

	DrvSndROM0		= Next; Next += 0x040000;

	DrvColPROM		= Next; Next += 0x000c00;

	DrvTransTab		= Next; Next += 0x000400;

	DrvPalette		= (UINT32*)Next; Next += 0x0400 * sizeof(UINT32);

	AllRam			= Next;

	DrvFgVRAM		= Next; Next += 0x000800;
	DrvBgVRAM		= Next; Next += 0x002000;
	DrvShareRAM		= Next; Next += 0x001800;
	DrvSprRAM		= Next; Next += 0x001800;
	DrvTxtRAM		= Next; Next += 0x000800;
	DrvZ80RAM2		= Next; Next += 0x001000;

	RamEnd			= Next;

	pAY8910Buffer[0]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[1]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[2]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[3]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[4]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);
	pAY8910Buffer[5]	= (INT16*)Next; Next += nBurnSoundLen * sizeof(INT16);

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode(INT32 gfx, INT32 nType, UINT8 *src, INT32 nLen)
{
	if (nLen == 0) {
		DrvGfxMask[gfx] = 0;
		return 1;
	}

	INT32 Plane0[4]  = { STEP4(0,1) };
	INT32 Plane1[4]  = { (nLen/4)*8*3, (nLen/4)*8*2, (nLen/4)*8*1, (nLen/4)*8*0 };
	INT32 Plane2[3]  = { (nLen/3)*8*2, (nLen/3)*8*1, (nLen/3)*8*0 };
	INT32 XOffs1[16] = { 4*1, 4*0, 4*3, 4*2, 4*5, 4*4, 4*7, 4*6, 32+4*1, 32+4*0, 32+4*3, 32+4*2, 32+4*5, 32+4*4, 32+4*7, 32+4*6 };
	INT32 XOffs2[16] = { STEP8(8,1), STEP8(0,1) };
	INT32 XOffs3[32] = { STEP8(24,1), STEP8(16,1), STEP8(8,1), STEP8(0,1) };
	INT32 XOffs4[32] = { STEP8(7,-1), STEP8(15,-1), STEP8(23,-1), STEP8(31,-1) };
	INT32 YOffs1[32] = { STEP8(0,32), STEP8(256,32), STEP8(512,32), STEP8(768,32) };
	INT32 YOffs2[16] = { STEP16(0,16) };
	INT32 YOffs3[16] = { STEP16(0,64) };

	UINT8 *tmp = (UINT8*)BurnMalloc(nLen);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, src, nLen);

	switch (nType)
	{
		case 0: // charlayout
			GfxDecode(((nLen * 8) / 4)/( 8 *  8), 4,  8,  8, Plane0, XOffs1, YOffs1, 0x100, tmp, src);
			DrvGfxMask[gfx] = ((nLen * 2) / (8 * 8));
		break;

		case 1: // tilelayout
			GfxDecode(((nLen * 8) / 4)/(16 * 16), 4, 16, 16, Plane0, XOffs1, YOffs3, 0x400, tmp, src);
			DrvGfxMask[gfx] = ((nLen * 2) / (16 * 16));
		break;

		case 2: // spritelayout 4 bpp
			GfxDecode(((nLen * 8) / 4)/(16 * 16), 4, 16, 16, Plane1, XOffs2, YOffs2, 0x100, tmp, src);
			DrvGfxMask[gfx] = ((nLen * 2) / (16 * 16));
		break;

		case 3: // bigspritelayout 4 bpp
			GfxDecode(((nLen * 8) / 4)/(32 * 32), 4, 32, 32, Plane1, XOffs3, YOffs1, 0x400, tmp, src);
			DrvGfxMask[gfx] = ((nLen * 2) / (32 * 32));
		break;

		case 4: // spritelayout 3bpp
			GfxDecode(((nLen * 8) / 3)/(16 * 16), 3, 16, 16, Plane2, XOffs4, YOffs2, 0x100, tmp, src);
			DrvGfxMask[gfx] = (((nLen * 8) / 3) / (16 * 16));
		break;

		case 5: // bigspritelayout 3bpp
			GfxDecode(((nLen * 8 )/ 3)/(32 * 32), 3, 32, 32, Plane2, XOffs4, YOffs1, 0x400, tmp, src);
			DrvGfxMask[gfx] = (((nLen * 8) / 3) / (32 * 32));
		break;
	}

	BurnFree(tmp);

	for (UINT32 i = 1; i <= 0x1000000; i<<=1) {
		if (i >= (UINT32)DrvGfxMask[gfx]) {
			DrvGfxMask[gfx] = i-1;
			break;
		}
	}

	if (nType == 1) memset (src + ((DrvGfxMask[gfx]+1) * 16 * 16), 0xf, 16 * 16); // empty tile for background

	return 0;
}


static INT32 DrvRomLoad()
{
	char* pRomName;
	struct BurnRomInfo ri;

	UINT8 *prog[3] = { DrvZ80ROM0, DrvZ80ROM1, DrvZ80ROM2 };
	UINT8 *gfx[5] = { DrvGfxROM0, DrvGfxROM1, DrvGfxROM2, DrvGfxROM3, DrvGfxROM4 };
	INT32 gfxtype[5] = {0,0,0,0,0};
	UINT8 *col = DrvColPROM;
	UINT8 *snd = DrvSndROM0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++)
	{
		BurnDrvGetRomInfo(&ri, i);
		if ((ri.nType & 0xf) == 0) continue;

		if ((ri.nType & 0xf) < 4) { // program roms
			if (BurnLoadRom(prog[(ri.nType&0xf)-1], i, 1)) return 1;
			prog[(ri.nType&0xf)-1] += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 4) { // txt
			if (BurnLoadRom(gfx[(ri.nType&0xf)-4], i, 1)) return 1;
			gfx[(ri.nType&0xf)-4] += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 5 || (ri.nType & 0xf) == 6) { // bg layer
			if (BurnLoadRom(gfx[1], i, 1)) return 1;
			gfx[1] += ri.nLen;
			gfxtype[1] = ((ri.nType&0xf)-5) ^ 1; // 1,0
			continue;
		}

		if ((ri.nType & 0xf) == 7 || (ri.nType & 0xf) == 9) { // 16x16 4 bpp sprite / 3 bpp sprite
			if (BurnLoadRom(gfx[2], i, 1)) return 1;
			gfx[2] += ri.nLen;
			gfxtype[2] = ((ri.nType & 0xf) == 7) ? 2 : 4;
			continue;
		}

		if ((ri.nType & 0xf) == 8 || (ri.nType & 0xf) == 10) { // 32x32 4 bpp sprite / 3 bpp sprite
			if (BurnLoadRom(gfx[3], i, 1)) return 1;
			gfx[3] += ri.nLen;
			gfxtype[3] = ((ri.nType & 0xf) == 8) ? 3 : 5;
			continue;
		}

		if ((ri.nType & 0xf) == 13) { // foreground tiles
			if (BurnLoadRom(gfx[4], i, 1)) return 1;
			gfx[4] += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 14) { // color data
			if (BurnLoadRom(col, i, 1)) return 1;
			col += ri.nLen;
			continue;
		}

		if ((ri.nType & 0xf) == 15) { // samples
			if (BurnLoadRom(snd, i, 1)) return 1;
			snd += ri.nLen;
			continue;
		}
	}

	DrvGfxDecode(0, gfxtype[0], DrvGfxROM0, gfx[0] - DrvGfxROM0);
	DrvGfxDecode(1, gfxtype[1], DrvGfxROM1, gfx[1] - DrvGfxROM1);
	DrvGfxDecode(2, gfxtype[2], DrvGfxROM2, gfx[2] - DrvGfxROM2);
	DrvGfxDecode(3, gfxtype[3], DrvGfxROM3, gfx[3] - DrvGfxROM3);
	DrvGfxDecode(4, gfxtype[4], DrvGfxROM4, gfx[4] - DrvGfxROM4);
	nSampleLen = snd - DrvSndROM0;

	return 0;
}

static INT32 PsychosInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(bermudat_main_write);
	ZetSetReadHandler(bermudat_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(bermudat_sub_write);
	ZetSetReadHandler(bermudat_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0,&DrvFMIRQHandler, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 1;

	bonus_dip_config = 0x3004;

	DrvDoReset();

	return 0;
}

static INT32 BermudatInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(bermudat_main_write);
	ZetSetReadHandler(bermudat_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(bermudat_sub_write);
	ZetSetReadHandler(bermudat_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0,&DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	bonus_dip_config = 0x3004;
	game_select = 2;
	game_rotates = 1;

	RotateSetGunPosRAM(&DrvSprRAM[0x041], &DrvSprRAM[0x055], 1);

	DrvDoReset();

	return 0;
}

static INT32 GwarInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(gwar_main_write);
	ZetSetReadHandler(bermudat_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(gwar_sub_write);
	ZetSetReadHandler(gwar_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0, &DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 3;
	game_rotates = 1;
	bonus_dip_config = 0x3004;

	RotateSetGunPosRAM(&DrvSprRAM[0x3d3], &DrvSprRAM[0x437], 2);

	DrvDoReset();

	return 0;
}

static INT32 GwarbInit()
{
	INT32 rc = GwarInit();

	game_rotates = 0;

	return rc;
}

static INT32 GwaraInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvTxtRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(gwar_main_write);
	ZetSetReadHandler(bermudat_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvTxtRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(gwar_sub_write);
	ZetSetReadHandler(gwar_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0,&DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 3;
	game_rotates = 1;
	bonus_dip_config = 0x3004;

	DrvDoReset();

	return 0;
}

static INT32 Tnk3Init()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tnk3_main_write);
	ZetSetReadHandler(tnk3_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xf000, 0xf7ff, MAP_RAM); // not really shared
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tnk3_sub_write);
	ZetSetReadHandler(tnk3_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(ym3526_sound_write);
	ZetSetReadHandler(ym3526_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 4;
	game_rotates = 1;
	bonus_dip_config = 0x01c0;

	RotateSetGunPosRAM(&DrvTxtRAM[0x547], &DrvTxtRAM[0x58d], 2); // TNK3

	DrvDoReset();

	return 0;
}

static INT32 AsoInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	// swap sprite halves
	{
		UINT8 *tmp = (UINT8*)BurnMalloc(0x20000);
		memcpy (tmp, DrvGfxROM2, 0x20000);
		memcpy (DrvGfxROM2, DrvGfxROM2 + 0x20000, 0x20000);
		memcpy (DrvGfxROM2 + 0x20000, tmp, 0x20000);
		BurnFree (tmp);
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe800, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(aso_main_write);
	ZetSetReadHandler(tnk3_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvShareRAM,	0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tnk3_sub_write);
	ZetSetReadHandler(tnk3_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xc7ff, MAP_RAM);
	ZetSetWriteHandler(aso_ym3526_sound_write);
	ZetSetReadHandler(aso_ym3526_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 4;
	bonus_dip_config = 0x01c0;

	DrvDoReset();

	return 0;
}

static INT32 AlphamisInit()
{
	INT32 nRet = AsoInit();

	bonus_dip_config = 0x0100;

	return nRet;
}

static INT32 AthenaInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tnk3_main_write);
	ZetSetReadHandler(athena_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xf000, 0xf7ff, MAP_RAM); // not really shared
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tnk3_sub_write);
	ZetSetReadHandler(tnk3_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, NULL, 0, NULL, 0,&DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 6;
	bonus_dip_config = 0x3004;

	DrvDoReset();

	return 0;
}

static INT32 MarvinsInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x5fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvFgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM+0x800,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(marvins_ab_write);
	ZetSetReadHandler(marvins_ab_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x1fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvFgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM+0x800,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(marvins_ab_write);
	ZetSetReadHandler(marvins_ab_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(marvins_sound_write);
	ZetSetReadHandler(marvins_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.20, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.20, BURN_SND_ROUTE_BOTH);

	snkwave_volume = 0.50;

	GenericTilesInit();

	game_select = 5;
	bonus_dip_config = 0;

	DrvDoReset();

	return 0;
}

static INT32 MadcrashInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvFgVRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM+0x800,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(madcrash_main_write);
	ZetSetReadHandler(marvins_ab_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvFgVRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM+0x800,	0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(madcrash_sub_write);
	ZetSetReadHandler(marvins_ab_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(marvins_sound_write);
	ZetSetReadHandler(marvins_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.25, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.25, BURN_SND_ROUTE_BOTH);

	snkwave_volume = 0.30; // for vangrd2

	GenericTilesInit();

	game_select = 5;
	bonus_dip_config = 0;

	DrvDoReset();

	return 0;
}

static INT32 MadcrushInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvFgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM+0x800,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(madcrush_ab_write);
	ZetSetReadHandler(marvins_ab_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvFgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM+0x800,	0xe800, 0xefff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(madcrush_ab_write);
	ZetSetReadHandler(marvins_ab_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(marvins_sound_write);
	ZetSetReadHandler(marvins_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.35, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.35, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 5;
	bonus_dip_config = 0;

	DrvDoReset();

	return 0;
}

static INT32 JcrossInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(jcross_main_write);
	ZetSetReadHandler(jcross_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xc800, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(jcross_main_write);
	ZetSetReadHandler(jcross_main_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(jcross_sound_write);
	ZetSetReadHandler(jcross_sound_read);
	ZetSetInHandler(jcross_sound_read_port);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	video_sprite_number = 25;
	game_select = 5;
	bonus_dip_config = 0x01c0;

	DrvDoReset();

	return 0;
}

static INT32 SgladiatInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf000, 0xf7ff, MAP_RAM);
	ZetSetWriteHandler(jcross_main_write);
	ZetSetReadHandler(jcross_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xc800, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetSetWriteHandler(sgladiat_sub_write);
	ZetSetReadHandler(jcross_main_read); // ?
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(jcross_sound_write);
	ZetSetReadHandler(jcross_sound_read);
	ZetSetInHandler(jcross_sound_read_port);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.35, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.35, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	video_sprite_number = 25;
	video_y_scroll_mask = 0xff;
	bonus_dip_config = 0x01c0;
	game_select = 5;

	DrvDoReset();

	return 0;
}

static INT32 Hal21Init()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xe7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xe800, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(hal21_main_write);
	ZetSetReadHandler(hal21_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0x9fff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc000, 0xc7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xe800, 0xefff, MAP_RAM);
	ZetSetWriteHandler(hal21_sub_write); // no reads, only write
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0x3fff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0x8000, 0x87ff, MAP_RAM);
	ZetSetWriteHandler(jcross_sound_write);
	ZetSetReadHandler(jcross_sound_read);
	ZetClose();

	AY8910Init(0, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(0, 0.15, BURN_SND_ROUTE_BOTH);

	AY8910Init(1, 2000000, nBurnSoundRate, NULL, NULL, NULL, NULL);
	AY8910SetAllRoutes(1, 0.15, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	video_sprite_number = 50;
	video_y_scroll_mask = 0x1ff;
	bonus_dip_config = 0x01c0;
	game_select = 5;
	hal21mode = 1;

	DrvDoReset();

	return 0;
}

static INT32 FitegolfInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tnk3_main_write);
	ZetSetReadHandler(fitegolf_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvSprRAM,		0xc800, 0xcfff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xefff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xf000, 0xf7ff, MAP_RAM); // not really shared
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tnk3_sub_write);
	ZetSetReadHandler(tnk3_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3812_sound_write);
	ZetSetReadHandler(ym3812_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 7;
	bonus_dip_config = 0;

	DrvDoReset();

	return 0;
}

static INT32 IkariCommonInit(INT32 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(ikari_main_write);
	ZetSetReadHandler((game == 1) ? ikaria_main_read : ikari_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(ikari_sub_write);
	ZetSetReadHandler(ikari_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, NULL, 0, NULL, 0,&DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 6;
	game_rotates = 1;
	bonus_dip_config = 0x3004;

	DrvDoReset();

	return 0;
}

static INT32 IkariInit() {
	INT32 rc = IkariCommonInit(0);

	RotateSetGunPosRAM(&DrvTxtRAM[0x5b6], &DrvTxtRAM[0x606], 2);

	return rc;
}

static INT32 IkariaInit() {
	INT32 rc = IkariCommonInit(1);

	RotateSetGunPosRAM(&DrvTxtRAM[0x5b6], &DrvTxtRAM[0x606], 2);

	return rc;
}

static INT32 IkarijoyInit() { ikarijoy = 1; return IkariCommonInit(1); }

static INT32 VictroadInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(ikari_main_write);
	ZetSetReadHandler(ikari_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvBgVRAM,		0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(ikari_sub_write);
	ZetSetReadHandler(ikari_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0,&DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 6;
	bonus_dip_config = 0x3004;
	game_rotates = 1;

	RotateSetGunPosRAM(&DrvTxtRAM[0x5b6], &DrvTxtRAM[0x606], 2);

	DrvDoReset();

	return 0;
}

static INT32 Chopper1Init()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(bermudat_main_write);
	ZetSetReadHandler(bermudat_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(gwar_sub_write);
	ZetSetReadHandler(gwar_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3812_y8950_sound_write);
	ZetSetReadHandler(ym3812_y8950_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.80, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0, &DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	bonus_dip_config = 0x3004;
	game_select = 9;

	DrvDoReset();

	return 0;
}

static INT32 ChopperaInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(gwar_main_write);
	ZetSetReadHandler(bermudat_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(gwar_sub_write);
	ZetSetReadHandler(gwar_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3812_y8950_sound_write);
	ZetSetReadHandler(ym3812_y8950_sound_read);
	ZetClose();

	BurnYM3812Init(1, 4000000, &DrvFMIRQHandler_CB1, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3812(4000000);
	BurnYM3812SetRoute(0, BURN_SND_YM3812_ROUTE, 1.80, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0, &DrvFMIRQHandler_CB2, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 1.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	bonus_dip_config = 0x3004;
	game_select = 9;

	DrvDoReset();

	return 0;
}

static INT32 TdfeverInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (DrvRomLoad()) return 1;

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tdfever_ab_write);
	ZetSetReadHandler(tdfever_main_read);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvBgVRAM,		0xd000, 0xd7ff, MAP_RAM);
	ZetMapMemory(DrvShareRAM,	0xd800, 0xdfff, MAP_RAM);
	ZetMapMemory(DrvSprRAM,		0xe000, 0xf7ff, MAP_RAM);
	ZetMapMemory(DrvTxtRAM,		0xf800, 0xffff, MAP_RAM);
	ZetSetWriteHandler(tdfever_ab_write);
	ZetSetReadHandler(tdfever_sub_read);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,	0x0000, 0xbfff, MAP_ROM);
	ZetMapMemory(DrvZ80RAM2,	0xc000, 0xcfff, MAP_RAM);
	ZetSetWriteHandler(ym3526_y8950_sound_write);
	ZetSetReadHandler(ym3526_y8950_sound_read);
	ZetClose();

	BurnYM3526Init(4000000, &DrvFMIRQHandler, &DrvSynchroniseStream, 0);
	BurnTimerAttachZetYM3526(4000000);
	BurnYM3526SetRoute(BURN_SND_YM3526_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	BurnY8950Init(1, 4000000, DrvSndROM0, nSampleLen, NULL, 0,&DrvFMIRQHandler, &DrvSynchroniseStream, 1);
	BurnTimerAttachZetY8950(4000000);
	BurnY8950SetRoute(0, BURN_SND_Y8950_ROUTE, 2.00, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	game_select = 3;
	game_rotates = 1;
	bonus_dip_config = 0;

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();

	if (game_select == 5)
	{
		AY8910Exit(0);
		AY8910Exit(1);
	} else if (game_select == 7) {
		BurnYM3812Exit();
	} else if (game_select == 9) {
		BurnYM3812Exit();
		BurnY8950Exit();
	} else {
		BurnYM3526Exit();
		if (game_select != 4) BurnY8950Exit();
	}

	BurnFree(AllMem);

	for (INT32 i = 0; i < 5; i++) DrvGfxMask[i] = ~0;

	video_y_scroll_mask = 0x1ff;
	video_sprite_number = 50;
	game_select = 0;
	bonus_dip_config = 0;
	game_rotates = 0;
	hal21mode = 0;
	nSampleLen = 0;
	ikarijoy = 0;

	rotate_gunpos[0] = rotate_gunpos[1] = NULL;

	return 0;
}

static void tnk3PaletteInit()
{
	INT32 len = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < len; i++)
	{
		INT32 bit0 = (DrvColPROM[i + len * 2] >> 3) & 0x01;
		INT32 bit1 = (DrvColPROM[i + len * 0] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + len * 0] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + len * 0] >> 3) & 0x01;

		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + len * 2] >> 2) & 0x01;
		bit1 = (DrvColPROM[i + len * 1] >> 2) & 0x01;
		bit2 = (DrvColPROM[i + len * 1] >> 3) & 0x01;
		bit3 = (DrvColPROM[i + len * 0] >> 0) & 0x01;

		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + len * 2] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + len * 2] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + len * 1] >> 0) & 0x01;
		bit3 = (DrvColPROM[i + len * 1] >> 1) & 0x01;

		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void DrvPaletteInit()
{
	INT32 len = BurnDrvGetPaletteEntries();

	for (INT32 i = 0; i < len; i++)
	{
		INT32 bit0 = (DrvColPROM[i + len * 0] >> 0) & 0x01;
		INT32 bit1 = (DrvColPROM[i + len * 0] >> 1) & 0x01;
		INT32 bit2 = (DrvColPROM[i + len * 0] >> 2) & 0x01;
		INT32 bit3 = (DrvColPROM[i + len * 0] >> 3) & 0x01;

		INT32 r = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + len * 1] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + len * 1] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + len * 1] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + len * 1] >> 3) & 0x01;

		INT32 g = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		bit0 = (DrvColPROM[i + len * 2] >> 0) & 0x01;
		bit1 = (DrvColPROM[i + len * 2] >> 1) & 0x01;
		bit2 = (DrvColPROM[i + len * 2] >> 2) & 0x01;
		bit3 = (DrvColPROM[i + len * 2] >> 3) & 0x01;

		INT32 b = 0x0e * bit0 + 0x1f * bit1 + 0x43 * bit2 + 0x8f * bit3;

		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

static void marvins_draw_layer_tx(INT32 )
{
	for (INT32 offs = 0; offs < 36 * 28; offs++)
	{
		INT32 col = offs % 36; // sx
		INT32 row = offs / 36; // sy

		INT32 sx = (col * 8);// - tx_position_x;
		INT32 sy = (row * 8);// - tx_position_y;

		INT32 ofst = (col - 2);

		if (ofst & 0x20) {
			ofst = 0x400 + row + ((ofst & 0x1f) << 5);
		} else {
			ofst = row + (ofst << 5);
		}

		INT32 code = DrvTxtRAM[ofst] | txt_tile_offset;
		INT32 color = (code >> 5) & 0x07;

		if (ofst & 0x400) { // correct??
			Render8x8Tile_Clip(pTransDraw, code & DrvGfxMask[0], sx, sy, color+0x18, 4, 0, DrvGfxROM0);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code & DrvGfxMask[0], sx, sy, color+0x18, 4, 0xf, 0, DrvGfxROM0);
		}
	}
}

static void ikari_draw_layer_tx(INT32 )
{
	for (INT32 offs = 0; offs < 36 * 28; offs++)
	{
		INT32 col = offs % 36; // sx
		INT32 row = offs / 36; // sy

		INT32 sx = (col * 8);// - tx_position_x;
		INT32 sy = (row * 8);// - tx_position_y;

		INT32 ofst = (col - 2);

		if (ofst & 0x20) {
			ofst = 0x400 + row + ((ofst & 0x1f) << 5);
		} else {
			ofst = row + (ofst << 5);
		}

		INT32 code = DrvTxtRAM[ofst] | txt_tile_offset;

		if (ofst & 0x400) { // correct??
			Render8x8Tile_Clip(pTransDraw, code & DrvGfxMask[0], sx, sy, 0, 4, txt_palette_offset+0x180, DrvGfxROM0);
		} else {
			Render8x8Tile_Mask_Clip(pTransDraw, code & DrvGfxMask[0], sx, sy, 0, 4, 0xf, txt_palette_offset+0x180, DrvGfxROM0);
		}
	}
}

static void gwar_draw_layer_tx(INT32 )
{
	for (INT32 offs = 0; offs < 50 * 32; offs++)
	{
		INT32 sy = (offs % 32) * 8;
		INT32 sx = (offs / 32) * 8;

		INT32 code = DrvTxtRAM[offs] + txt_tile_offset;

		Render8x8Tile_Mask_Clip(pTransDraw, code & DrvGfxMask[0], sx, sy, 0, 4, 0xf, txt_palette_offset, DrvGfxROM0);
	}
}

static void marvins_draw_layer(UINT8 *ram, UINT8 *gfx, INT32 scrollx, INT32 scrolly, INT32 color_offset, INT32 transparent)
{
	scrollx &= 0x1ff;
	scrolly &= 0x0ff;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = ((offs / 32) & 0x3f) * 8;
		INT32 sy = (offs & 0x1f) * 8;

		INT32 code = ram[offs];

		sx -= scrollx;
		if (sx < -7) sx += 512;
		sy -= scrolly;
		if (sy < -7) sy += 256;
		sx += 15; // offset -dink

		if (transparent) {
			Render8x8Tile_Mask_Clip(pTransDraw, code, sx, sy, 0, 4, 0xf, color_offset, gfx);
		} else {
			Render8x8Tile_Clip(pTransDraw, code, sx, sy, 0, 4, color_offset, gfx);
		}
	}
}

static void gwar_draw_layer_bg(INT32 color_offset,INT32 xoffset, INT32 yoffset)
{
	INT32 scrolly = (bg_scrolly - yoffset) & 0x1ff;
	INT32 scrollx = (bg_scrollx - xoffset) & 0x1ff;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs % 32) * 16;
		INT32 sx = (offs / 32) * 16;

		sy -= scrolly;
		if (sy < -15) sy += 512;
		sx -= scrollx;
		if (sx < -15) sx += 512;

		INT32 attr  = DrvBgVRAM[offs * 2 + 1];
		INT32 code  = DrvBgVRAM[offs * 2 + 0] | ((attr & 0x0f) << 8);
		INT32 color = attr >> 4;

		if (game_select == 1) color &= 0x07; // psychos!

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;
		if (code > (INT32)DrvGfxMask[1]) code = DrvGfxMask[1] + 1;

		Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, color_offset+bg_palette_offset, DrvGfxROM1);
	}
}

static void ikari_draw_layer_bg(INT32 xoffset, INT32 yoffset)
{
	INT32 scrolly = (bg_scrolly - yoffset) & 0x1ff;
	INT32 scrollx = (bg_scrollx - xoffset) & 0x1ff;

	for (INT32 offs = 0; offs < 32 * 32; offs++)
	{
		INT32 sy = (offs % 32) * 16;
		INT32 sx = (offs / 32) * 16;

		sy -= scrolly;
		if (sy < -15) sy += 512;
		sx -= scrollx;
		if (sx < -15) sx += 512;
		sx += 15; // offset -dink (yes, 15)

		INT32 attr  = DrvBgVRAM[offs * 2 + 1];
		INT32 code  = DrvBgVRAM[offs * 2 + 0] | ((attr & 0x03) << 8);
		INT32 color = (attr >> 4) & 0x7;

		if (sx >= nScreenWidth || sy >= nScreenHeight) continue;

		Render16x16Tile_Clip(pTransDraw, code & DrvGfxMask[1], sx, sy, color, 4, 0x100, DrvGfxROM1);
	}
}

static void aso_draw_layer_bg(INT32 xoffset,INT32 yoffset)
{
	INT32 height = (video_y_scroll_mask + 1) / 8;
	INT32 scrolly = (bg_scrolly - yoffset) & video_y_scroll_mask;
	INT32 scrollx = (bg_scrollx - xoffset) & 0x1ff;

	for (INT32 offs = 0; offs < 64 * height; offs++)
	{
		INT32 sy = (offs & (height-1)) * 8;
		INT32 sx = (offs / height) * 8;

		sy -= scrolly;
		if (sy < -7) sy += height*8;
		sx -= scrollx;
		if (sx < -7) sx += 512;

		sx += 16;

		INT32 code = DrvBgVRAM[offs] + bg_tile_offset;

		Render8x8Tile_Clip(pTransDraw, code & DrvGfxMask[1], sx, sy, 0, 4, bg_palette_offset+0x80, DrvGfxROM1);
	}
}

static void tnk3_draw_layer_bg(INT32 xoffset,INT32 yoffset)
{
	INT32 scrolly = (bg_scrolly - yoffset) & 0x1ff;
	INT32 scrollx = (bg_scrollx - xoffset) & 0x1ff;

	for (INT32 offs = 0; offs < 64 * 64; offs++)
	{
		INT32 sy = (offs & 0x3f) * 8;
		INT32 sx = (offs / 0x40) * 8;

		sy -= scrolly;
		if (sy < -15) sy += 512;
		sx -= scrollx;
		if (sx < -15) sx += 512;

		sx += 15; // offset -dink

		INT32 attr  = DrvBgVRAM[offs * 2 + 1];
		INT32 code  = DrvBgVRAM[offs * 2 + 0] | ((attr & 0x30) << 4);
		INT32 color = (attr & 0xf) ^ 8; // ??

		Render8x8Tile_Clip(pTransDraw, code & DrvGfxMask[1], sx, sy, color+8, 4, 0, DrvGfxROM1);
	}
}

static void marvins_draw_sprites(INT32 scrollx, INT32 scrolly, INT32 from, INT32 to)
{
	const UINT8 *source = DrvSprRAM + from*4;
	const UINT8 *finish = DrvSprRAM + to*4;

	while( source<finish )
	{
		int attributes = source[3]; /* Y?F? CCCC */
		int tile_number = source[1];
		int sx =  scrollx + 301 - 15 - source[2] + ((attributes&0x80)?256:0);
		int sy = -scrolly - 8 + source[0];
		int color = attributes&0xf;
		int flipy = (attributes&0x20);
		int flipx = 0;

		if (flipscreen)
		{
			sx = 89 - 16 - sx;
			sy = 262 - 16 - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sx &= 0x1ff;
		sy &= 0xff;
		if (sx > 512-16) sx -= 512;
		if (sy > 256-16) sy -= 256;

		sy -= 8; // offset -dink

		{
			color <<= 3;

			INT32 flip = ((flipx ? 0x0f : 0) | (flipy ? 0x0f : 0));
			UINT8 *gfxbase = DrvGfxROM2 + ((tile_number * 0x100) /*& DrvGfxMask[2]*/); // this mask causes the sprites to not animate in marvins & vanguard II

			for (INT32 y = 0; y < 16; y++)
			{
				if ((sy + y) < 0 || (sy + y) >= nScreenHeight) continue;

				for (INT32 x = 0; x < 16; x++)
				{
					if ((sx + x) < 0 || (sx + x) >= nScreenWidth) continue;

					INT32 pxl = gfxbase[((y*16)+x)^flip];

					if (pxl == 0x7) continue; // transparent

					if (pxl == 0x6) {
						pTransDraw[((sy + y) * nScreenWidth) + sx + x] |= 0x200;
					} else {
						pTransDraw[((sy + y) * nScreenWidth) + sx + x] = pxl + color;
					}
				}
			}
		}

		source+=4;
	}
}

static void tnk3_draw_sprites(const int xscroll, const int yscroll)
{
	UINT8 *spriteram = DrvSprRAM;
	UINT8 *gfx = DrvGfxROM2;
	const int size = 16;
	int tile_number, attributes, color, sx, sy;
	int xflip,yflip;

	for (INT32 offs = 0; offs < video_sprite_number*4; offs += 4)
	{
		tile_number = spriteram[offs+1];
		attributes  = spriteram[offs+3];
		color = attributes & 0xf;
		sx =  xscroll + 301 - size - spriteram[offs+2];
		sy = -yscroll + 7 - size + spriteram[offs];
		sx += (attributes & 0x80) << 1;
		sy += (attributes & 0x10) << 4;
		xflip = 0;
		yflip = 0;

		if (DrvGfxMask[2] > 256)  // all except jcross
		{
			tile_number |= (attributes & 0x40) << 2;
		}

		if (DrvGfxMask[2] > 512)  // athena
		{
			tile_number |= (attributes & 0x20) << 4;
		}
		else    // all others
		{
			yflip = attributes & 0x20;
		}

		if (flipscreen)
		{
			sx = 89 - size - sx;    // this causes slight misalignment in tnk3 but is correct for athena and fitegolf
			sy = 262 - size - sy;
			xflip = !xflip;
			yflip = !yflip;
		}

		sx &= 0x1ff;
		sy &= video_y_scroll_mask;    // sgladiat apparently has only 256 pixels of vertical scrolling range
		if (sx > 512-size) sx -= 512;
		if (sy > (video_y_scroll_mask+1)-size) sy -= (video_y_scroll_mask+1);
		sy -= 8; // offset -dink

		{
			UINT8 *gfxbase = gfx + ((tile_number & DrvGfxMask[2]) * size * size);

			INT32 flip = (yflip ? ((size-1)*size) : 0) | (xflip ? (size-1) : 0);

			color = (color << 3) + 0x000;

			for (INT32 y = 0; y < size; y++) {
				if ((sy+y) < 0 || (sy+y) >= nScreenHeight) continue;

				for (INT32 x = 0; x < size; x++) {
					if ((sx+x) < 0 || (sx+x) >= nScreenWidth) continue;

					INT32 pxl = gfxbase[((y*size)+x)^flip];
					if (pxl == 7) continue;
					if (pxl == 6) pTransDraw[(sy+y)*nScreenWidth+(sx+x)] |= 0x200;
					if (pxl <= 5) pTransDraw[(sy+y)*nScreenWidth+(sx+x)] = pxl + color;
				}
			}
		}
	}
}

static void tdfever_draw_sprites(INT32 xscroll, INT32 yscroll, UINT8 *source, UINT8 *gfx, const int hw_xflip, const int from, const int to, INT32 color_offset)
{
	int size = (gfx == DrvGfxROM3) ? 32 : 16;

	INT32 mask = (gfx == DrvGfxROM3) ?  DrvGfxMask[3] : DrvGfxMask[2];

	for (INT32 which = from*4; which < to*4; which+=4)
	{
		int tile_number = source[which+1];
		int attributes  = source[which+3];
		int color = attributes & 0x0f;
		int sx = -xscroll - 9 + source[which+2];
		int sy = -yscroll + 1 - size + source[which];
		sx += (attributes & 0x80) << 1;
		sy += (attributes & 0x10) << 4;

		switch (size)
		{
			case 16:
				tile_number |= ((attributes & 0x08) << 5) | ((attributes & 0x60) << 4);
				color &= 7; // attribute bit 3 is used for bank select
				if (from == 0)
					color |= 8; // low priority sprites use the other palette bank
				break;

			case 32:
				tile_number |= (attributes & 0x60) << 3;
				break;
		}

		int flipx = hw_xflip;
		int flipy = 0;

		if (hw_xflip)
			sx = 495 - size - sx;

		if (flipscreen)
		{
			sx = 495 - size - sx;
			sy = 258 - size - sy;
			flipx = !flipx;
			flipy = !flipy;
		}

		sx &= 0x1ff;
		sy &= 0x1ff;
		if (sx > 512-size) sx -= 512;
		if (sy > 512-size) sy -= 512;


		{
			UINT8 *gfxbase = gfx + ((tile_number & mask) * size * size);

			INT32 flip = (flipy ? ((size-1)*size) : 0) | (flipx ? (size-1) : 0);

			color = (color << 4) + color_offset;

			for (INT32 y = 0; y < size; y++) {
				if ((sy+y) < 0 || (sy+y) >= nScreenHeight) continue;

				for (INT32 x = 0; x < size; x++) {
					if ((sx+x) < 0 || (sx+x) >= nScreenWidth) continue;

					INT32 pxl = gfxbase[((y*size)+x)^flip];
					if (pxl == 15) continue;
					if (pxl == 14) {
						if (pTransDraw[(sy+y)*nScreenWidth+(sx+x)] & 0x200) {
							pTransDraw[(sy+y)*nScreenWidth+(sx+x)] += 0x100;
						} else {
							pTransDraw[(sy+y)*nScreenWidth+(sx+x)] = pxl + color;
						}
					}
					if (pxl <= 13) pTransDraw[(sy+y)*nScreenWidth+(sx+x)] = pxl + color;
				}
			}
		}
	}
}

static void ikari_draw_sprites(const int start, const int xscroll, const int yscroll, const UINT8 *source, UINT8 *gfx)
{
	int size = (gfx == DrvGfxROM3) ? 32 : 16;
	INT32 color_offset = (gfx == DrvGfxROM3) ? 0x80 : 0;
	INT32 mask = (gfx == DrvGfxROM3) ?  DrvGfxMask[3] : DrvGfxMask[2];
	int tile_number, attributes, color, sx, sy;
	int which, finish;
//	int flipx = 0, flipy = 0;

	finish = (start+25)*4;

	for (which = start*4; which < finish; which += 4)
	{
		tile_number = source[which+1];
		attributes  = source[which+3];
		color = attributes & 0xf;
		sx =  xscroll + 300 - size - source[which+2];
		sy = -yscroll + 7 - size + source[which];
		sx += (attributes & 0x80) << 1;
		sy += (attributes & 0x10) << 4;

		switch (size)
		{
			case 16:
				tile_number |= (attributes & 0x60) << 3;
				break;

			case 32:
				tile_number |= (attributes & 0x40) << 2;
				break;
		}

		sx &= 0x1ff;
		sy &= 0x1ff;
		if (sx > 512-size) sx -= 512;
		if (sy > 512-size) sy -= 512;
		sy -= 8; // offset -dink

		{
			UINT8 *gfxbase = gfx + ((tile_number & mask) * size * size);

			INT32 flip = 0; //(yflip ? ((size-1)*size) : 0) | (xflip ? (size-1) : 0);

			color = (color << 3) + color_offset;

			for (INT32 y = 0; y < size; y++) {
				if ((sy+y) < 0 || (sy+y) >= nScreenHeight) continue;

				for (INT32 x = 0; x < size; x++) {
					if ((sx+x) < 0 || (sx+x) >= nScreenWidth) continue;

					INT32 pxl = gfxbase[((y*size)+x)^flip];
					if (pxl == 7) continue;
					if (pxl == 6) pTransDraw[(sy+y)*nScreenWidth+(sx+x)] |= 0x200;
					if (pxl <= 5) pTransDraw[(sy+y)*nScreenWidth+(sx+x)] = pxl + color;
				}
			}
		}
	}
}

static INT32 GwarDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) gwar_draw_layer_bg(0x300,15,0);

	if (nSpriteEnable & 1) tdfever_draw_sprites(sp16_scrollx, sp16_scrolly, DrvSprRAM + 0x800, DrvGfxROM2, 0, 0, sprite_split_point, 0x100 );
	if (nSpriteEnable & 2) tdfever_draw_sprites(sp32_scrollx, sp32_scrolly, DrvSprRAM,         DrvGfxROM3, 0, 0, 32, 0x200 );
	if (nSpriteEnable & 4) tdfever_draw_sprites(sp16_scrollx, sp16_scrolly, DrvSprRAM + 0x800, DrvGfxROM2, 0, sprite_split_point, 64, 0x100 );

	if (nBurnLayer & 2) gwar_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 TdfeverDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) gwar_draw_layer_bg(0x200,143,-32);

	if (nSpriteEnable & 2) tdfever_draw_sprites(sp32_scrollx, sp32_scrolly, DrvSprRAM, DrvGfxROM3, 0, 0, 32, 0x100 );

	if (nBurnLayer & 2) gwar_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 FsoccerDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) gwar_draw_layer_bg(0x200,16,0);

	if (nSpriteEnable & 2) tdfever_draw_sprites(sp32_scrollx, sp32_scrolly, DrvSprRAM, DrvGfxROM3, 1, 0, 32, 0x100 );

	if (nBurnLayer & 2) gwar_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Tnk3Draw()
{
	if (DrvRecalc) {
		tnk3PaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) tnk3_draw_layer_bg(0,0);
	if (nSpriteEnable & 1) tnk3_draw_sprites(sp16_scrollx, sp16_scrolly);
	if (nBurnLayer & 2) marvins_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 JcrossDraw()
{
	if (DrvRecalc) {
		tnk3PaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) aso_draw_layer_bg(0,0);
	if (nSpriteEnable & 1) tnk3_draw_sprites(sp16_scrollx, sp16_scrolly);
	if (nBurnLayer & 2) marvins_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 Hal21Draw()
{
	if (DrvRecalc) {
		tnk3PaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) aso_draw_layer_bg(0,0);
	if (nSpriteEnable & 1) tnk3_draw_sprites(sp16_scrollx, sp16_scrolly);
	if (nBurnLayer & 2) marvins_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 AsoDraw()
{
	if (DrvRecalc) {
		tnk3PaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) aso_draw_layer_bg(256,0);
	if (nSpriteEnable & 1) tnk3_draw_sprites(sp16_scrollx, sp16_scrolly);
	if (nBurnLayer & 2) marvins_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 MarvinsDraw()
{
	if (DrvRecalc) {
		tnk3PaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if ( nBurnLayer & 1) marvins_draw_layer(DrvBgVRAM, DrvGfxROM4, bg_scrollx, bg_scrolly, 0x100 + bg_palette_offset, 0);
	if ( nSpriteEnable & 1) marvins_draw_sprites(sp16_scrollx, sp16_scrolly, 0, sprite_split_point >> 2);
	if ( nBurnLayer & 2) marvins_draw_layer(DrvFgVRAM, DrvGfxROM1, fg_scrollx, fg_scrolly, 0x080 + fg_palette_offset, 1);
	if ( nSpriteEnable & 2) marvins_draw_sprites(sp16_scrollx, sp16_scrolly, sprite_split_point >> 2, 25);
	if ( nBurnLayer & 4) marvins_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 IkariDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	if (~nBurnLayer & 1) BurnTransferClear();

	if (nBurnLayer & 1) ikari_draw_layer_bg(0,0);

	if (nSpriteEnable & 1) ikari_draw_sprites( 0, sp16_scrollx, sp16_scrolly, DrvSprRAM + 0x800, DrvGfxROM2);
	if (nSpriteEnable & 2) ikari_draw_sprites( 0, sp32_scrollx, sp32_scrolly, DrvSprRAM,         DrvGfxROM3);
	if (nSpriteEnable & 4) ikari_draw_sprites(25, sp16_scrollx, sp16_scrolly, DrvSprRAM + 0x800, DrvGfxROM2);

	if (nBurnLayer & 2) ikari_draw_layer_tx(0);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 MarvinsFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 800;
	INT32 nCyclesTotal[3] = { 3360000 / 60, 3360000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		INT32 nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nSegment);
		nSegment = ZetTotalCycles();
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment - ZetTotalCycles());
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		nSegment = nCyclesTotal[2] / nInterleave;
		nCyclesDone[2] += ZetRun(nSegment);
		if ((i % (nInterleave/4)) == ((nInterleave / 4) - 1))
			ZetSetIRQLine(0x20, CPU_IRQSTATUS_ACK);
		ZetClose();
	}

	if (pBurnSoundOut) {
		AY8910Render(&pAY8910Buffer[0], pBurnSoundOut, nBurnSoundLen, 0);
		snkwave_render(pBurnSoundOut, nBurnSoundLen);
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 JcrossFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 3);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
		}
	}

	INT32 nInterleave = 800;
	INT32 nCyclesTotal[3] = { 3350000 / 60, 3350000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nSoundBufferPos = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		INT32 nSegment = nCyclesTotal[0] / nInterleave;
		nCyclesDone[0] += ZetRun(nSegment);
		nSegment = ZetTotalCycles();
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment - ZetTotalCycles());
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		nSegment = nCyclesTotal[2] / nInterleave;
		nCyclesDone[2] += ZetRun(nSegment);
		if ((i % (nInterleave/4)) == ((nInterleave / 4) - 1)) {
			if (hal21mode) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
			} else {
				ZetSetIRQLine(0, CPU_IRQSTATUS_ACK);
			}
		}
		ZetClose();

		// Render Sound Segment
		if (pBurnSoundOut && i%8==7) {
			INT32 nSegmentLength = nBurnSoundLen / 100; //nInterleave;
			INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
			nSoundBufferPos += nSegmentLength;
		}
	}

	// Make sure the buffer is entirely filled.
	if (pBurnSoundOut) {
		INT32 nSegmentLength = nBurnSoundLen - nSoundBufferPos;
		INT16* pSoundBuf = pBurnSoundOut + (nSoundBufferPos << 1);
		if (nSegmentLength) {
			AY8910Render(&pAY8910Buffer[0], pSoundBuf, nSegmentLength, 0);
		}
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 gwar_rotary(INT32 which)
{
	INT32 value = dialRotation(which);

	if ((gwar_rot_last[which] == 0x5 && value == 0x6) || (gwar_rot_last[which] == 0x6 && value == 0x5))
	{
		if (!gwar_rot_cnt[which])
			value = 0xf;
		gwar_rot_cnt[which] = (gwar_rot_cnt[which] + 1) & 0x07;
	}
	gwar_rot_last[which] = value;

	return value;
}

static INT32 GwarFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
			DrvInputs[4] ^= (DrvJoy5[i] & 1) << i;
			DrvInputs[5] ^= (DrvJoy6[i] & 1) << i;
		}

		if (game_select == 1) { // psychos
			DrvDips[0] = (DrvDips[0] & ~0x04) | (DrvDips[2] & 0x04);
			DrvDips[1] = (DrvDips[1] & ~0x30) | (DrvDips[2] & 0x30);
		}

		if (game_rotates) {
			SuperJoy2Rotate();
		}

		if (game_rotates) {
			if (game_select == 3) {
				DrvInputs[1] = (DrvInputs[1] & 0x0f) + (gwar_rotary(0) << 4);
				DrvInputs[2] = (DrvInputs[2] & 0x0f) + (gwar_rotary(1) << 4);
			} else {
				DrvInputs[1] = (DrvInputs[1] & 0x0f) + (dialRotation(0) << 4);
				DrvInputs[2] = (DrvInputs[2] & 0x0f) + (dialRotation(1) << 4);
			}
		} else {
			if (game_select == 3) {
				// gwarb
				DrvInputs[1] = (DrvInputs[1] & 0x0f) + 0xf0;
				DrvInputs[2] = (DrvInputs[2] & 0x0f) + 0xf0;
			}
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 4000000 / 60, 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };
	INT32 nSegment = 0;

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		nSegment = ZetTotalCycles();
		if (i == 240/*(nInterleave - 1)*/) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateYM3526((i + 1) * nCyclesTotal[1] / nInterleave);//nSegment);
		if (i == 240/*(nInterleave - 1)*/) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdateY8950((i + 1) * nCyclesTotal[2] / nInterleave);//nSegment);
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrameYM3526(nCyclesTotal[1]);
	ZetClose();

	ZetOpen(2);
	BurnTimerEndFrameY8950(nCyclesTotal[2]);
	ZetClose();

	if (pBurnSoundOut) {
		ZetOpen(1);
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
			
		ZetOpen(2);
		BurnY8950Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 AthenaFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (game_select == 1) { // psychos
			DrvDips[0] = (DrvDips[0] & ~0x04) | (DrvDips[2] & 0x04);
			DrvDips[1] = (DrvDips[1] & ~0x30) | (DrvDips[2] & 0x30);
		}

		if (game_rotates) {
			SuperJoy2Rotate();
		}

		//if (rotate_gunpos[0]) bprintf(0, _T("[[target %X mempos %X nRotate %X.\n"), nRotateTarget[0], *rotate_gunpos[0] & 0x0f, nRotate[0]);

		if (ikarijoy) {
			DrvInputs[1] &= 0x0f;
			DrvInputs[2] &= 0x0f;
			if (DrvJoy2[0]) DrvInputs[1] |= 0x20;
			if (DrvJoy2[1]) DrvInputs[1] |= 0x40;
			if (DrvJoy2[2]) DrvInputs[1] |= 0x80;
			if (DrvJoy2[3]) DrvInputs[1] |= 0x10;

			if (DrvJoy3[0]) DrvInputs[2] |= 0x20;
			if (DrvJoy3[1]) DrvInputs[2] |= 0x40;
			if (DrvJoy3[2]) DrvInputs[2] |= 0x80;
			if (DrvJoy3[3]) DrvInputs[2] |= 0x10;

		}
	}

	INT32 nInterleave = 800;
	INT32 nCyclesTotal[3] = { 3350000 / 60, 3350000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		INT32 nSegment = ZetTotalCycles();
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateYM3526(nSegment);
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		if ((i&7)==7) // update 100x per frame
		{
			ZetOpen(2);
			BurnTimerUpdateY8950(nSegment);
			ZetClose();
		}
	}

	ZetOpen(1);
	BurnTimerEndFrameYM3526(nCyclesTotal[1]);
	ZetClose();

	ZetOpen(2);
	BurnTimerEndFrameY8950(nCyclesTotal[2]);
	ZetClose();

	if (pBurnSoundOut) {
		ZetOpen(1);
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
			
		ZetOpen(2);
		BurnY8950Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 Tnk3Frame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}

		if (game_rotates) {
			SuperJoy2Rotate();
		}
	}

	INT32 nInterleave = 800;
	INT32 nCyclesTotal[3] = { 3350000 / 60, 3350000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		INT32 nSegment = ZetTotalCycles();
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment - ZetTotalCycles());
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdateYM3526(nSegment);
		ZetClose();
	}

	ZetOpen(2);

	BurnTimerEndFrameYM3526(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM3526Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 FitegolfFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 800;
	INT32 nCyclesTotal[3] = { 3350000 / 60, 3350000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		INT32 nSegment = ZetTotalCycles();
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment - ZetTotalCycles());
		if (i == (nInterleave - 1)) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		if ((i&7)==7) { // update 100x per frame
			ZetOpen(2);
			BurnTimerUpdateYM3812(nSegment);
			ZetClose();
		}
	}

	ZetOpen(2);

	BurnTimerEndFrameYM3812(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 ChopperFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	ZetNewFrame();

	{
		memset (DrvInputs, 0xff, 4);

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
			DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
			DrvInputs[3] ^= (DrvJoy4[i] & 1) << i;
		}
	}

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] = { 4000000 / 60, 4000000 / 60, 4000000 / 60 };
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++)
	{
		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nCyclesTotal[0] / nInterleave);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(1);
		BurnTimerUpdateY8950((i + 1) * nCyclesTotal[1] / nInterleave);
		if (i == 240) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdateYM3812((i + 1) * nCyclesTotal[2] / nInterleave);
		ZetClose();
	}

	ZetOpen(1);
	BurnTimerEndFrameY8950(nCyclesTotal[1]);
	ZetClose();

	ZetOpen(2);
	BurnTimerEndFrameYM3812(nCyclesTotal[2]);
	ZetClose();

	if (pBurnSoundOut) {
		ZetOpen(2);
		BurnYM3812Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();

		ZetOpen(1);
		BurnY8950Update(pBurnSoundOut, nBurnSoundLen);
		ZetClose();
	}

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

		ZetScan(nAction);

		if (game_select == 1 || game_select == 2 || game_select == 3 || game_select == 4 || game_select == 6)
			BurnYM3526Scan(nAction, pnMin);

		if (game_select == 1 || game_select == 2 || game_select == 3 || game_select == 6 || game_select == 9)
			BurnY8950Scan(nAction, pnMin);

		if (game_select == 9)
			BurnYM3812Scan(nAction, pnMin);

		if (game_select == 5)
			AY8910Scan(nAction, pnMin);

		SCAN_VAR(sp16_scrolly);
		SCAN_VAR(sp16_scrollx);
		SCAN_VAR(sp32_scrolly);
		SCAN_VAR(sp32_scrollx);
		SCAN_VAR(bg_scrollx);
		SCAN_VAR(bg_scrolly);
		SCAN_VAR(fg_scrollx);
		SCAN_VAR(fg_scrolly);
		SCAN_VAR(txt_palette_offset);
		SCAN_VAR(txt_tile_offset);
		SCAN_VAR(bg_tile_offset);
		SCAN_VAR(bg_palette_offset);
		SCAN_VAR(fg_palette_offset);
		SCAN_VAR(sprite_split_point);
		SCAN_VAR(tc16_posy);
		SCAN_VAR(tc16_posx);
		SCAN_VAR(tc32_posy);
		SCAN_VAR(tc32_posx);

		SCAN_VAR(nRotate);
		SCAN_VAR(nRotateTarget);
		SCAN_VAR(nRotateTry);
		SCAN_VAR(gwar_rot_last);
		SCAN_VAR(gwar_rot_cnt);

		if (nAction & ACB_WRITE) {
			nRotateTime[0] = nRotateTime[1] = 0;
		}
	}

	return 0;
}


// Marvin's Maze

static struct BurnRomInfo marvinsRomDesc[] = {
	{ "pa1",		0x02000, 0x0008d791, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "pa2",		0x02000, 0x9457003c, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "pa3",		0x02000, 0x54c33ecb, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "pb1",		0x02000, 0x3b6941a5, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code

	{ "m1",			0x02000, 0x2314c696, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "m2",			0x02000, 0x74ba5799, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "s1",			0x02000, 0x327f70f3, 4 | BRF_GRA },	     //  6 Text Characters

	{ "b1",			0x02000, 0xe528bc60, 6 | BRF_GRA },	     //  7 Foreground Characters

	{ "b2",			0x02000, 0xe528bc60, 13 | BRF_GRA },	     //  8 Background Characters

	{ "f1",			0x02000, 0x0bd6b4e5, 9 | BRF_GRA },	     //  9 Sprites
	{ "f2",			0x02000, 0x8fc2b081, 9 | BRF_GRA },	     // 10
	{ "f3",			0x02000, 0xe55c9b83, 9 | BRF_GRA },	     // 11

	{ "marvmaze.j1",	0x00400, 0x92f5b06d, 14 | BRF_GRA },	     // 12 Color Data
	{ "marvmaze.j2",	0x00400, 0xd2b25665, 14 | BRF_GRA },	     // 13
	{ "marvmaze.j3",	0x00400, 0xdf9e6005, 14 | BRF_GRA },	     // 14
};

STD_ROM_PICK(marvins)
STD_ROM_FN(marvins)

struct BurnDriver BurnDrvMarvins = {
	"marvins", NULL, NULL, NULL, "1983",
	"Marvin's Maze\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, marvinsRomInfo, marvinsRomName, NULL, NULL, MarvinsInputInfo, MarvinsDIPInfo,
	MarvinsInit, DrvExit, MarvinsFrame, MarvinsDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Mad Crasher

static struct BurnRomInfo madcrashRomDesc[] = {
	{ "p8",			0x02000, 0xecb2fdc9, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "p9",			0x02000, 0x0a87df26, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "p10",		0x02000, 0x6eb8a87c, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "p4",			0x02000, 0x5664d699, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "p5",			0x02000, 0xdea2865a, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "p6",			0x02000, 0xe25a9b9c, 2 | BRF_ESS | BRF_PRG }, //  5
	{ "p7",			0x02000, 0x55b14a36, 2 | BRF_ESS | BRF_PRG }, //  6
	{ "p3",			0x02000, 0xe3c8c2cb, 2 | BRF_ESS | BRF_PRG }, //  7

	{ "p1",			0x02000, 0x2dcd036d, 3 | BRF_ESS | BRF_PRG }, //  8 Z80 #2 Code
	{ "p2",			0x02000, 0xcc30ae8b, 3 | BRF_ESS | BRF_PRG }, //  9

	{ "p13",		0x02000, 0x48c4ade0, 4 | BRF_GRA },	      // 10 Text Characters

	{ "p11",		0x02000, 0x67174956, 6 | BRF_GRA },	      // 11 Foreground Characters

	{ "p12",		0x02000, 0x085094c1, 13 | BRF_GRA },	      // 12 Background Characters 

	{ "p16",		0x02000, 0x6153611a, 9 | BRF_GRA },	      // 13 Sprites
	{ "p15",		0x02000, 0xa74149d4, 9 | BRF_GRA },	      // 14
	{ "p14",		0x02000, 0x07e807bc, 9 | BRF_GRA },	      // 15

	{ "m3-prom.j3",		0x00400, 0xd19e8a91, 14 | BRF_GRA },	      // 16 Color Data
	{ "m2-prom.j4",		0x00400, 0x9fc325af, 14 | BRF_GRA },	      // 17
	{ "m1-prom.j5",		0x00400, 0x07678443, 14 | BRF_GRA },	      // 18
};

STD_ROM_PICK(madcrash)
STD_ROM_FN(madcrash)

struct BurnDriver BurnDrvMadcrash = {
	"madcrash", NULL, NULL, NULL, "1984",
	"Mad Crasher\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, madcrashRomInfo, madcrashRomName, NULL, NULL, MadcrashInputInfo, MadcrashDIPInfo,
	MadcrashInit, DrvExit, MarvinsFrame, MarvinsDraw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// Mad Crusher (Japan)

static struct BurnRomInfo madcrushRomDesc[] = {
	{ "p3.a8",		0x02000, 0xfbd3eda1, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "p4.a9",		0x02000, 0x1bc67cab, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "p5.a10",		0x02000, 0xd905ff79, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "p6.a11",		0x02000, 0x432b5743, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "p7.a13",		0x02000, 0xdea2865a, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "p8.a14",		0x02000, 0xe25a9b9c, 2 | BRF_ESS | BRF_PRG }, //  5
	{ "p10.bin",		0x02000, 0x55b14a36, 2 | BRF_ESS | BRF_PRG }, //  6
	{ "p9.bin",		0x02000, 0xe3c8c2cb, 2 | BRF_ESS | BRF_PRG }, //  7

	{ "p1.a6",		0x02000, 0x2dcd036d, 3 | BRF_ESS | BRF_PRG }, //  8 Z80 #2 Code
	{ "p2.a8",		0x02000, 0xcc30ae8b, 3 | BRF_ESS | BRF_PRG }, //  9

	{ "p13.e2",		0x02000, 0xfcdd36ca, 4 | BRF_GRA },	      // 10 Text Characters

	{ "p11.a2",		0x02000, 0x67174956, 6 | BRF_GRA },	      // 11 Foreground Characters

	{ "p12.c2",		0x02000, 0x085094c1, 13 | BRF_GRA },	      // 12 Background Characters 

	{ "p16.k4",		0x02000, 0x6153611a, 9 | BRF_GRA },	      // 13 Sprites
	{ "p15.k2",		0x02000, 0xa74149d4, 9 | BRF_GRA },	      // 14
	{ "p14.k1",		0x02000, 0x07e807bc, 9 | BRF_GRA },	      // 15

	{ "m3-prom.j3",		0x00400, 0xd19e8a91, 14 | BRF_GRA },	      // 16 Color Data
	{ "m2-prom.j4",		0x00400, 0x9fc325af, 14 | BRF_GRA },	      // 17
	{ "m1-prom.j5",		0x00400, 0x07678443, 14 | BRF_GRA },	      // 18
};

STD_ROM_PICK(madcrush)
STD_ROM_FN(madcrush)

struct BurnDriver BurnDrvMadcrush = {
	"madcrush", "madcrash", NULL, NULL, "1984",
	"Mad Crusher (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, madcrushRomInfo, madcrushRomName, NULL, NULL, MadcrashInputInfo, MadcrashDIPInfo,
	MadcrushInit, DrvExit, MarvinsFrame, MarvinsDraw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// Vanguard II

static struct BurnRomInfo vangrd2RomDesc[] = {
	{ "p1.9a",		0x02000, 0xbc9eeca5, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "p3.11a",		0x02000, 0x3970f69d, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "p2.12a",		0x02000, 0x58b08b58, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "p4.14a",		0x02000, 0xa95f11ea, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "p5.4a",		0x02000, 0xe4dfd0ba, 2 | BRF_ESS | BRF_PRG }, //  4 Z80 #1 Code
	{ "p6.6a",		0x02000, 0x894ff00d, 2 | BRF_ESS | BRF_PRG }, //  5
	{ "p7.7a",		0x02000, 0x40b4d069, 2 | BRF_ESS | BRF_PRG }, //  6

	{ "p8.6a",		0x02000, 0xa3daa438, 3 | BRF_ESS | BRF_PRG }, //  7 Z80 #2 Code
	{ "p9.8a",		0x02000, 0x9345101a, 3 | BRF_ESS | BRF_PRG }, //  8

	{ "p15.1e",		0x02000, 0x85718a41, 4 | BRF_GRA },	      //  9 Text Characters

	{ "p13.1a",		0x02000, 0x912f22c6, 6 | BRF_GRA },	      // 10 Foreground Characters

	{ "p9",			0x02000, 0x7aa0b684, 13 | BRF_GRA },	      // 11 Background Characters

	{ "p10.4kl",		0x02000, 0x5bfc04c0, 9 | BRF_GRA },	      // 12 Sprites
	{ "p11.3kl",		0x02000, 0x620cd4ec, 9 | BRF_GRA },	      // 13
	{ "p12.1kl",		0x02000, 0x8658ea6c, 9 | BRF_GRA },	      // 14

	{ "mb7054.3j",		0x00400, 0x506f659a, 14 | BRF_GRA },	      // 15 Color Data
	{ "mb7054.4j",		0x00400, 0x222133ce, 14 | BRF_GRA },	      // 16
	{ "mb7054.5j",		0x00400, 0x2e21a79b, 14 | BRF_GRA },	      // 17
};

STD_ROM_PICK(vangrd2)
STD_ROM_FN(vangrd2)

struct BurnDriver BurnDrvVangrd2 = {
	"vangrd2", NULL, NULL, NULL, "1984",
	"Vanguard II\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, vangrd2RomInfo, vangrd2RomName, NULL, NULL, Vangrd2InputInfo, Vangrd2DIPInfo,
	MadcrashInit, DrvExit, MarvinsFrame, MarvinsDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Jumping Cross

static struct BurnRomInfo jcrossRomDesc[] = {
	{ "jcrossa0.10b",	0x02000, 0x0e79bbcd, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "jcrossa1.12b",	0x02000, 0x999b2bcc, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "jcrossa2.13b",	0x02000, 0xac89e49c, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "jcrossa3.14b",	0x02000, 0x4fd7848d, 1 | BRF_ESS | BRF_PRG }, //  3
	{ "jcrossa4.15b",	0x02000, 0x8500575d, 1 | BRF_ESS | BRF_PRG }, //  4

	{ "jcrossb0.15a",	0x02000, 0x77ed51e7, 2 | BRF_ESS | BRF_PRG }, //  5 Z80 #1 Code
	{ "jcrossb1.14a",	0x02000, 0x23cf0f70, 2 | BRF_ESS | BRF_PRG }, //  6
	{ "jcrossb2.13a",	0x02000, 0x5bed3118, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "jcrossb3.12a",	0x02000, 0xcd75dc95, 2 | BRF_ESS | BRF_PRG }, //  8

	{ "jcrosss0.f1",	0x02000, 0x9ae8ea93, 3 | BRF_ESS | BRF_PRG }, //  9 Z80 #2 Code
	{ "jcrosss1.h2",	0x02000, 0x83785601, 3 | BRF_ESS | BRF_PRG }, // 10

	{ "jcrossb4.10a",	0x02000, 0x08ad93fe, 4 | BRF_GRA },	      // 11 Text Characters
	{ "jcrosss.d2",		0x02000, 0x3ebb5beb, 4 | BRF_GRA },           // 12

	{ "jcrossb1.a2",	0x02000, 0xea3dfbc9, 6 | BRF_GRA },           // 13 Background Characters

	{ "jcrossf0.l2",	0x02000, 0x4532509b, 9 | BRF_GRA },           // 14 Sprites
	{ "jcrossf1.k2",	0x02000, 0x70d219bf, 9 | BRF_GRA },           // 15
	{ "jcrossf2.j2",	0x02000, 0x42a12b9d, 9 | BRF_GRA },           // 16

	{ "jcrossp2.j7",	0x00400, 0xb72a96a5, 14 | BRF_GRA },          // 17 Color Data
	{ "jcrossp1.j8",	0x00400, 0x35650448, 14 | BRF_GRA },          // 18
	{ "jcrossp0.j9",	0x00400, 0x99f54d48, 14 | BRF_GRA },          // 19
};

STD_ROM_PICK(jcross)
STD_ROM_FN(jcross)

struct BurnDriver BurnDrvJcross = {
	"jcross", NULL, NULL, NULL, "1984",
	"Jumping Cross\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, jcrossRomInfo, jcrossRomName, NULL, NULL, JcrossInputInfo, JcrossDIPInfo,
	JcrossInit, DrvExit, JcrossFrame, JcrossDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Gladiator 1984

static struct BurnRomInfo sgladiatRomDesc[] = {
	{ "glad.005",		0x04000, 0x4bc60f0b, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "glad.004",		0x04000, 0xdb557f46, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "glad.003",		0x02000, 0x55ce82b4, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "glad.002",		0x04000, 0x8350261c, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "glad.001",		0x04000, 0x5ee9d3fb, 2 | BRF_ESS | BRF_PRG }, //  4

	{ "glad.007",		0x02000, 0xc25b6731, 3 | BRF_ESS | BRF_PRG }, //  5 Z80 #2 Code
	{ "glad.006",		0x02000, 0x2024d716, 3 | BRF_ESS | BRF_PRG }, //  6

	{ "glad.011",		0x02000, 0x305bb294, 4 | BRF_GRA },	      //  7 Text Characters

	{ "glad.012",		0x02000, 0xb7dd519f, 6 | BRF_GRA },           //  8 Background Characters

	{ "glad.008",		0x02000, 0xbcf42587, 9 | BRF_GRA },           //  9 Sprites
	{ "glad.009",		0x02000, 0x912a20e0, 9 | BRF_GRA },           // 10
	{ "glad.010",		0x02000, 0x8b1db3a5, 9 | BRF_GRA },           // 11

	{ "82s137.001",		0x00400, 0xd9184823, 14 | BRF_GRA },          // 12 Color Data
	{ "82s137.002",		0x00400, 0x1a6b0953, 14 | BRF_GRA },          // 13
	{ "82s137.003",		0x00400, 0xc0e70308, 14 | BRF_GRA },          // 14
};

STD_ROM_PICK(sgladiat)
STD_ROM_FN(sgladiat)

struct BurnDriver BurnDrvSgladiat = {
	"sgladiat", NULL, NULL, NULL, "1984",
	"Gladiator 1984\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_RACING, 0,
	NULL, sgladiatRomInfo, sgladiatRomName, NULL, NULL, SgladiatInputInfo, SgladiatDIPInfo,
	SgladiatInit, DrvExit, JcrossFrame, JcrossDraw, DrvScan, &DrvRecalc, 0x400,
	288, 224, 4, 3
};


// HAL21

static struct BurnRomInfo hal21RomDesc[] = {
	{ "hal21p1.bin",	0x02000, 0x9d193830, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "hal21p2.bin",	0x02000, 0xc1f00350, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "hal21p3.bin",	0x02000, 0x881d22a6, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "hal21p4.bin",	0x02000, 0xce692534, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "hal21p5.bin",	0x02000, 0x3ce0684a, 2 | BRF_ESS | BRF_PRG }, //  4 Z80 #1 Code
	{ "hal21p6.bin",	0x02000, 0x878ef798, 2 | BRF_ESS | BRF_PRG }, //  5
	{ "hal21p7.bin",	0x02000, 0x72ebbe95, 2 | BRF_ESS | BRF_PRG }, //  6
	{ "hal21p8.bin",	0x02000, 0x17e22ad3, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "hal21p9.bin",	0x02000, 0xb146f891, 2 | BRF_ESS | BRF_PRG }, //  8

	{ "hal21p10.bin",	0x04000, 0x916f7ba0, 3 | BRF_ESS | BRF_PRG }, //  9 Z80 #2 Code

	{ "hal21p12.bin",	0x02000, 0x9839a7cd, 4 | BRF_GRA },	      // 10 Text Characters

	{ "hal21p11.bin",	0x04000, 0x24abc57e, 6 | BRF_GRA },           // 11 Background Characters

	{ "hal21p13.bin",	0x04000, 0x052b4f4f, 9 | BRF_GRA },           // 12 Sprites
	{ "hal21p14.bin",	0x04000, 0xda0cb670, 9 | BRF_GRA },           // 13
	{ "hal21p15.bin",	0x04000, 0x5c5ea945, 9 | BRF_GRA },           // 14

	{ "hal21_3.prm",	0x00400, 0x605afff8, 14 | BRF_GRA },          // 15 Color Data
	{ "hal21_2.prm",	0x00400, 0xc5d84225, 14 | BRF_GRA },          // 16
	{ "hal21_1.prm",	0x00400, 0x195768fc, 14 | BRF_GRA },          // 17
};

STD_ROM_PICK(hal21)
STD_ROM_FN(hal21)

struct BurnDriver BurnDrvHal21 = {
	"hal21", NULL, NULL, NULL, "1984",
	"HAL21\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, hal21RomInfo, hal21RomName, NULL, NULL, Hal21InputInfo, Hal21DIPInfo,
	Hal21Init, DrvExit, JcrossFrame, Hal21Draw, DrvScan, &DrvRecalc, 0x400,
	224, 288, 3, 4
};


// HAL21 (Japan)

static struct BurnRomInfo hal21jRomDesc[] = {
	{ "hal21p1.bin",	0x02000, 0x9d193830, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "hal21p2.bin",	0x02000, 0xc1f00350, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "hal21p3.bin",	0x02000, 0x881d22a6, 1 | BRF_ESS | BRF_PRG }, //  2
	{ "hal21p4.bin",	0x02000, 0xce692534, 1 | BRF_ESS | BRF_PRG }, //  3

	{ "hal21p5.bin",	0x02000, 0x3ce0684a, 2 | BRF_ESS | BRF_PRG }, //  4 Z80 #1 Code
	{ "hal21p6.bin",	0x02000, 0x878ef798, 2 | BRF_ESS | BRF_PRG }, //  5
	{ "hal21p7.bin",	0x02000, 0x72ebbe95, 2 | BRF_ESS | BRF_PRG }, //  6
	{ "hal21p8.bin",	0x02000, 0x17e22ad3, 2 | BRF_ESS | BRF_PRG }, //  7
	{ "hal21p9.bin",	0x02000, 0xb146f891, 2 | BRF_ESS | BRF_PRG }, //  8

	{ "hal21-10.bin",	0x04000, 0xa182b3f0, 3 | BRF_ESS | BRF_PRG }, //  9 Z80 #2 Code

	{ "hal21p12.bin",	0x02000, 0x9839a7cd, 4 | BRF_GRA },	      // 10 Text Characters

	{ "hal21p11.bin",	0x04000, 0x24abc57e, 6 | BRF_GRA },           // 11 Background Characters

	{ "hal21p13.bin",	0x04000, 0x052b4f4f, 9 | BRF_GRA },           // 12 Sprites
	{ "hal21p14.bin",	0x04000, 0xda0cb670, 9 | BRF_GRA },           // 13
	{ "hal21p15.bin",	0x04000, 0x5c5ea945, 9 | BRF_GRA },           // 14

	{ "hal21_3.prm",	0x00400, 0x605afff8, 14 | BRF_GRA },          // 15 Color Data
	{ "hal21_2.prm",	0x00400, 0xc5d84225, 14 | BRF_GRA },          // 16
	{ "hal21_1.prm",	0x00400, 0x195768fc, 14 | BRF_GRA },          // 17
};

STD_ROM_PICK(hal21j)
STD_ROM_FN(hal21j)

struct BurnDriver BurnDrvHal21j = {
	"hal21j", "hal21", NULL, NULL, "1985",
	"HAL21 (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, hal21jRomInfo, hal21jRomName, NULL, NULL, Hal21InputInfo, Hal21DIPInfo,
	Hal21Init, DrvExit, JcrossFrame, Hal21Draw, DrvScan, &DrvRecalc, 0x400,
	224, 288, 3, 4
};


// Psycho Soldier (US)

static struct BurnRomInfo psychosRomDesc[] = {
	{ "p7",			0x10000, 0x562809f4, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "up03_m8.rom",	0x10000, 0x5f426ddb, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "p5",			0x10000, 0x64503283, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "up03_k1.rom",	0x00400, 0x27b8ca8c, 14 | BRF_GRA },	      //  3 Color Data
	{ "up03_l1.rom",	0x00400, 0x40e78c9e, 14 | BRF_GRA },	      //  4
	{ "up03_k2.rom",	0x00400, 0xd845d5ac, 14 | BRF_GRA },	      //  5
	{ "mb7122e.8j",		0x00400, 0xc20b197b, 0 | BRF_GRA },	      //  6
	{ "mb7122e.8k",		0x00400, 0x5d0c617f, 0 | BRF_GRA },	      //  7

	{ "up02_a3.rom",	0x08000, 0x11a71919, 4 | BRF_GRA },	      //  8 Text Characters

	{ "up01_f1.rom",	0x10000, 0x167e5765, 5 | BRF_GRA },	      //  9 Background Layer
	{ "up01_d1.rom",	0x10000, 0x8b0fe8d0, 5 | BRF_GRA },	      // 10
	{ "up01_c1.rom",	0x10000, 0xf4361c50, 5 | BRF_GRA },	      // 11
	{ "up01_a1.rom",	0x10000, 0xe4b0b95e, 5 | BRF_GRA },	      // 12

	{ "up02_f3.rom",	0x08000, 0xf96f82db, 7 | BRF_GRA },	      // 13 16x16 Sprites
	{ "up02_e3.rom",	0x08000, 0x2b007733, 7 | BRF_GRA },	      // 14
	{ "up02_c3.rom",	0x08000, 0xefa830e1, 7 | BRF_GRA },	      // 15
	{ "up02_b3.rom",	0x08000, 0x24559ee1, 7 | BRF_GRA },	      // 16

	{ "up01_f10.rom",	0x10000, 0x2bac250e, 8 | BRF_GRA },	      // 17 32x32 Sprites
	{ "up01_h10.rom",	0x10000, 0x5e1ba353, 8 | BRF_GRA },	      // 18
	{ "up01_j10.rom",	0x10000, 0x9ff91a97, 8 | BRF_GRA },	      // 19
	{ "up01_l10.rom",	0x10000, 0xae1965ef, 8 | BRF_GRA },	      // 20
	{ "up01_m10.rom",	0x10000, 0xdf283b67, 8 | BRF_GRA },	      // 21
	{ "up01_n10.rom",	0x10000, 0x914f051f, 8 | BRF_GRA },	      // 22
	{ "up01_r10.rom",	0x10000, 0xc4488472, 8 | BRF_GRA },	      // 23
	{ "up01_s10.rom",	0x10000, 0x8ec7fe18, 8 | BRF_GRA },	      // 24

	{ "p1",			0x10000, 0x58f1683f, 15 | BRF_SND },	      // 25 Samples
	{ "p2",			0x10000, 0xda3abda1, 15 | BRF_SND },	      // 26
	{ "p3",			0x10000, 0xf3683ae8, 15 | BRF_SND },	      // 27
	{ "p4",			0x10000, 0x437d775a, 15 | BRF_SND },	      // 28
};

STD_ROM_PICK(psychos)
STD_ROM_FN(psychos)

struct BurnDriver BurnDrvPsychos = {
	"psychos", NULL, NULL, NULL, "1987",
	"Psycho Soldier (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, psychosRomInfo, psychosRomName, NULL, NULL, PsychosInputInfo, PsychosDIPInfo,
	PsychosInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// Psycho Soldier (Japan)

static struct BurnRomInfo psychosjRomDesc[] = {
	{ "up03_m4.rom",	0x10000, 0x05dfb409, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "up03_m8.rom",	0x10000, 0x5f426ddb, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "up03_j6.rom",	0x10000, 0xbbd0a8e3, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "up03_k1.rom",	0x00400, 0x27b8ca8c, 14 | BRF_GRA },	      //  3 Color Data
	{ "up03_l1.rom",	0x00400, 0x40e78c9e, 14 | BRF_GRA },	      //  4
	{ "up03_k2.rom",	0x00400, 0xd845d5ac, 14 | BRF_GRA },	      //  5
	{ "mb7122e.8j",		0x00400, 0xc20b197b, 0 | BRF_GRA },	      //  6
	{ "mb7122e.8k",		0x00400, 0x5d0c617f, 0 | BRF_GRA },	      //  7

	{ "up02_a3.rom",	0x08000, 0x11a71919, 4 | BRF_GRA },	      //  8 Text Characters

	{ "up01_f1.rom",	0x10000, 0x167e5765, 5 | BRF_GRA },	      //  9 Background Layer
	{ "up01_d1.rom",	0x10000, 0x8b0fe8d0, 5 | BRF_GRA },	      // 10
	{ "up01_c1.rom",	0x10000, 0xf4361c50, 5 | BRF_GRA },	      // 11
	{ "up01_a1.rom",	0x10000, 0xe4b0b95e, 5 | BRF_GRA },	      // 12

	{ "up02_f3.rom",	0x08000, 0xf96f82db, 7 | BRF_GRA },	      // 13 16x16 Sprites
	{ "up02_e3.rom",	0x08000, 0x2b007733, 7 | BRF_GRA },	      // 14
	{ "up02_c3.rom",	0x08000, 0xefa830e1, 7 | BRF_GRA },	      // 15
	{ "up02_b3.rom",	0x08000, 0x24559ee1, 7 | BRF_GRA },	      // 16

	{ "up01_f10.rom",	0x10000, 0x2bac250e, 8 | BRF_GRA },	      // 17 32x32 Sprites
	{ "up01_h10.rom",	0x10000, 0x5e1ba353, 8 | BRF_GRA },	      // 18
	{ "up01_j10.rom",	0x10000, 0x9ff91a97, 8 | BRF_GRA },	      // 19
	{ "up01_l10.rom",	0x10000, 0xae1965ef, 8 | BRF_GRA },	      // 20
	{ "up01_m10.rom",	0x10000, 0xdf283b67, 8 | BRF_GRA },	      // 21
	{ "up01_n10.rom",	0x10000, 0x914f051f, 8 | BRF_GRA },	      // 22
	{ "up01_r10.rom",	0x10000, 0xc4488472, 8 | BRF_GRA },	      // 23
	{ "up01_s10.rom",	0x10000, 0x8ec7fe18, 8 | BRF_GRA },	      // 24

	{ "up03_b5.rom",	0x10000, 0x0f8e8276, 15 | BRF_SND },	      // 25 Samples
	{ "up03_c5.rom",	0x10000, 0x34e41dfb, 15 | BRF_SND },	      // 26
	{ "up03_d5.rom",	0x10000, 0xaa583c5e, 15 | BRF_SND },	      // 27
	{ "up03_f5.rom",	0x10000, 0x7e8bce7a, 15 | BRF_SND },	      // 28
};

STD_ROM_PICK(psychosj)
STD_ROM_FN(psychosj)

struct BurnDriver BurnDrvPsychosj = {
	"psychosj", "psychos", NULL, NULL, "1987",
	"Psycho Soldier (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SCRFIGHT, 0,
	NULL, psychosjRomInfo, psychosjRomName, NULL, NULL, PsychosInputInfo, PsychosDIPInfo,
	PsychosInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// Bermuda Triangle (World?)

static struct BurnRomInfo bermudatRomDesc[] = {
	{ "bt_p1.rom",		0x10000, 0x43dec5e9, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "bt_p2.rom",		0x10000, 0x0e193265, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "bt_p3.rom",		0x10000, 0x53a82e50, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "btj_01r.prm",	0x00400, 0xf4b54d06, 14 | BRF_GRA },	      //  3 Color Data
	{ "btj_02g.prm",	0x00400, 0xbaac139e, 14 | BRF_GRA },	      //  4
	{ "btj_03b.prm",	0x00400, 0x2edf2e0b, 14 | BRF_GRA },	      //  5
	{ "btj_h.prm",		0x00400, 0xc20b197b, 0 | BRF_OPT },	      //  6
	{ "btj_v.prm",		0x00400, 0x5d0c617f, 0 | BRF_OPT },	      //  7

	{ "bt_p10.rom",		0x08000, 0xd3650211, 4 | BRF_GRA },	      //  8 Text Characters

	{ "bt_p22.rom",		0x10000, 0x8daf7df4, 5 | BRF_GRA },	      //  9 Background Layer
	{ "bt_p21.rom",		0x10000, 0xb7689599, 5 | BRF_GRA },	      // 10
	{ "bt_p20.rom",		0x10000, 0xab6217b7, 5 | BRF_GRA },	      // 11
	{ "bt_p19.rom",		0x10000, 0x8ed759a0, 5 | BRF_GRA },	      // 12

	{ "bt_p6.rom",		0x08000, 0x8ffdf969, 7 | BRF_GRA },	      // 13 16x16 Sprites
	{ "bt_p7.rom",		0x08000, 0x268d10df, 7 | BRF_GRA },	      // 14
	{ "bt_p8.rom",		0x08000, 0x3e39e9dd, 7 | BRF_GRA },	      // 15
	{ "bt_p9.rom",		0x08000, 0xbf56da61, 7 | BRF_GRA },	      // 16

	{ "bt_p11.rom",		0x10000, 0xaae7410e, 8 | BRF_GRA },	      // 17 32x32 Sprites
	{ "bt_p12.rom",		0x10000, 0x18914f70, 8 | BRF_GRA },	      // 18
	{ "bt_p13.rom",		0x10000, 0xcd79ce81, 8 | BRF_GRA },	      // 19
	{ "bt_p14.rom",		0x10000, 0xedc57117, 8 | BRF_GRA },	      // 20
	{ "bt_p15.rom",		0x10000, 0x448bf9f4, 8 | BRF_GRA },	      // 21
	{ "bt_p16.rom",		0x10000, 0x119999eb, 8 | BRF_GRA },	      // 22
	{ "bt_p17.rom",		0x10000, 0xb5462139, 8 | BRF_GRA },	      // 23
	{ "bt_p18.rom",		0x10000, 0xcb416227, 8 | BRF_GRA },	      // 24

	{ "bt_p4.rom",		0x10000, 0x4bc83229, 15 | BRF_SND },	      // 25 Samples
	{ "bt_p5.rom",		0x10000, 0x817bd62c, 15 | BRF_SND },	      // 26
};

STD_ROM_PICK(bermudat)
STD_ROM_FN(bermudat)

struct BurnDriver BurnDrvBermudat = {
	"bermudat", NULL, NULL, NULL, "1987",
	"Bermuda Triangle (World?)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, bermudatRomInfo, bermudatRomName, NULL, NULL, BermudatInputInfo, BermudatDIPInfo,
	BermudatInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Bermuda Triangle (Japan)

static struct BurnRomInfo bermudatjRomDesc[] = {
	{ "btj_p01.bin",	0x10000, 0xeda75f36, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "bt_p2.rom",		0x10000, 0x0e193265, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "btj_p03.bin",	0x10000, 0xfea8a096, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "btj_01r.prm",	0x00400, 0xf4b54d06, 14 | BRF_GRA },	      //  3 Color Data
	{ "btj_02g.prm",	0x00400, 0xbaac139e, 14 | BRF_GRA },	      //  4
	{ "btj_03b.prm",	0x00400, 0x2edf2e0b, 14 | BRF_GRA },	      //  5
	{ "btj_h.prm",		0x00400, 0xc20b197b, 0 | BRF_OPT },	      //  6
	{ "btj_v.prm",		0x00400, 0x5d0c617f, 0 | BRF_OPT },	      //  7

	{ "bt_p10.rom",		0x08000, 0xd3650211, 4 | BRF_GRA },	      //  8 Text Characters

	{ "bt_p22.rom",		0x10000, 0x8daf7df4, 5 | BRF_GRA },	      //  9 Background Layer
	{ "bt_p21.rom",		0x10000, 0xb7689599, 5 | BRF_GRA },	      // 10
	{ "bt_p20.rom",		0x10000, 0xab6217b7, 5 | BRF_GRA },	      // 11
	{ "bt_p19.rom",		0x10000, 0x8ed759a0, 5 | BRF_GRA },	      // 12

	{ "bt_p6.rom",		0x08000, 0x8ffdf969, 7 | BRF_GRA },	      // 13 16x16 Sprites
	{ "bt_p7.rom",		0x08000, 0x268d10df, 7 | BRF_GRA },	      // 14
	{ "bt_p8.rom",		0x08000, 0x3e39e9dd, 7 | BRF_GRA },	      // 15
	{ "bt_p9.rom",		0x08000, 0xbf56da61, 7 | BRF_GRA },	      // 16

	{ "bt_p11.rom",		0x10000, 0xaae7410e, 8 | BRF_GRA },	      // 17 32x32 Sprites
	{ "bt_p12.rom",		0x10000, 0x18914f70, 8 | BRF_GRA },	      // 18
	{ "bt_p13.rom",		0x10000, 0xcd79ce81, 8 | BRF_GRA },	      // 19
	{ "bt_p14.rom",		0x10000, 0xedc57117, 8 | BRF_GRA },	      // 20
	{ "bt_p15.rom",		0x10000, 0x448bf9f4, 8 | BRF_GRA },	      // 21
	{ "bt_p16.rom",		0x10000, 0x119999eb, 8 | BRF_GRA },	      // 22
	{ "bt_p17.rom",		0x10000, 0xb5462139, 8 | BRF_GRA },	      // 23
	{ "bt_p18.rom",		0x10000, 0xcb416227, 8 | BRF_GRA },	      // 24

	{ "btj_p04.bin",	0x10000, 0xb2e01129, 15 | BRF_SND },	      // 25 Samples
	{ "btj_p05.bin",	0x10000, 0x924c24f7, 15 | BRF_SND },	      // 26
};

STD_ROM_PICK(bermudatj)
STD_ROM_FN(bermudatj)

struct BurnDriver BurnDrvBermudatj = {
	"bermudatj", "bermudat", NULL, NULL, "1987",
	"Bermuda Triangle (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, bermudatjRomInfo, bermudatjRomName, NULL, NULL, BermudatInputInfo, BermudatDIPInfo,
	BermudatInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// World Wars (World?)

static struct BurnRomInfo worldwarRomDesc[] = {
	{ "ww4.bin",		0x10000, 0xbc29d09f, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "ww5.bin",		0x10000, 0x8dc15909, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "ww3.bin",		0x10000, 0x8b74c951, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "ww_r.bin",		0x00400, 0xb88e95f0, 14 | BRF_GRA },	      //  3 Color Data
	{ "ww_g.bin",		0x00400, 0x5e1616b2, 14 | BRF_GRA },	      //  4
	{ "ww_b.bin",		0x00400, 0xe9770796, 14 | BRF_GRA },	      //  5
	{ "btj_h.prm",		0x00400, 0xc20b197b, 0 | BRF_OPT },	      //  6
	{ "btj_v.prm", 		0x00400, 0x5d0c617f, 0 | BRF_OPT },	      //  7

	{ "ww6.bin",		0x08000, 0xd57570ab, 4 | BRF_GRA },	      //  8 Text Characters

	{ "ww11.bin",		0x10000, 0x603ddcb5, 5 | BRF_GRA },	      //  9 Background Layer
	{ "ww12.bin",		0x10000, 0x388093ff, 5 | BRF_GRA },	      // 10
	{ "ww13.bin",		0x10000, 0x83a7ef62, 5 | BRF_GRA },	      // 11
	{ "ww14.bin",		0x10000, 0x04c784be, 5 | BRF_GRA },	      // 12

	{ "ww10.bin",		0x08000, 0xf68a2d51, 7 | BRF_GRA },	      // 13 16x16 Sprites
	{ "ww9.bin",		0x08000, 0xd9d35911, 7 | BRF_GRA },	      // 14
	{ "ww8.bin",		0x08000, 0x0ec15086, 7 | BRF_GRA },	      // 15
	{ "ww7.bin",		0x08000, 0x53c4b24e, 7 | BRF_GRA },	      // 16

	{ "ww21.bin",		0x10000, 0xbe974fbe, 8 | BRF_GRA },	      // 17 32x32 Sprites
	{ "ww22.bin",		0x10000, 0x9914972a, 8 | BRF_GRA },	      // 18
	{ "ww19.bin",		0x10000, 0xc39ac1a7, 8 | BRF_GRA },	      // 19
	{ "ww20.bin",		0x10000, 0x8504170f, 8 | BRF_GRA },	      // 20
	{ "ww15.bin",		0x10000, 0xd55ce063, 8 | BRF_GRA },	      // 21
	{ "ww16.bin",		0x10000, 0xa2d19ce5, 8 | BRF_GRA },	      // 22
	{ "ww17.bin",		0x10000, 0xa9a6b128, 8 | BRF_GRA },	      // 23
	{ "ww18.bin",		0x10000, 0xc712d24c, 8 | BRF_GRA },	      // 24

	{ "bt_p4.rom",  	0x10000, 0x4bc83229, 15 | BRF_SND },	      // 25 Samples
	{ "bt_p5.rom",		0x10000, 0x817bd62c, 15 | BRF_SND },	      // 26
};

STD_ROM_PICK(worldwar)
STD_ROM_FN(worldwar)

struct BurnDriver BurnDrvWorldwar = {
	"worldwar", NULL, NULL, NULL, "1987",
	"World Wars (World?)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, worldwarRomInfo, worldwarRomName, NULL, NULL, BermudatInputInfo, BermudatDIPInfo,
	BermudatInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Bermuda Triangle (World Wars) (US)

static struct BurnRomInfo bermudataRomDesc[] = {
	{ "4",			0x10000, 0x4de39d01, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "5",			0x10000, 0x76158e94, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "3",			0x10000, 0xc79134a8, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "mb7122e.1k",		0x00400, 0x1e8fc4c3, 14 | BRF_GRA },	      //  3 proms
	{ "mb7122e.2l",		0x00400, 0x23ce9707, 14 | BRF_GRA },	      //  4
	{ "mb7122e.1l",		0x00400, 0x26caf985, 14 | BRF_GRA },	      //  5
	{ "btj_h.prm",		0x00400, 0xc20b197b, 0 | BRF_OPT },	      //  6
	{ "btj_v.prm",		0x00400, 0x5d0c617f, 0 | BRF_OPT },	      //  7

	{ "6",			0x08000, 0xa0e6710c, 4 | BRF_GRA },	      //  8 Text Characters

	{ "ww11.bin",		0x10000, 0x603ddcb5, 5 | BRF_GRA },	      //  9 Background Layer
	{ "ww12.bin",		0x10000, 0x388093ff, 5 | BRF_GRA },	      // 10
	{ "ww13.bin",		0x10000, 0x83a7ef62, 5 | BRF_GRA },	      // 11
	{ "ww14.bin",		0x10000, 0x04c784be, 5 | BRF_GRA },	      // 12

	{ "ww10.bin",		0x08000, 0xf68a2d51, 7 | BRF_GRA },	      // 13 16x16 Sprites
	{ "ww9.bin",		0x08000, 0xd9d35911, 7 | BRF_GRA },	      // 14
	{ "ww8.bin",		0x08000, 0x0ec15086, 7 | BRF_GRA },	      // 15
	{ "ww7.bin",		0x08000, 0x53c4b24e, 7 | BRF_GRA },	      // 16

	{ "ww21.bin",		0x10000, 0xbe974fbe, 8 | BRF_GRA },	      // 17 32x32 Sprites
	{ "ww22.bin",		0x10000, 0x9914972a, 8 | BRF_GRA },	      // 18
	{ "ww19.bin",		0x10000, 0xc39ac1a7, 8 | BRF_GRA },	      // 19
	{ "ww20.bin",		0x10000, 0x8504170f, 8 | BRF_GRA },	      // 20
	{ "ww15.bin",		0x10000, 0xd55ce063, 8 | BRF_GRA },	      // 21
	{ "ww16.bin",		0x10000, 0xa2d19ce5, 8 | BRF_GRA },	      // 22
	{ "ww17.bin",		0x10000, 0xa9a6b128, 8 | BRF_GRA },	      // 23
	{ "ww18.bin",		0x10000, 0xc712d24c, 8 | BRF_GRA },	      // 24

	{ "bt_p4.rom",  	0x10000, 0x4bc83229, 15 | BRF_SND },	      // 25 Samples
	{ "bt_p5.rom",		0x10000, 0x817bd62c, 15 | BRF_SND },	      // 26
};

STD_ROM_PICK(bermudata)
STD_ROM_FN(bermudata)

struct BurnDriver BurnDrvBermudata = {
	"bermudata", "worldwar", NULL, NULL, "1987",
	"Bermuda Triangle (World Wars) (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, bermudataRomInfo, bermudataRomName, NULL, NULL, BermudatInputInfo, BermudatDIPInfo,
	BermudatInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Guerrilla War (US)

static struct BurnRomInfo gwarRomDesc[] = {
	{ "7g",			0x10000, 0x5bcfa7dc, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "g02",		0x10000, 0x86d931bf, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "g03",		0x10000, 0xeb544ab9, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "guprom.3",		0x00400, 0x090236a3, 14 | BRF_GRA },	      //  3 Color Data
	{ "guprom.2",		0x00400, 0x9147de69, 14 | BRF_GRA },	      //  4
	{ "guprom.1",		0x00400, 0x7f9c839e, 14 | BRF_GRA },	      //  5
	{ "btj_h.prm",		0x00400, 0xc20b197b, 0 | BRF_OPT },	      //  6
	{ "btj_v.prm",		0x00400, 0x5d0c617f, 0 | BRF_OPT },	      //  7
	{ "ls.bin",		0x01000, 0x73df921d, 0 | BRF_OPT },	      //  8

	{ "g05",		0x08000, 0x80f73e2e, 4 | BRF_GRA },	      //  9 Text Characters

	{ "g06",		0x10000, 0xf1dcdaef, 5 | BRF_GRA },	      // 10 Background Layer
	{ "g07",		0x10000, 0x326e4e5e, 5 | BRF_GRA },	      // 11
	{ "g08",		0x10000, 0x0aa70967, 5 | BRF_GRA },	      // 12
	{ "g09",		0x10000, 0xb7686336, 5 | BRF_GRA },	      // 13

	{ "g10",		0x10000, 0x58600f7d, 7 | BRF_GRA },	      // 14 16x16 Sprites
	{ "g11",		0x10000, 0xa3f9b463, 7 | BRF_GRA },	      // 15
	{ "g12",		0x10000, 0x092501be, 7 | BRF_GRA },	      // 16
	{ "g13",		0x10000, 0x25801ea6, 7 | BRF_GRA },	      // 17

	{ "g20",		0x10000, 0x2b46edff, 8 | BRF_GRA },	      // 18 32x32 Sprites
	{ "g21",		0x10000, 0xbe19888d, 8 | BRF_GRA },	      // 19
	{ "g18",		0x10000, 0x2d653f0c, 8 | BRF_GRA },	      // 21
	{ "g19",		0x10000, 0xebbf3ba2, 8 | BRF_GRA },	      // 22
	{ "g16",		0x10000, 0xaeb3707f, 8 | BRF_GRA },	      // 23
	{ "g17",		0x10000, 0x0808f95f, 8 | BRF_GRA },	      // 24
	{ "g14",		0x10000, 0x8dfc7b87, 8 | BRF_GRA },	      // 25
	{ "g15",		0x10000, 0x06822aac, 8 | BRF_GRA },	      // 26

	{ "g04",		0x10000, 0x2255f8dd, 15 | BRF_SND },	      // 27 Samples
};

STD_ROM_PICK(gwar)
STD_ROM_FN(gwar)

struct BurnDriver BurnDrvGwar = {
	"gwar", NULL, NULL, NULL, "1987",
	"Guerrilla War (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gwarRomInfo, gwarRomName, NULL, NULL, GwarInputInfo, GwarDIPInfo,
	GwarInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Guevara (Japan)

static struct BurnRomInfo gwarjRomDesc[] = {
	{ "7y3047",		0x10000, 0x7f8a880c, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "g02",		0x10000, 0x86d931bf, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "g03",		0x10000, 0xeb544ab9, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "guprom.3",		0x00400, 0x090236a3, 14 | BRF_GRA },	      //  3 Color Data
	{ "guprom.2",		0x00400, 0x9147de69, 14 | BRF_GRA },	      //  4
	{ "guprom.1",		0x00400, 0x7f9c839e, 14 | BRF_GRA },	      //  5
	{ "btj_h.prm",		0x00400, 0xc20b197b, 0 | BRF_OPT },	      //  6
	{ "btj_v.prm",		0x00400, 0x5d0c617f, 0 | BRF_OPT },	      //  7
	{ "ls.bin",		0x01000, 0x73df921d, 0 | BRF_OPT },	      //  8

	{ "792001",		0x08000, 0x99d7ddf3, 4 | BRF_GRA },	      //  9 Text Characters

	{ "g06",		0x10000, 0xf1dcdaef, 5 | BRF_GRA },	      // 10 Background Layer
	{ "g07",		0x10000, 0x326e4e5e, 5 | BRF_GRA },	      // 11
	{ "g08",		0x10000, 0x0aa70967, 5 | BRF_GRA },	      // 12
	{ "g09",		0x10000, 0xb7686336, 5 | BRF_GRA },	      // 13

	{ "g10",		0x10000, 0x58600f7d, 7 | BRF_GRA },	      // 14 16x16 Sprites
	{ "g11",		0x10000, 0xa3f9b463, 7 | BRF_GRA },	      // 15
	{ "g12",		0x10000, 0x092501be, 7 | BRF_GRA },	      // 16
	{ "g13",		0x10000, 0x25801ea6, 7 | BRF_GRA },	      // 17

	{ "g20",		0x10000, 0x2b46edff, 8 | BRF_GRA },	      // 18 32x32 Sprites
	{ "g21",		0x10000, 0xbe19888d, 8 | BRF_GRA },	      // 19
	{ "g18",		0x10000, 0x2d653f0c, 8 | BRF_GRA },	      // 21
	{ "g19",		0x10000, 0xebbf3ba2, 8 | BRF_GRA },	      // 22
	{ "g16",		0x10000, 0xaeb3707f, 8 | BRF_GRA },	      // 23
	{ "g17",		0x10000, 0x0808f95f, 8 | BRF_GRA },	      // 24
	{ "g14",		0x10000, 0x8dfc7b87, 8 | BRF_GRA },	      // 25
	{ "g15",		0x10000, 0x06822aac, 8 | BRF_GRA },	      // 26

	{ "g04",		0x10000, 0x2255f8dd, 15 | BRF_SND },	      // 27 Samples
};

STD_ROM_PICK(gwarj)
STD_ROM_FN(gwarj)

struct BurnDriver BurnDrvGwarj = {
	"gwarj", "gwar", NULL, NULL, "1987",
	"Guevara (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gwarjRomInfo, gwarjRomName, NULL, NULL, GwarInputInfo, GwarDIPInfo,
	GwarInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Guerrilla War (Joystick hack bootleg)

static struct BurnRomInfo gwarbRomDesc[] = {
	{ "g01",		0x10000, 0xce1d3c80, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "g02",		0x10000, 0x86d931bf, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "g03",		0x10000, 0xeb544ab9, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "guprom.3",		0x00400, 0x090236a3, 14 | BRF_GRA },	      //  3 Color Data
	{ "guprom.2",		0x00400, 0x9147de69, 14 | BRF_GRA },	      //  4
	{ "guprom.1",		0x00400, 0x7f9c839e, 14 | BRF_GRA },	      //  5

	{ "g05",		0x08000, 0x80f73e2e, 4 | BRF_GRA },	      //  6 Text Characters

	{ "g06",		0x10000, 0xf1dcdaef, 5 | BRF_GRA },	      //  7 Background Layer
	{ "g07",		0x10000, 0x326e4e5e, 5 | BRF_GRA },	      //  8
	{ "g08",		0x10000, 0x0aa70967, 5 | BRF_GRA },	      //  9
	{ "g09",		0x10000, 0xb7686336, 5 | BRF_GRA },	      // 10

	{ "g10",		0x10000, 0x58600f7d, 7 | BRF_GRA },	      // 11 16x16 Sprites
	{ "g11",		0x10000, 0xa3f9b463, 7 | BRF_GRA },	      // 12
	{ "g12",		0x10000, 0x092501be, 7 | BRF_GRA },	      // 13
	{ "g13",		0x10000, 0x25801ea6, 7 | BRF_GRA },	      // 14

	{ "g20",		0x10000, 0x2b46edff, 8 | BRF_GRA },	      // 15 32x32 Sprites
	{ "g21",		0x10000, 0xbe19888d, 8 | BRF_GRA },	      // 16
	{ "g18",		0x10000, 0x2d653f0c, 8 | BRF_GRA },	      // 17
	{ "g19",		0x10000, 0xebbf3ba2, 8 | BRF_GRA },	      // 18
	{ "g16",		0x10000, 0xaeb3707f, 8 | BRF_GRA },	      // 19
	{ "g17",		0x10000, 0x0808f95f, 8 | BRF_GRA },	      // 20
	{ "g14",		0x10000, 0x8dfc7b87, 8 | BRF_GRA },	      // 21
	{ "g15",		0x10000, 0x06822aac, 8 | BRF_GRA },	      // 22

	{ "g04",		0x10000, 0x2255f8dd, 15 | BRF_SND },	      // 23 Samples
};

STD_ROM_PICK(gwarb)
STD_ROM_FN(gwarb)

struct BurnDriver BurnDrvGwarb = {
	"gwarb", "gwar", NULL, NULL, "1987",
	"Guerrilla War (Joystick hack bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gwarbRomInfo, gwarbRomName, NULL, NULL, GwarbInputInfo, GwarbDIPInfo,
	GwarbInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Guerrilla War (Version 1)

static struct BurnRomInfo gwaraRomDesc[] = {
	{ "gv ver1 3.p4",	0x10000, 0x24936d83, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "gv4.p8",		0x10000, 0x26335a55, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "gv2.k7",		0x10000, 0x896682dd, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "1.k1",		0x00400, 0x090236a3, 14 | BRF_GRA },	      //  3 Color Data
	{ "3.l2",		0x00400, 0x9147de69, 14 | BRF_GRA },	      //  4
	{ "2.l1",		0x00400, 0x7f9c839e, 14 | BRF_GRA },	      //  5
	{ "horizon.j8",		0x00400, 0xc20b197b, 0 | BRF_OPT },	      //  6
	{ "vertical.k8",	0x00400, 0x5d0c617f, 0 | BRF_OPT },	      //  7
	{ "ls.bin",		0x01000, 0x73df921d, 0 | BRF_OPT },	      //  8

	{ "gv5.a3",		0x08000, 0x80f73e2e, 4 | BRF_GRA },	      //  9 Text Characters

	{ "gv13.ef1",		0x10000, 0xf1dcdaef, 5 | BRF_GRA },	      // 10 Background Layer
	{ "gv12.d1",		0x10000, 0x326e4e5e, 5 | BRF_GRA },	      // 11
	{ "gv11.c1",		0x10000, 0x0aa70967, 5 | BRF_GRA },	      // 12
	{ "gv10.a1",		0x10000, 0xb7686336, 5 | BRF_GRA },	      // 13

	{ "gv9.g3",		0x10000, 0x58600f7d, 7 | BRF_GRA },	      // 14 16x16 Sprites
	{ "gv8.e3",		0x10000, 0xa3f9b463, 7 | BRF_GRA },	      // 15
	{ "gv7.cd3",		0x10000, 0x092501be, 7 | BRF_GRA },	      // 16
	{ "gv6.b3",		0x10000, 0x25801ea6, 7 | BRF_GRA },	      // 17

	{ "gv14.f10",		0x10000, 0x2b46edff, 8 | BRF_GRA },	      // 18 32x32 Sprites
	{ "gv15.h10",		0x10000, 0xbe19888d, 8 | BRF_GRA },	      // 19
	{ "gv16.j10",		0x10000, 0x2d653f0c, 8 | BRF_GRA },	      // 20
	{ "gv17.l10",		0x10000, 0xebbf3ba2, 8 | BRF_GRA },	      // 21
	{ "gv18.m10",		0x10000, 0xaeb3707f, 8 | BRF_GRA },	      // 22
	{ "gv19.pn10",		0x10000, 0x0808f95f, 8 | BRF_GRA },	      // 23
	{ "gv20.r10",		0x10000, 0x8dfc7b87, 8 | BRF_GRA },	      // 24
	{ "gv21.s10",		0x10000, 0x06822aac, 8 | BRF_GRA },	      // 25

	{ "gv1.g5",		0x10000, 0x2255f8dd, 15 | BRF_SND },	      // 26 Samples
};

STD_ROM_PICK(gwara)
STD_ROM_FN(gwara)

struct BurnDriver BurnDrvGwara = {
	"gwara", "gwar", NULL, NULL, "1987",
	"Guerrilla War (Version 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, gwaraRomInfo, gwaraRomName, NULL, NULL, GwarInputInfo, GwarDIPInfo,
	GwaraInit, DrvExit, GwarFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// T.N.K III (US)

static struct BurnRomInfo tnk3RomDesc[] = {
	{ "tnk3-p1.bin",	0x04000, 0x0d2a8ca9, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "tnk3-p2.bin",	0x04000, 0x0ae0a483, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "tnk3-p3.bin",	0x04000, 0xd16dd4db, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "tnk3-p4.bin",	0x04000, 0x01b45a90, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "tnk3-p5.bin",	0x04000, 0x60db6667, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "tnk3-p6.bin",	0x04000, 0x4761fde7, 2 | BRF_ESS | BRF_PRG }, //  5

	{ "tnk3-p10.bin",	0x04000, 0x7bf0a517, 3 | BRF_ESS | BRF_PRG }, //  6 Z80 #2 Code
	{ "tnk3-p11.bin",	0x04000, 0x0569ce27, 3 | BRF_ESS | BRF_PRG }, //  7

	{ "7122.2",		0x00400, 0x34c06bc6, 14 | BRF_GRA },	      //  8 Color Data
	{ "7122.1",		0x00400, 0x6d0ac66a, 14 | BRF_GRA },	      //  9
	{ "7122.0",		0x00400, 0x4662b4c8, 14 | BRF_GRA },	      // 10

	{ "tnk3-p14.bin",	0x02000, 0x1fd18c43, 4 | BRF_GRA },	      // 11 Text Characters

	{ "tnk3-p12.bin",	0x04000, 0xff495a16, 6 | BRF_GRA },	      // 12 Background Characters
	{ "tnk3-p13.bin",	0x04000, 0xf8344843, 6 | BRF_GRA },	      // 13

	{ "tnk3-p7.bin",	0x04000, 0x06b92c88, 9 | BRF_GRA },	      // 14 Sprites
	{ "tnk3-p8.bin",	0x04000, 0x63d0e2eb, 9 | BRF_GRA },	      // 15
	{ "tnk3-p9.bin",	0x04000, 0x872e3fac, 9 | BRF_GRA },	      // 16
};

STD_ROM_PICK(tnk3)
STD_ROM_FN(tnk3)

struct BurnDriver BurnDrvTnk3 = {
	"tnk3", NULL, NULL, NULL, "1985",
	"T.N.K III (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, tnk3RomInfo, tnk3RomName, NULL, NULL, Tnk3InputInfo, Tnk3DIPInfo,
	Tnk3Init, DrvExit, Tnk3Frame, Tnk3Draw, DrvScan, &DrvRecalc, 0x400,
	224, 288, 3, 4
};


// T.A.N.K (Japan)

static struct BurnRomInfo tnk3jRomDesc[] = {
	{ "p1.4e",			0x04000, 0x03aca147, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "tnk3-p2.bin",	0x04000, 0x0ae0a483, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "tnk3-p3.bin",	0x04000, 0xd16dd4db, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "tnk3-p4.bin",	0x04000, 0x01b45a90, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "tnk3-p5.bin",	0x04000, 0x60db6667, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "tnk3-p6.bin",	0x04000, 0x4761fde7, 2 | BRF_ESS | BRF_PRG }, //  5

	{ "tnk3-p10.bin",	0x04000, 0x7bf0a517, 3 | BRF_ESS | BRF_PRG }, //  6 Z80 #2 Code
	{ "tnk3-p11.bin",	0x04000, 0x0569ce27, 3 | BRF_ESS | BRF_PRG }, //  7

	{ "7122.2",		0x00400, 0x34c06bc6, 14 | BRF_GRA },	      //  8 Color Data
	{ "7122.1",		0x00400, 0x6d0ac66a, 14 | BRF_GRA },	      //  9
	{ "7122.0",		0x00400, 0x4662b4c8, 14 | BRF_GRA },	      // 10

	{ "p14.1e",		0x02000, 0x6bd575ca, 4 | BRF_GRA },	      // 11 Text Characters

	{ "tnk3-p12.bin",	0x04000, 0xff495a16, 6 | BRF_GRA },	      // 12 Background Characters
	{ "tnk3-p13.bin",	0x04000, 0xf8344843, 6 | BRF_GRA },	      // 13

	{ "tnk3-p7.bin",	0x04000, 0x06b92c88, 9 | BRF_GRA },	      // 14 Sprites
	{ "tnk3-p8.bin",	0x04000, 0x63d0e2eb, 9 | BRF_GRA },	      // 15
	{ "tnk3-p9.bin",	0x04000, 0x872e3fac, 9 | BRF_GRA },	      // 16
};

STD_ROM_PICK(tnk3j)
STD_ROM_FN(tnk3j)

struct BurnDriver BurnDrvTnk3j = {
	"tnk3j", "tnk3", NULL, NULL, "1985",
	"T.A.N.K (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, tnk3jRomInfo, tnk3jRomName, NULL, NULL, Tnk3InputInfo, Tnk3DIPInfo,
	Tnk3Init, DrvExit, Tnk3Frame, Tnk3Draw, DrvScan, &DrvRecalc, 0x400,
	224, 288, 3, 4
};


// Athena

static struct BurnRomInfo athenaRomDesc[] = {
	{ "up02_p4.rom",	0x04000, 0x900a113c, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "up02_m4.rom",	0x08000, 0x61c69474, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "up02_p8.rom",	0x04000, 0xdf50af7e, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "up02_m8.rom",	0x08000, 0xf3c933df, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "up02_g6.rom",	0x04000, 0x42dbe029, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "up02_k6.rom",	0x08000, 0x596f1c8a, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "up02_c2.rom",	0x00400, 0x294279ae, 14 | BRF_GRA },	      //  6 Color Data
	{ "up02_b1.rom",	0x00400, 0xd25c9099, 14 | BRF_GRA },	      //  7
	{ "up02_c1.rom",	0x00400, 0xa4a4e7dc, 14 | BRF_GRA },	      //  8

	{ "up01_d2.rom",	0x04000, 0x18b4bcca, 4 | BRF_GRA },	      //  9 Text Characters

	{ "up01_b2.rom",	0x08000, 0xf269c0eb, 6 | BRF_GRA },	      // 10 Background Characters

	{ "up01_p2.rom",	0x08000, 0xc63a871f, 9 | BRF_GRA },	      // 11 Sprites
	{ "up01_s2.rom",	0x08000, 0x760568d8, 9 | BRF_GRA },	      // 12
	{ "up01_t2.rom",	0x08000, 0x57b35c73, 9 | BRF_GRA },	      // 13
};

STD_ROM_PICK(athena)
STD_ROM_FN(athena)

struct BurnDriver BurnDrvAthena = {
	"athena", NULL, NULL, NULL, "1986",
	"Athena\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_PLATFORM, 0,
	NULL, athenaRomInfo, athenaRomName, NULL, NULL, AthenaInputInfo, AthenaDIPInfo,
	AthenaInit, DrvExit, AthenaFrame, Tnk3Draw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// ASO - Armored Scrum Object

static struct BurnRomInfo asoRomDesc[] = {
	{ "aso_p1.d8",		0x04000, 0x84981f3c, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "aso_p2.d7",		0x04000, 0xcfe912a6, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "aso_p3.d5",		0x04000, 0x39a666d2, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "aso_p4.d3",		0x04000, 0xa4122355, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "p5.d2",		0x04000, 0x9879e506, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "p6.d1",		0x04000, 0xc0bfdf1f, 2 | BRF_ESS | BRF_PRG }, //  5

	{ "p7.f4",		0x04000, 0xdbc19736, 3 | BRF_ESS | BRF_PRG }, //  6 Z80 #2 Code
	{ "p8.f3",		0x04000, 0x537726a9, 3 | BRF_ESS | BRF_PRG }, //  7
	{ "p9.f2",		0x04000, 0xaef5a4f4, 3 | BRF_ESS | BRF_PRG }, //  8

	{ "aso_p14.h1",		0x02000, 0x8baa2253, 4 | BRF_GRA },	      //  9 Text Characters

	{ "p10.h14",		0x08000, 0x00dff996, 6 | BRF_GRA },	      // 10 Background Characters

	{ "p11.h11",		0x08000, 0x7feac86c, 9 | BRF_GRA },	      // 11 Sprites
	{ "p12.h9",		0x08000, 0x6895990b, 9 | BRF_GRA },	      // 12
	{ "p13.h8",		0x08000, 0x87a81ce1, 9 | BRF_GRA },	      // 13

	{ "mb7122h.f12",	0x00400, 0x5b0a0059, 14 | BRF_GRA },	      // 14 Color Data
	{ "mb7122h.f13",	0x00400, 0x37e28dd8, 14 | BRF_GRA },	      // 15
	{ "mb7122h.f14",	0x00400, 0xc3fd1dd3, 14 | BRF_GRA },	      // 16

	{ "pal16l8a-1.bin",	0x00104, 0x4e3f9e0d, 0 | BRF_OPT },	      // 17 PLDs
	{ "pal16l8a-2.bin",	0x00104, 0x2a681f9e, 0 | BRF_OPT },	      // 18
	{ "pal16r6a.bin",	0x00104, 0x59c03681, 0 | BRF_OPT },	      // 19
};

STD_ROM_PICK(aso)
STD_ROM_FN(aso)

struct BurnDriver BurnDrvAso = {
	"aso", NULL, NULL, NULL, "1985",
	"ASO - Armored Scrum Object\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, asoRomInfo, asoRomName, NULL, NULL, AsoInputInfo, AsoDIPInfo,
	AsoInit, DrvExit, Tnk3Frame, AsoDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Alpha Mission

static struct BurnRomInfo alphamisRomDesc[] = {
	{ "p1.rom",		0x04000, 0x69af874b, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "p2.rom",		0x04000, 0x7707bfe3, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "p3.rom",		0x04000, 0xb970d642, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "p4.rom",		0x04000, 0x91a89d3c, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "p5.d2",		0x04000, 0x9879e506, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "p6.d1",		0x04000, 0xc0bfdf1f, 2 | BRF_ESS | BRF_PRG }, //  5

	{ "p7.f4",		0x04000, 0xdbc19736, 3 | BRF_ESS | BRF_PRG }, //  6 Z80 #2 Code
	{ "p8.f3",		0x04000, 0x537726a9, 3 | BRF_ESS | BRF_PRG }, //  7
	{ "p9.f2",		0x04000, 0xaef5a4f4, 3 | BRF_ESS | BRF_PRG }, //  8

	{ "p14.rom",		0x02000, 0xacbe29b2, 4 | BRF_GRA },	      //  9 Text Characters

	{ "p10.h14",		0x08000, 0x00dff996, 6 | BRF_GRA },	      // 10 Background Characters

	{ "p11.h11",		0x08000, 0x7feac86c, 9 | BRF_GRA },	      // 11 Sprites
	{ "p12.h9",		0x08000, 0x6895990b, 9 | BRF_GRA },	      // 12
	{ "p13.h8",		0x08000, 0x87a81ce1, 9 | BRF_GRA },	      // 13

	{ "mb7122h.f12",	0x00400, 0x5b0a0059, 14 | BRF_GRA },	      // 14 Color Data
	{ "mb7122h.f13",	0x00400, 0x37e28dd8, 14 | BRF_GRA },	      // 15
	{ "mb7122h.f14",	0x00400, 0xc3fd1dd3, 14 | BRF_GRA },	      // 16

	{ "pal16l8a-1.bin",	0x00104, 0x4e3f9e0d, 0 | BRF_OPT },	      // 17 PLDs
	{ "pal16l8a-2.bin",	0x00104, 0x2a681f9e, 0 | BRF_OPT },	      // 18
	{ "pal16r6a.bin",	0x00104, 0x59c03681, 0 | BRF_OPT },	      // 19
};

STD_ROM_PICK(alphamis)
STD_ROM_FN(alphamis)

struct BurnDriver BurnDrvAlphamis = {
	"alphamis", "aso", NULL, NULL, "1985",
	"Alpha Mission\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, alphamisRomInfo, alphamisRomName, NULL, NULL, AlphamisInputInfo, AlphamisDIPInfo,
	AlphamisInit, DrvExit, Tnk3Frame, AsoDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Arian Mission

static struct BurnRomInfo arianRomDesc[] = {
	{ "p1.d8",		0x04000, 0x0ca89307, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "p2.d7",		0x04000, 0x724518c3, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "p3.d6",		0x04000, 0x4d8db650, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "p4.d3",		0x04000, 0x47baf1db, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "p5.d2",		0x04000, 0x9879e506, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "p6.d1",		0x04000, 0xc0bfdf1f, 2 | BRF_ESS | BRF_PRG }, //  5

	{ "p7.f4",		0x04000, 0xdbc19736, 3 | BRF_ESS | BRF_PRG }, //  6 Z80 #2 Code
	{ "p8.f3",		0x04000, 0x537726a9, 3 | BRF_ESS | BRF_PRG }, //  7
	{ "p9.f2",		0x04000, 0xaef5a4f4, 3 | BRF_ESS | BRF_PRG }, //  8

	{ "p14.h1",		0x02000, 0xe599bd30, 4 | BRF_GRA },	      //  9 Text Characters

	{ "p10.h14",		0x08000, 0x00dff996, 6 | BRF_GRA },	      // 10 Background Characters

	{ "p11.h11",		0x08000, 0x7feac86c, 9 | BRF_GRA },	      // 11 Sprites
	{ "p12.h9",		0x08000, 0x6895990b, 9 | BRF_GRA },	      // 12
	{ "p13.h8",		0x08000, 0x87a81ce1, 9 | BRF_GRA },	      // 13

	{ "mb7122h.f12",	0x00400, 0x5b0a0059, 14 | BRF_GRA },	      // 14 Color Data
	{ "mb7122h.f13",	0x00400, 0x37e28dd8, 14 | BRF_GRA },	      // 15
	{ "mb7122h.f14",	0x00400, 0xc3fd1dd3, 14 | BRF_GRA },	      // 16

	{ "pal16l8a-1.bin",	0x00104, 0x4e3f9e0d, 0 | BRF_OPT },	      // 17 PLDs
	{ "pal16l8a-2.bin",	0x00104, 0x2a681f9e, 0 | BRF_OPT },	      // 18
	{ "pal16r6a.bin",	0x00104, 0x59c03681, 0 | BRF_OPT },	      // 19
};

STD_ROM_PICK(arian)
STD_ROM_FN(arian)

struct BurnDriver BurnDrvArian = {
	"arian", "aso", NULL, NULL, "1985",
	"Arian Mission\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, arianRomInfo, arianRomName, NULL, NULL, AlphamisInputInfo, AlphamisDIPInfo,
	AlphamisInit, DrvExit, Tnk3Frame, AsoDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Fighting Golf (World?)

static struct BurnRomInfo fitegolfRomDesc[] = {
	{ "gu2",		0x04000, 0x19be7ad6, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "gu1",		0x08000, 0xbc32568f, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "gu6",		0x04000, 0x2b9978c5, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "gu5",		0x08000, 0xea3d138c, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "gu3",		0x04000, 0x811b87d7, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "gu4",		0x08000, 0x2d998e2b, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "82s137.2c",		0x00400, 0x6e4c7836, 14 | BRF_GRA },	      //  6 Color Data
	{ "82s137.1b",		0x00400, 0x29e7986f, 14 | BRF_GRA },	      //  7
	{ "82s137.1c",		0x00400, 0x27ba9ff9, 14 | BRF_GRA },	      //  8

	{ "gu8",		0x04000, 0xf1628dcf, 4 | BRF_GRA },	      //  9 Text Characters

	{ "gu7",		0x08000, 0x4655f94e, 6 | BRF_GRA },	      // 10 Background Characters

	{ "gu9",		0x08000, 0xd4957ec5, 9 | BRF_GRA },	      // 11 Sprites
	{ "gu10",		0x08000, 0xb3acdac2, 9 | BRF_GRA },	      // 12
	{ "gu11",		0x08000, 0xb99cf73b, 9 | BRF_GRA },	      // 13

	{ "pal16r6a.6c",	0x00104, 0xde291f4e, 0 | BRF_OPT },	      // 14 PLDs
	{ "pal16l8a.3f",	0x00104, 0xc5f1c1da, 0 | BRF_OPT },	      // 15
	{ "pal20l8a.6r",	0x00144, 0x0f011673, 0 | BRF_OPT },	      // 16
};

STD_ROM_PICK(fitegolf)
STD_ROM_FN(fitegolf)

struct BurnDriver BurnDrvFitegolf = {
	"fitegolf", NULL, NULL, NULL, "1988",
	"Fighting Golf (World?)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, fitegolfRomInfo, fitegolfRomName, NULL, NULL, FitegolfInputInfo, FitegolfDIPInfo,
	FitegolfInit, DrvExit, FitegolfFrame, Tnk3Draw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// Fighting Golf (US)

static struct BurnRomInfo fitegolfuRomDesc[] = {
	{ "np45.128",		0x04000, 0x16e8e763, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "mn45.256",		0x08000, 0xa4fa09d5, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "gu6",		0x04000, 0x2b9978c5, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "gu5",		0x08000, 0xea3d138c, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "gu3",		0x04000, 0x811b87d7, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "gu4",		0x08000, 0x2d998e2b, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "82s137.2c",		0x00400, 0x6e4c7836, 14 | BRF_GRA },	      //  6 Color Data
	{ "82s137.1b",		0x00400, 0x29e7986f, 14 | BRF_GRA },	      //  7
	{ "82s137.1c",		0x00400, 0x27ba9ff9, 14 | BRF_GRA },	      //  8

	{ "gu8",		0x04000, 0xf1628dcf, 4 | BRF_GRA },	      //  9 Text Characters

	{ "gu7",		0x08000, 0x4655f94e, 6 | BRF_GRA },	      // 10 Background Characters

	{ "gu9",		0x08000, 0xd4957ec5, 9 | BRF_GRA },	      // 11 Sprites
	{ "gu10",		0x08000, 0xb3acdac2, 9 | BRF_GRA },	      // 12
	{ "gu11",		0x08000, 0xb99cf73b, 9 | BRF_GRA },	      // 13

	{ "pal16r6a.6c",	0x00104, 0xde291f4e, 0 | BRF_OPT },	      // 14 PLDs
	{ "pal16l8a.3f",	0x00104, 0xc5f1c1da, 0 | BRF_OPT },	      // 15
	{ "pal20l8a.6r",	0x00144, 0x0f011673, 0 | BRF_OPT },	      // 16
};

STD_ROM_PICK(fitegolfu)
STD_ROM_FN(fitegolfu)

struct BurnDriver BurnDrvFitegolfu = {
	"fitegolfu", "fitegolf", NULL, NULL, "1988",
	"Fighting Golf (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, fitegolfuRomInfo, fitegolfuRomName, NULL, NULL, FitegolfInputInfo, FitegolfDIPInfo,
	FitegolfInit, DrvExit, FitegolfFrame, Tnk3Draw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// Fighting Golf (US, Ver 2)

static struct BurnRomInfo fitegolf2RomDesc[] = {
	{ "fg_ver2_6.4e",	0x04000, 0x4cc9ef0c, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "fg_ver2_7.g4",	0x04000, 0x144b0beb, 1 | BRF_ESS | BRF_PRG }, //  1
	{ "fg_ver2_8.4h",	0x04000, 0x057888c9, 1 | BRF_ESS | BRF_PRG }, //  2

	{ "fg_ver2_3.2e",	0x04000, 0xcf8c29d7, 2 | BRF_ESS | BRF_PRG }, //  3 Z80 #1 Code
	{ "fg_ver2_4.2g",	0x04000, 0x90c1fb09, 2 | BRF_ESS | BRF_PRG }, //  4
	{ "fg_ver2_5.2h",	0x04000, 0x0ffbdbb8, 2 | BRF_ESS | BRF_PRG }, //  5

	{ "fg_2.3e",		0x04000, 0x811b87d7, 3 | BRF_ESS | BRF_PRG }, //  6 Z80 #2 Code
	{ "fg_1.2e",		0x08000, 0x2d998e2b, 3 | BRF_ESS | BRF_PRG }, //  7

	{ "gl1.5f",			0x00400, 0x6e4c7836, 14 | BRF_GRA },	      //  8 Color Data
	{ "gl2.5g",			0x00400, 0x29e7986f, 14 | BRF_GRA },	      //  9
	{ "gl3.5h",			0x00400, 0x27ba9ff9, 14 | BRF_GRA },	      // 10

	{ "fg_12.1e",		0x04000, 0xf1628dcf, 4 | BRF_GRA },	      // 11 Text Characters

	{ "fg_14.3d",		0x04000, 0x29393a19, 6 | BRF_GRA },	      // 12 Background Characters
	{ "fg_ver2_13.3c",	0x04000, 0x5cd57c93, 6 | BRF_GRA },	      // 13

	{ "fg_ver2_11.7h",	0x08000, 0xd4957ec5, 9 | BRF_GRA },	      // 14 Sprites
	{ "fg_ver2_10.7g",	0x08000, 0xb3acdac2, 9 | BRF_GRA },	      // 15
	{ "fg_ver2_9.7e",	0x08000, 0xb99cf73b, 9 | BRF_GRA },	      // 16

	{ "pal16r6a.6c",	0x00104, 0xde291f4e, 0 | BRF_OPT },	      // 17 PLDs
	{ "pal16l8a.3f",	0x00104, 0xc5f1c1da, 0 | BRF_OPT },	      // 18
	{ "pal20l8a.6r",	0x00144, 0x0f011673, 0 | BRF_OPT },	      // 19
};

STD_ROM_PICK(fitegolf2)
STD_ROM_FN(fitegolf2)

struct BurnDriver BurnDrvFitegolf2 = {
	"fitegolf2", "fitegolf", NULL, NULL, "1988",
	"Fighting Golf (US, Ver 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, fitegolf2RomInfo, fitegolf2RomName, NULL, NULL, FitegolfInputInfo, FitegolfDIPInfo,
	FitegolfInit, DrvExit, FitegolfFrame, Tnk3Draw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// Country Club

static struct BurnRomInfo countrycRomDesc[] = {
	{ "cc5.4e",		0x04000, 0x07666af8, 1 | BRF_ESS | BRF_PRG }, //  3 Z80 #0 Code
	{ "cc6.4g",		0x04000, 0xab18fd9f, 1 | BRF_ESS | BRF_PRG }, //  4
	{ "cc7.4h",		0x04000, 0x58a1ec0c, 1 | BRF_ESS | BRF_PRG }, //  5

	{ "cc2.2e",		0x04000, 0x43d205e0, 2 | BRF_ESS | BRF_PRG }, //  0 Z80 #1 Code
	{ "cc3.2g",		0x04000, 0x7290770f, 2 | BRF_ESS | BRF_PRG }, //  1
	{ "cc4.2h",		0x04000, 0x61990582, 2 | BRF_ESS | BRF_PRG }, //  2

	{ "cc1.1f",		0x10000, 0x863f1624, 3 | BRF_ESS | BRF_PRG }, //  6 Z80 #2 Code

	{ "cc1pr.5f",		0x00400, 0x7da9ce33, 14 | BRF_GRA },	      //  7 Color Data
	{ "cc2pr.5g",		0x00400, 0x982e4f46, 14 | BRF_GRA },	      //  8
	{ "cc3pr.5h",		0x00400, 0x47f2b83d, 14 | BRF_GRA },	      //  9

	{ "cc11.1e",		0x04000, 0xce927ac7, 4 | BRF_GRA },	      // 10 Text Characters

	{ "cc13.2d",		0x04000, 0xef86c388, 6 | BRF_GRA },	      // 11 Background Characters
	{ "cc12.2c",		0x04000, 0xd7d55a36, 6 | BRF_GRA },	      // 12

	{ "cc10.7h",		0x08000, 0x90091667, 9 | BRF_GRA },	      // 13 Sprites
	{ "cc9.7g",		0x08000, 0x56249142, 9 | BRF_GRA },	      // 14
	{ "cc8.7e",		0x08000, 0x55943065, 9 | BRF_GRA },	      // 15
};

STD_ROM_PICK(countryc)
STD_ROM_FN(countryc)

struct BurnDriver BurnDrvCountryc = {
	"countryc", NULL, NULL, NULL, "1988",
	"Country Club\0", "bad inputs, use fitegolf instead!", "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/*BDF_GAME_WORKING |*/0, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, countrycRomInfo, countrycRomName, NULL, NULL, FitegolfInputInfo, FitegolfDIPInfo, // wrong
	FitegolfInit, DrvExit, FitegolfFrame, Tnk3Draw, DrvScan, &DrvRecalc, 0x400,
	288, 216, 4, 3
};


// Ikari Warriors (US JAMMA)

static struct BurnRomInfo ikariRomDesc[] = {
	{ "1.rom",		0x10000, 0x52a8b2dd, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "2.rom",		0x10000, 0x45364d55, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "3.rom",		0x10000, 0x56a26699, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "7122er.prm",		0x00400, 0xb9bf2c2c, 14 | BRF_GRA },	      //  3 Color Data
	{ "7122eg.prm",		0x00400, 0x0703a770, 14 | BRF_GRA },	      //  4
	{ "7122eb.prm",		0x00400, 0x0a11cdde, 14 | BRF_GRA },	      //  5

	{ "7.rom",		0x04000, 0xa7eb4917, 4 | BRF_GRA },	      //  6 Text Characters

	{ "17.rom",		0x08000, 0xe0dba976, 6 | BRF_GRA },           //  7 Background Characters
	{ "18.rom",		0x08000, 0x24947d5f, 6 | BRF_GRA },           //  8
	{ "19.rom",		0x08000, 0x9ee59e91, 6 | BRF_GRA },           //  9
	{ "20.rom",		0x08000, 0x5da7ec1a, 6 | BRF_GRA },           // 10

	{ "8.rom",		0x08000, 0x9827c14a, 9 | BRF_GRA },           // 11 16x16 Sprites
	{ "9.rom",		0x08000, 0x545c790c, 9 | BRF_GRA },           // 12
	{ "10.rom",		0x08000, 0xec9ba07e, 9 | BRF_GRA },           // 13

	{ "11.rom",		0x08000, 0x5c75ea8f, 10 | BRF_GRA },          // 14 32x32 Sprites
	{ "14.rom",		0x08000, 0x3293fde4, 10 | BRF_GRA },          // 15
	{ "12.rom",		0x08000, 0x95138498, 10 | BRF_GRA },          // 16
	{ "15.rom",		0x08000, 0x65a61c99, 10 | BRF_GRA },          // 17
	{ "13.rom",		0x08000, 0x315383d7, 10 | BRF_GRA },          // 18
	{ "16.rom",		0x08000, 0xe9b03e07, 10 | BRF_GRA },          // 19
};

STD_ROM_PICK(ikari)
STD_ROM_FN(ikari)

struct BurnDriver BurnDrvIkari = {
	"ikari", NULL, NULL, NULL, "1986",
	"Ikari Warriors (US JAMMA)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ikariRomInfo, ikariRomName, NULL, NULL, IkariInputInfo, IkariDIPInfo,
	IkariInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Ikari Warriors (US)

static struct BurnRomInfo ikariaRomDesc[] = {
	{ "p1.bin",		0x04000, 0xad0e440e, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "p2.bin",		0x08000, 0xb585e931, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "p3",			0x04000, 0x8a9bd1f0, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "p4",			0x08000, 0xf4101cb4, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "ik5",		0x04000, 0x863448fa, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "ik6",		0x08000, 0x9b16aa57, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "7122er.prm",		0x00400, 0xb9bf2c2c, 14 | BRF_GRA },	      //  6 Color Data
	{ "7122eg.prm",		0x00400, 0x0703a770, 14 | BRF_GRA },	      //  7
	{ "7122eb.prm",		0x00400, 0x0a11cdde, 14 | BRF_GRA },	      //  8

	{ "7.rom",		0x04000, 0xa7eb4917, 4 | BRF_GRA },	      //  9 Text Characters

	{ "17.rom",		0x08000, 0xe0dba976, 6 | BRF_GRA },	      // 10 Background Tiles
	{ "18.rom",		0x08000, 0x24947d5f, 6 | BRF_GRA },	      // 11
	{ "19.rom",		0x08000, 0x9ee59e91, 6 | BRF_GRA },	      // 12
	{ "20.rom",		0x08000, 0x5da7ec1a, 6 | BRF_GRA },	      // 13

	{ "8.rom",		0x08000, 0x9827c14a, 9 | BRF_GRA },	      // 14 16x16 Sprite Tiles
	{ "9.rom",		0x08000, 0x545c790c, 9 | BRF_GRA },	      // 15
	{ "10.rom",		0x08000, 0xec9ba07e, 9 | BRF_GRA },	      // 16

	{ "11.rom",		0x08000, 0x5c75ea8f, 10 | BRF_GRA },	      // 17 32x32 Sprite Tiles
	{ "14.rom",		0x08000, 0x3293fde4, 10 | BRF_GRA },	      // 18
	{ "12.rom",		0x08000, 0x95138498, 10 | BRF_GRA },	      // 19
	{ "15.rom",		0x08000, 0x65a61c99, 10 | BRF_GRA },	      // 20
	{ "13.rom",		0x08000, 0x315383d7, 10 | BRF_GRA },	      // 21
	{ "16.rom",		0x08000, 0xe9b03e07, 10 | BRF_GRA },	      // 22
};

STD_ROM_PICK(ikaria)
STD_ROM_FN(ikaria)

struct BurnDriver BurnDrvIkaria = {
	"ikaria", "ikari", NULL, NULL, "1986",
	"Ikari Warriors (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ikariaRomInfo, ikariaRomName, NULL, NULL, IkariaInputInfo, IkariDIPInfo,
	IkariaInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Ikari Warriors (US No Continues)

static struct BurnRomInfo ikarincRomDesc[] = {
	{ "p1",			0x04000, 0x738fcec4, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "p2",			0x08000, 0x89f7945a, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "p3",			0x04000, 0x8a9bd1f0, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "p4",			0x08000, 0xf4101cb4, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "ik5",		0x04000, 0x863448fa, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "ik6",		0x08000, 0x9b16aa57, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "7122er.prm",		0x00400, 0xb9bf2c2c, 14 | BRF_GRA },	      //  6 Color Data
	{ "7122eg.prm",		0x00400, 0x0703a770, 14 | BRF_GRA },	      //  7
	{ "7122eb.prm",		0x00400, 0x0a11cdde, 14 | BRF_GRA },	      //  8

	{ "7.rom",		0x04000, 0xa7eb4917, 4 | BRF_GRA },	      //  9 Text Characters

	{ "17.rom",		0x08000, 0xe0dba976, 6 | BRF_GRA },	      // 10 Background Tiles
	{ "18.rom",		0x08000, 0x24947d5f, 6 | BRF_GRA },	      // 11
	{ "19.rom",		0x08000, 0x9ee59e91, 6 | BRF_GRA },	      // 12
	{ "20.rom",		0x08000, 0x5da7ec1a, 6 | BRF_GRA },	      // 13

	{ "8.rom",		0x08000, 0x9827c14a, 9 | BRF_GRA },	      // 14 16x16 Sprite Tiles
	{ "9.rom",		0x08000, 0x545c790c, 9 | BRF_GRA },	      // 15
	{ "10.rom",		0x08000, 0xec9ba07e, 9 | BRF_GRA },	      // 16

	{ "11.rom",		0x08000, 0x5c75ea8f, 10 | BRF_GRA },	      // 17 32x32 Sprite Tiles
	{ "14.rom",		0x08000, 0x3293fde4, 10 | BRF_GRA },	      // 18
	{ "12.rom",		0x08000, 0x95138498, 10 | BRF_GRA },	      // 19
	{ "15.rom",		0x08000, 0x65a61c99, 10 | BRF_GRA },	      // 20
	{ "13.rom",		0x08000, 0x315383d7, 10 | BRF_GRA },	      // 21
	{ "16.rom",		0x08000, 0xe9b03e07, 10 | BRF_GRA },	      // 22

	{ "ampal16r6a-a5004.1",	0x0104, 0xa2e9a162, 0 | BRF_OPT },            // 23 plds
	{ "pal20l8a-a5004.2",	0x0144, 0x28f2c404, 0 | BRF_OPT },            // 24
	{ "ampal16l8a-a5004.3",	0x0104, 0x540351f2, 0 | BRF_OPT },            // 25
	{ "ampal16l8a-a5004.4",	0x0104, 0x540351f2, 0 | BRF_OPT },            // 26
};

STD_ROM_PICK(ikarinc)
STD_ROM_FN(ikarinc)

struct BurnDriver BurnDrvIkarinc = {
	"ikarinc", "ikari", NULL, NULL, "1986",
	"Ikari Warriors (US No Continues)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ikarincRomInfo, ikarincRomName, NULL, NULL, IkariaInputInfo, IkariDIPInfo,
	IkariInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Ikari (Japan No Continues)

static struct BurnRomInfo ikarijpRomDesc[] = {
	{ "up03_l4.rom",	0x04000, 0xcde006be, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "up03_k4.rom",	0x08000, 0x26948850, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "ik3",		0x04000, 0x9bb385f8, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "ik4",		0x08000, 0x3a144bca, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "ik5",		0x04000, 0x863448fa, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "ik6",		0x08000, 0x9b16aa57, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "7122er.prm",		0x00400, 0xb9bf2c2c, 14 | BRF_GRA },	      //  6 Color Data
	{ "7122eg.prm",		0x00400, 0x0703a770, 14 | BRF_GRA },	      //  7
	{ "7122eb.prm",		0x00400, 0x0a11cdde, 14 | BRF_GRA },	      //  8

	{ "ik7",		0x04000, 0x9e88f536, 4 | BRF_GRA },	      //  9 Text Characters

	{ "17.rom",		0x08000, 0xe0dba976, 6 | BRF_GRA },	      // 10 Background Tiles
	{ "18.rom",		0x08000, 0x24947d5f, 6 | BRF_GRA },	      // 11
	{ "ik19",		0x08000, 0x566242ec, 6 | BRF_GRA },	      // 12
	{ "20.rom",		0x08000, 0x5da7ec1a, 6 | BRF_GRA },	      // 13

	{ "ik8",		0x08000, 0x75d796d0, 9 | BRF_GRA },	      // 14 16x16 Sprite Tiles
	{ "ik9",		0x08000, 0x2c34903b, 9 | BRF_GRA },	      // 15
	{ "ik10",		0x08000, 0xda9ccc94, 9 | BRF_GRA },	      // 16

	{ "11.rom",		0x08000, 0x5c75ea8f, 10 | BRF_GRA },	      // 17 32x32 Sprite Tiles
	{ "14.rom",		0x08000, 0x3293fde4, 10 | BRF_GRA },	      // 18
	{ "12.rom",		0x08000, 0x95138498, 10 | BRF_GRA },	      // 19
	{ "15.rom",		0x08000, 0x65a61c99, 10 | BRF_GRA },	      // 20
	{ "13.rom",		0x08000, 0x315383d7, 10 | BRF_GRA },	      // 21
	{ "16.rom",		0x08000, 0xe9b03e07, 10 | BRF_GRA },	      // 22

	{ "ampal16r6a-a5004.1",	0x00104, 0xa2e9a162, 0 | BRF_OPT },           // 23 plds
	{ "pal20l8a-a5004.2",	0x00144, 0x28f2c404, 0 | BRF_OPT },           // 24
	{ "ampal16l8a-a5004.3",	0x00104, 0x540351f2, 0 | BRF_OPT },           // 25
	{ "ampal16l8a-a5004.4",	0x00104, 0x540351f2, 0 | BRF_OPT },           // 26
};

STD_ROM_PICK(ikarijp)
STD_ROM_FN(ikarijp)

struct BurnDriver BurnDrvIkarijp = {
	"ikarijp", "ikari", NULL, NULL, "1986",
	"Ikari (Japan No Continues)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ikarijpRomInfo, ikarijpRomName, NULL, NULL, IkariaInputInfo, IkariDIPInfo,
	IkariaInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Ikari (Joystick hack bootleg)

static struct BurnRomInfo ikarijpbRomDesc[] = {
	{ "ik1",		0x04000, 0x2ef87dce, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "up03_k4.rom",	0x08000, 0x26948850, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "ik3",		0x04000, 0x9bb385f8, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "ik4",		0x08000, 0x3a144bca, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "ik5",		0x04000, 0x863448fa, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "ik6",		0x08000, 0x9b16aa57, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "7122er.prm",		0x00400, 0xb9bf2c2c, 14 | BRF_GRA },	      //  6 Color Data
	{ "7122eg.prm",		0x00400, 0x0703a770, 14 | BRF_GRA },	      //  7
	{ "7122eb.prm",		0x00400, 0x0a11cdde, 14 | BRF_GRA },	      //  8

	{ "ik7",		0x04000, 0x9e88f536, 4 | BRF_GRA },	      //  9 Text Characters

	{ "17.rom",		0x08000, 0xe0dba976, 6 | BRF_GRA },	      // 10 Background Tiles
	{ "18.rom",		0x08000, 0x24947d5f, 6 | BRF_GRA },	      // 11
	{ "ik19",		0x08000, 0x566242ec, 6 | BRF_GRA },	      // 12
	{ "20.rom",		0x08000, 0x5da7ec1a, 6 | BRF_GRA },	      // 13

	{ "ik8",		0x08000, 0x75d796d0, 9 | BRF_GRA },	      // 14 16x16 Sprite Tiles
	{ "ik9",		0x08000, 0x2c34903b, 9 | BRF_GRA },	      // 15
	{ "ik10",		0x08000, 0xda9ccc94, 9 | BRF_GRA },	      // 16

	{ "11.rom",		0x08000, 0x5c75ea8f, 10 | BRF_GRA },	      // 17 32x32 Sprite Tiles
	{ "14.rom",		0x08000, 0x3293fde4, 10 | BRF_GRA },	      // 18
	{ "12.rom",		0x08000, 0x95138498, 10 | BRF_GRA },	      // 19
	{ "15.rom",		0x08000, 0x65a61c99, 10 | BRF_GRA },	      // 20
	{ "13.rom",		0x08000, 0x315383d7, 10 | BRF_GRA },	      // 21
	{ "16.rom",		0x08000, 0xe9b03e07, 10 | BRF_GRA },	      // 22
};

STD_ROM_PICK(ikarijpb)
STD_ROM_FN(ikarijpb)

struct BurnDriver BurnDrvIkarijpb = {
	"ikarijpb", "ikari", NULL, NULL, "1986",
	"Ikari (Joystick hack bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ikarijpbRomInfo, ikarijpbRomName, NULL, NULL, IkariaInputInfo, IkariDIPInfo,
	IkarijoyInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Rambo 3 (bootleg of Ikari, Joystick hack)

static struct BurnRomInfo ikariramRomDesc[] = {
	{ "1.bin",		0x04000, 0xce97e30f, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code
	{ "up03_k4.rom",	0x08000, 0x26948850, 1 | BRF_ESS | BRF_PRG }, //  1

	{ "ik3",		0x04000, 0x9bb385f8, 2 | BRF_ESS | BRF_PRG }, //  2 Z80 #1 Code
	{ "ik4",		0x08000, 0x3a144bca, 2 | BRF_ESS | BRF_PRG }, //  3

	{ "ik5",		0x04000, 0x863448fa, 3 | BRF_ESS | BRF_PRG }, //  4 Z80 #2 Code
	{ "ik6",		0x08000, 0x9b16aa57, 3 | BRF_ESS | BRF_PRG }, //  5

	{ "7122er.prm",		0x00400, 0xb9bf2c2c, 14 | BRF_GRA },	      //  6 Color Data
	{ "7122eg.prm",		0x00400, 0x0703a770, 14 | BRF_GRA },	      //  7
	{ "7122eb.prm",		0x00400, 0x0a11cdde, 14 | BRF_GRA },	      //  8

	{ "ik7",		0x04000, 0x9e88f536, 4 | BRF_GRA },	      //  9 Text Characters

	{ "17.rom",		0x08000, 0xe0dba976, 6 | BRF_GRA },	      // 10 Background Tiles
	{ "18.rom",		0x08000, 0x24947d5f, 6 | BRF_GRA },	      // 11
	{ "ik19",		0x08000, 0x566242ec, 6 | BRF_GRA },	      // 12
	{ "20.rom",		0x08000, 0x5da7ec1a, 6 | BRF_GRA },	      // 13

	{ "ik8",		0x08000, 0x75d796d0, 9 | BRF_GRA },	      // 14 16x16 Sprite Tiles
	{ "ik9",		0x08000, 0x2c34903b, 9 | BRF_GRA },	      // 15
	{ "ik10",		0x08000, 0xda9ccc94, 9 | BRF_GRA },	      // 16

	{ "11.rom",		0x08000, 0x5c75ea8f, 10 | BRF_GRA },	      // 17 32x32 Sprite Tiles
	{ "14.rom",		0x08000, 0x3293fde4, 10 | BRF_GRA },	      // 18
	{ "12.rom",		0x08000, 0x95138498, 10 | BRF_GRA },	      // 19
	{ "15.rom",		0x08000, 0x65a61c99, 10 | BRF_GRA },	      // 20
	{ "13.rom",		0x08000, 0x315383d7, 10 | BRF_GRA },	      // 21
	{ "16.rom",		0x08000, 0xe9b03e07, 10 | BRF_GRA },	      // 22

	{ "82s191.bin",		0x00800, 0x072f8622, 0 | BRF_OPT },           // 23 Bootleg Proms
};

STD_ROM_PICK(ikariram)
STD_ROM_FN(ikariram)

struct BurnDriver BurnDrvIkariram = {
	"ikariram", "ikari", NULL, NULL, "1986",
	"Rambo 3 (bootleg of Ikari, Joystick hack)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, ikariramRomInfo, ikariramRomName, NULL, NULL, IkariaInputInfo, IkariDIPInfo,
	IkarijoyInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Victory Road

static struct BurnRomInfo victroadRomDesc[] = {
	{ "p1.p4",		0x10000, 0xe334acef, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "p2.p8",		0x10000, 0x907fac83, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "p3.k7",		0x10000, 0xbac745f6, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "mb7122e.1k",		0x00400, 0x491ab831, 14 | BRF_GRA },	      //  3 Color Data
	{ "mb7122e.2l",		0x00400, 0x8feca424, 14 | BRF_GRA },	      //  4
	{ "mb7122e.1l",		0x00400, 0x220076ca, 14 | BRF_GRA },	      //  5

	{ "p7.b3",		0x04000, 0x2b6ed95b, 4 | BRF_GRA },	      //  6 Text Characters

	{ "p17.cd4",		0x08000, 0x19d4518c, 6 | BRF_GRA },           //  7 Background Characters
	{ "p18.cd2",		0x08000, 0xd818be43, 6 | BRF_GRA },           //  8
	{ "p19.b4",		0x08000, 0xd64e0f89, 6 | BRF_GRA },           //  9
	{ "p20.b2",		0x08000, 0xedba0f31, 6 | BRF_GRA },           // 10

	{ "p8.d3",		0x08000, 0xdf7f252a, 9 | BRF_GRA },           // 11 16x16 Sprites
	{ "p9.ef3",		0x08000, 0x9897bc05, 9 | BRF_GRA },           // 12
	{ "p10.gh3",		0x08000, 0xecd3c0ea, 9 | BRF_GRA },           // 13

	{ "p11.m4",		0x08000, 0x668b25a4, 10 | BRF_GRA },          // 14 32x32 Sprites
	{ "p14.m2",		0x08000, 0xa7031d4a, 10 | BRF_GRA },          // 15
	{ "p12.np4",		0x08000, 0xf44e95fa, 10 | BRF_GRA },          // 16
	{ "p15.np2",		0x08000, 0x120d2450, 10 | BRF_GRA },          // 17
	{ "p13.r4",		0x08000, 0x980ca3d8, 10 | BRF_GRA },          // 18
	{ "p16.r2",		0x08000, 0x9f820e8a, 10 | BRF_GRA },          // 19

	{ "p4.ef5",		0x10000, 0xe10fb8cc, 15 | BRF_SND },          // 20 Samples
	{ "p5.g5",		0x10000, 0x93e5f110, 15 | BRF_SND },          // 21
};

STD_ROM_PICK(victroad)
STD_ROM_FN(victroad)

struct BurnDriver BurnDrvVictroad = {
	"victroad", NULL, NULL, NULL, "1986",
	"Victory Road\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, victroadRomInfo, victroadRomName, NULL, NULL, VictroadInputInfo, VictroadDIPInfo,
	VictroadInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Dogou Souken

static struct BurnRomInfo dogosokeRomDesc[] = {
	{ "up03_p4.rom",	0x10000, 0x37867ad2, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "p2",			0x10000, 0x907fac83, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "up03_k7.rom",	0x10000, 0x173fa571, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "up03_k1.rom",	0x00400, 0x10a2ce2b, 14 | BRF_GRA },	      //  3 Color Data
	{ "up03_l2.rom",	0x00400, 0x99dc9792, 14 | BRF_GRA },	      //  4
	{ "up03_l1.rom",	0x00400, 0xe7213160, 14 | BRF_GRA },	      //  5

	{ "up02_b3.rom",	0x04000, 0x51a4ec83, 4 | BRF_GRA },	      //  6 Text Characters

	{ "p17",		0x08000, 0x19d4518c, 6 | BRF_GRA },           //  7 Background Tiles
	{ "p18",		0x08000, 0xd818be43, 6 | BRF_GRA },           //  8
	{ "p19",		0x08000, 0xd64e0f89, 6 | BRF_GRA },           //  9
	{ "p20",		0x08000, 0xedba0f31, 6 | BRF_GRA },           // 10

	{ "up02_d3.rom",	0x08000, 0xd43044f8, 9 | BRF_GRA },           // 11 16x16 Sprites
	{ "up02_e3.rom",	0x08000, 0x365ed2d8, 9 | BRF_GRA },           // 12
	{ "up02_g3.rom",	0x08000, 0x92579bf3, 9 | BRF_GRA },           // 13

	{ "p11",		0x08000, 0x668b25a4, 10 | BRF_GRA },          // 14 32x32 Sprites
	{ "p14",		0x08000, 0xa7031d4a, 10 | BRF_GRA },          // 15
	{ "p12",		0x08000, 0xf44e95fa, 10 | BRF_GRA },          // 16
	{ "p15",		0x08000, 0x120d2450, 10 | BRF_GRA },          // 17
	{ "p13",		0x08000, 0x980ca3d8, 10 | BRF_GRA },          // 18
	{ "p16",		0x08000, 0x9f820e8a, 10 | BRF_GRA },          // 19

	{ "up03_f5.rom",	0x10000, 0x5b43fe9f, 15 | BRF_SND },          // 20 Samples
	{ "up03_g5.rom",	0x10000, 0xaae30cd6, 15 | BRF_SND },          // 21
};

STD_ROM_PICK(dogosoke)
STD_ROM_FN(dogosoke)

struct BurnDriver BurnDrvDogosoke = {
	"dogosoke", "victroad", NULL, NULL, "1986",
	"Dogou Souken\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, dogosokeRomInfo, dogosokeRomName, NULL, NULL, VictroadInputInfo, VictroadDIPInfo,
	VictroadInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Dogou Souken (Joystick hack bootleg)

static struct BurnRomInfo dogosokbRomDesc[] = {
	{ "01",			0x10000, 0x53b0ad90, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 Code

	{ "p2",			0x10000, 0x907fac83, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 Code

	{ "up03_k7.rom",	0x10000, 0x173fa571, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 Code

	{ "up03_k1.rom",	0x00400, 0x10a2ce2b, 14 | BRF_GRA },	      //  3 Color Data
	{ "up03_l2.rom",	0x00400, 0x99dc9792, 14 | BRF_GRA },	      //  4
	{ "up03_l1.rom",	0x00400, 0xe7213160, 14 | BRF_GRA },	      //  5

	{ "up02_b3.rom",	0x04000, 0x51a4ec83, 4 | BRF_GRA },	      //  6 Text Characters

	{ "p17",		0x08000, 0x19d4518c, 6 | BRF_GRA },           //  7 Background Tiles
	{ "p18",		0x08000, 0xd818be43, 6 | BRF_GRA },           //  8
	{ "p19",		0x08000, 0xd64e0f89, 6 | BRF_GRA },           //  9
	{ "p20",		0x08000, 0xedba0f31, 6 | BRF_GRA },           // 10

	{ "up02_d3.rom",	0x08000, 0xd43044f8, 9 | BRF_GRA },           // 11 16x16 Sprites
	{ "up02_e3.rom",	0x08000, 0x365ed2d8, 9 | BRF_GRA },           // 12
	{ "up02_g3.rom",	0x08000, 0x92579bf3, 9 | BRF_GRA },           // 13

	{ "p11",		0x08000, 0x668b25a4, 10 | BRF_GRA },          // 14 32x32 Sprites
	{ "p14",		0x08000, 0xa7031d4a, 10 | BRF_GRA },          // 15
	{ "p12",		0x08000, 0xf44e95fa, 10 | BRF_GRA },          // 16
	{ "p15",		0x08000, 0x120d2450, 10 | BRF_GRA },          // 17
	{ "p13",		0x08000, 0x980ca3d8, 10 | BRF_GRA },          // 18
	{ "p16",		0x08000, 0x9f820e8a, 10 | BRF_GRA },          // 19

	{ "up03_f5.rom",	0x10000, 0x5b43fe9f, 15 | BRF_SND },          // 20 Samples
	{ "up03_g5.rom",	0x10000, 0xaae30cd6, 15 | BRF_SND },          // 21
};

STD_ROM_PICK(dogosokb)
STD_ROM_FN(dogosokb)

struct BurnDriver BurnDrvDogosokb = {
	"dogosokb", "victroad", NULL, NULL, "1986",
	"Dogou Souken (Joystick hack bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_BOOTLEG | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SHOOT, 0,
	NULL, dogosokbRomInfo, dogosokbRomName, NULL, NULL, VictroadInputInfo, VictroadDIPInfo,
	VictroadInit, DrvExit, AthenaFrame, IkariDraw, DrvScan, &DrvRecalc, 0x400,
	216, 288, 3, 4
};


// Chopper I (US set 1)

static struct BurnRomInfo chopperRomDesc[] = {
	{ "kk_01.rom",		0x10000, 0x8fa2f839, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "kk_04.rom",		0x10000, 0x004f7d9a, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "kk_03.rom",		0x10000, 0xdbaafb87, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "up03_k1.rom",	0x00400, 0x7f07a45c, 14 | BRF_GRA },	      //  3 Color Data
	{ "up03_l1.rom",	0x00400, 0x15359fc3, 14 | BRF_GRA },	      //  4
	{ "up03_k2.rom",	0x00400, 0x79b50f7d, 14 | BRF_GRA },	      //  5

	{ "kk_05.rom",		0x08000, 0xdefc0987, 4 | BRF_GRA },           //  6 Text Characters

	{ "kk_10.rom",		0x10000, 0x5cf4d22b, 6 | BRF_GRA },           //  7 Background Tiles
	{ "kk_11.rom",		0x10000, 0x9af4cad0, 6 | BRF_GRA },           //  8
	{ "kk_12.rom",		0x10000, 0x02fec778, 6 | BRF_GRA },           //  9
	{ "kk_13.rom",		0x10000, 0x2756817d, 6 | BRF_GRA },           // 10

	{ "kk_09.rom",		0x08000, 0x653c4342, 7 | BRF_GRA },           // 11 16x16 Sprites
	{ "kk_08.rom",		0x08000, 0x2da45894, 7 | BRF_GRA },           // 12
	{ "kk_07.rom",		0x08000, 0xa0ebebdf, 7 | BRF_GRA },           // 13
	{ "kk_06.rom",		0x08000, 0x284fad9e, 7 | BRF_GRA },           // 14

	{ "kk_18.rom",		0x10000, 0x6abbff36, 8 | BRF_GRA },           // 15 32x32 Sprites
	{ "kk_19.rom",		0x10000, 0x5283b4d3, 8 | BRF_GRA },           // 16
	{ "kk_20.rom",		0x10000, 0x6403ddf2, 8 | BRF_GRA },           // 17
	{ "kk_21.rom",		0x10000, 0x9f411940, 8 | BRF_GRA },           // 18
	{ "kk_14.rom",		0x10000, 0x9bad9e25, 8 | BRF_GRA },           // 19
	{ "kk_15.rom",		0x10000, 0x89faf590, 8 | BRF_GRA },           // 20
	{ "kk_16.rom",		0x10000, 0xefb1fb6c, 8 | BRF_GRA },           // 21
	{ "kk_17.rom",		0x10000, 0x6b7fb0a5, 8 | BRF_GRA },           // 22

	{ "kk_02.rom",		0x10000, 0x06169ae0, 15 | BRF_SND },          // 23 Samples

	{ "pal16r6b.2c",	0x00104, 0x311e5ae6, 0 | BRF_OPT },           // 24 PLDs
};

STD_ROM_PICK(chopper)
STD_ROM_FN(chopper)

struct BurnDriver BurnDrvChopper = {
	"chopper", NULL, NULL, NULL, "1988",
	"Chopper I (US set 1)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, chopperRomInfo, chopperRomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo,
	Chopper1Init, DrvExit, ChopperFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Chopper I (US set 2)

static struct BurnRomInfo chopperaRomDesc[] = {
	{ "1a.rom",		0x10000, 0xdc325860, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "4a.rom",		0x10000, 0x56d10ba3, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "kk_03.rom",		0x10000, 0xdbaafb87, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "up03_k1.rom",	0x00400, 0x7f07a45c, 14 | BRF_GRA },	      //  3 Color Data
	{ "up03_l1.rom",	0x00400, 0x15359fc3, 14 | BRF_GRA },	      //  4
	{ "up03_k2.rom",	0x00400, 0x79b50f7d, 14 | BRF_GRA },	      //  5

	{ "kk_05.rom",		0x08000, 0xdefc0987, 4 | BRF_GRA },           //  6 Text Characters

	{ "kk_10.rom",		0x10000, 0x5cf4d22b, 6 | BRF_GRA },           //  7 Background Tiles
	{ "11a.rom",		0x10000, 0x881ac259, 6 | BRF_GRA },           //  8
	{ "12a.rom",		0x10000, 0xde96b331, 6 | BRF_GRA },           //  9
	{ "kk_13.rom",		0x10000, 0x2756817d, 6 | BRF_GRA },           // 10

	{ "9a.rom",			0x08000, 0x106c2dcc, 7 | BRF_GRA },           // 11 16x16 Sprites
	{ "8a.rom",			0x08000, 0xd4f88f62, 7 | BRF_GRA },           // 12
	{ "7a.rom",			0x08000, 0x28ae39f9, 7 | BRF_GRA },           // 13
	{ "6a.rom",			0x08000, 0x16774a36, 7 | BRF_GRA },           // 14

	{ "kk_18.rom",		0x10000, 0x6abbff36, 8 | BRF_GRA },           // 15 32x32 Sprites
	{ "kk_19.rom",		0x10000, 0x5283b4d3, 8 | BRF_GRA },           // 16
	{ "kk_20.rom",		0x10000, 0x6403ddf2, 8 | BRF_GRA },           // 17
	{ "kk_21.rom",		0x10000, 0x9f411940, 8 | BRF_GRA },           // 18
	{ "kk_14.rom",		0x10000, 0x9bad9e25, 8 | BRF_GRA },           // 19
	{ "kk_15.rom",		0x10000, 0x89faf590, 8 | BRF_GRA },           // 20
	{ "kk_16.rom",		0x10000, 0xefb1fb6c, 8 | BRF_GRA },           // 21
	{ "kk_17.rom",		0x10000, 0x6b7fb0a5, 8 | BRF_GRA },           // 22

	{ "kk_02.rom",		0x10000, 0x06169ae0, 15 | BRF_SND },          // 23 Samples

	{ "pal16r6b.2c",	0x00104, 0x311e5ae6, 0 | BRF_OPT },           // 24 PLDs
};

STD_ROM_PICK(choppera)
STD_ROM_FN(choppera)

struct BurnDriver BurnDrvChoppera = {
	"choppera", "chopper", NULL, NULL, "1988",
	"Chopper I (US set 2)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, chopperaRomInfo, chopperaRomName, NULL, NULL, ChopperaInputInfo, ChopperaDIPInfo,
	ChopperaInit, DrvExit, ChopperFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Chopper I (US set 3)

static struct BurnRomInfo chopperbRomDesc[] = {
	{ "chpri-1.bin",	0x10000, 0xa4e6e978, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "chpri-4.bin",	0x10000, 0x56d10ba3, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "kk_03.rom",		0x10000, 0xdbaafb87, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "up03_k1.rom",	0x00400, 0x7f07a45c, 14 | BRF_GRA },	      //  3 Color Data
	{ "up03_l1.rom",	0x00400, 0x15359fc3, 14 | BRF_GRA },	      //  4
	{ "up03_k2.rom",	0x00400, 0x79b50f7d, 14 | BRF_GRA },	      //  5

	{ "kk_05.rom",		0x08000, 0xdefc0987, 4 | BRF_GRA },           //  6 Text Characters

	{ "kk_10.rom",		0x10000, 0x5cf4d22b, 6 | BRF_GRA },           //  7 bBackground Tiles
	{ "chpri-11.bin",	0x10000, 0x881ac259, 6 | BRF_GRA },           //  8
	{ "chpri-12.bin",	0x10000, 0xde96b331, 6 | BRF_GRA },           //  9
	{ "kk_13.rom",		0x10000, 0x2756817d, 6 | BRF_GRA },           // 10

	{ "chpri-9.bin",	0x08000, 0x106c2dcc, 7 | BRF_GRA },           // 11 16x16 Sprites
	{ "chpri-8.bin",	0x08000, 0xd4f88f62, 7 | BRF_GRA },           // 12
	{ "chpri-7.bin",	0x08000, 0x28ae39f9, 7 | BRF_GRA },           // 13
	{ "chpri-6.bin",	0x08000, 0x16774a36, 7 | BRF_GRA },           // 14

	{ "kk_18.rom",		0x10000, 0x6abbff36, 8 | BRF_GRA },           // 15 32x32 Sprites
	{ "kk_19.rom",		0x10000, 0x5283b4d3, 8 | BRF_GRA },           // 16
	{ "kk_20.rom",		0x10000, 0x6403ddf2, 8 | BRF_GRA },           // 17
	{ "kk_21.rom",		0x10000, 0x9f411940, 8 | BRF_GRA },           // 18
	{ "kk_14.rom",		0x10000, 0x9bad9e25, 8 | BRF_GRA },           // 19
	{ "kk_15.rom",		0x10000, 0x89faf590, 8 | BRF_GRA },           // 20
	{ "kk_16.rom",		0x10000, 0xefb1fb6c, 8 | BRF_GRA },           // 21
	{ "kk_17.rom",		0x10000, 0x6b7fb0a5, 8 | BRF_GRA },           // 22

	{ "kk_02.rom",		0x10000, 0x06169ae0, 15 | BRF_SND },          // 23 Samples

	{ "pal16r6b.2c",	0x00104, 0x311e5ae6, 0 | BRF_OPT },           // 24 PLDs
};

STD_ROM_PICK(chopperb)
STD_ROM_FN(chopperb)

struct BurnDriver BurnDrvChopperb = {
	"chopperb", "chopper", NULL, NULL, "1988",
	"Chopper I (US set 3)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, chopperbRomInfo, chopperbRomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo,
	Chopper1Init, DrvExit, ChopperFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// Koukuu Kihei Monogatari - The Legend of Air Cavalry (Japan)

static struct BurnRomInfo legofairRomDesc[] = {
	{ "up03_m4.rom",	0x10000, 0x79a485c0, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "up03_m8.rom",	0x10000, 0x96d3a4d9, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "kk_03.rom",		0x10000, 0xdbaafb87, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "up03_k1.rom",	0x00400, 0x7f07a45c, 14 | BRF_GRA },          //  3 Color Data
	{ "up03_l1.rom",	0x00400, 0x15359fc3, 14 | BRF_GRA },          //  4
	{ "up03_k2.rom",	0x00400, 0x79b50f7d, 14 | BRF_GRA },          //  5

	{ "kk_05.rom",		0x08000, 0xdefc0987, 4 | BRF_GRA },           //  6 Text Tiles

	{ "kk_10.rom",		0x10000, 0x5cf4d22b, 6 | BRF_GRA },           //  7 Background Tiles
	{ "kk_11.rom",		0x10000, 0x9af4cad0, 6 | BRF_GRA },           //  8
	{ "kk_12.rom",		0x10000, 0x02fec778, 6 | BRF_GRA },           //  9
	{ "kk_13.rom",		0x10000, 0x2756817d, 6 | BRF_GRA },           // 10

	{ "kk_09.rom",		0x08000, 0x653c4342, 7 | BRF_GRA },           // 11 16x16 Sprites
	{ "kk_08.rom",		0x08000, 0x2da45894, 7 | BRF_GRA },           // 12
	{ "kk_07.rom",		0x08000, 0xa0ebebdf, 7 | BRF_GRA },           // 13
	{ "kk_06.rom",		0x08000, 0x284fad9e, 7 | BRF_GRA },           // 14

	{ "kk_18.rom",		0x10000, 0x6abbff36, 8 | BRF_GRA },           // 15 32x32 Sprites
	{ "kk_19.rom",		0x10000, 0x5283b4d3, 8 | BRF_GRA },           // 16
	{ "kk_20.rom",		0x10000, 0x6403ddf2, 8 | BRF_GRA },           // 17
	{ "kk_21.rom",		0x10000, 0x9f411940, 8 | BRF_GRA },           // 18
	{ "kk_14.rom",		0x10000, 0x9bad9e25, 8 | BRF_GRA },           // 19
	{ "kk_15.rom",		0x10000, 0x89faf590, 8 | BRF_GRA },           // 20
	{ "kk_16.rom",		0x10000, 0xefb1fb6c, 8 | BRF_GRA },           // 21
	{ "kk_17.rom",		0x10000, 0x6b7fb0a5, 8 | BRF_GRA },           // 22

	{ "kk_02.rom",		0x10000, 0x06169ae0, 15 | BRF_SND },          // 23 Samples
};

STD_ROM_PICK(legofair)
STD_ROM_FN(legofair)

struct BurnDriver BurnDrvLegofair = {
	"legofair", "chopper", NULL, NULL, "1988",
	"Koukuu Kihei Monogatari - The Legend of Air Cavalry (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_VERSHOOT, 0,
	NULL, legofairRomInfo, legofairRomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo,
	Chopper1Init, DrvExit, ChopperFrame, GwarDraw, DrvScan, &DrvRecalc, 0x400,
	224, 400, 3, 4
};


// TouchDown Fever (US)

static struct BurnRomInfo tdfeverRomDesc[] = {
	{ "td2-ver3u.6c",	0x10000, 0x92138fe4, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "td1-ver3u.2c",	0x10000, 0x798711f5, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "td3-ver2u.3j",	0x10000, 0x5d13e0b1, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "2t.8e",		0x00400, 0x67bdf8a0, 14 | BRF_GRA },          //  3 Color Data
	{ "1t.8d",		0x00400, 0x9c4a9198, 14 | BRF_GRA },          //  4
	{ "3t.9e",		0x00400, 0xc93c18e8, 14 | BRF_GRA },          //  5

	{ "td14-u.4n",		0x08000, 0xe841bf1a, 4 | BRF_GRA },           //  6 Text Characters

	{ "td15.8d",		0x10000, 0xad6e0927, 6 | BRF_GRA },           //  7 Background Tiles
	{ "td16.8e",		0x10000, 0x181db036, 6 | BRF_GRA },           //  8
	{ "td17.8f",		0x10000, 0xc5decca3, 6 | BRF_GRA },           //  9
	{ "td18-ver2u.8g",	0x10000, 0x3924da37, 6 | BRF_GRA },           // 10
	{ "td19.8j",		0x10000, 0xbc17ea7f, 6 | BRF_GRA },           // 11

	{ "td13.2t",		0x10000, 0x88e2e819, 8 | BRF_GRA },           // 12 32x32 Sprites
	{ "td12-1.2s",		0x10000, 0xf6f83d63, 8 | BRF_GRA },           // 13
	{ "td11.2r",		0x10000, 0xa0d53fbd, 8 | BRF_GRA },           // 14
	{ "td10-1.20",		0x10000, 0xc8c71c7b, 8 | BRF_GRA },           // 15
	{ "td9.2n",		0x10000, 0xa8979657, 8 | BRF_GRA },           // 16
	{ "td8-1.2l",		0x10000, 0x28f49182, 8 | BRF_GRA },           // 17
	{ "td7.2k",		0x10000, 0x72a5590d, 8 | BRF_GRA },           // 18
	{ "td6-1.2j",		0x10000, 0x9b6d4053, 8 | BRF_GRA },           // 19

	{ "td5.7p",		0x10000, 0x04794557, 15 | BRF_SND },          // 20 Samples
	{ "td4.7n",		0x10000, 0x155e472e, 15 | BRF_SND },          // 21
};

STD_ROM_PICK(tdfever)
STD_ROM_FN(tdfever)

struct BurnDriver BurnDrvTdfever = {
	"tdfever", NULL, NULL, NULL, "1987",
	"TouchDown Fever (US)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/*BDF_GAME_WORKING |*/0, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tdfeverRomInfo, tdfeverRomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo, //TdfeverInputInfo, TdfeverDIPInfo,
	TdfeverInit, DrvExit, GwarFrame, TdfeverDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// TouchDown Fever (Japan)

static struct BurnRomInfo tdfeverjRomDesc[] = {
	{ "up02_c6.rom",	0x10000, 0x88d88ec4, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "up02_c2.rom",	0x10000, 0x191e6442, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "up02_j3.rom",	0x10000, 0x4e4d71c7, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "2t.8e",		0x00400, 0x67bdf8a0, 14 | BRF_GRA },          //  3 Color Data
	{ "1t.8d",		0x00400, 0x9c4a9198, 14 | BRF_GRA },          //  4
	{ "3t.9e",		0x00400, 0xc93c18e8, 14 | BRF_GRA },          //  5

	{ "td14.4n",		0x08000, 0xaf9bced5, 4 | BRF_GRA },           //  6 Text Characters

	{ "td15.8d",		0x10000, 0xad6e0927, 6 | BRF_GRA },           //  7 Background Tiles
	{ "td16.8e",		0x10000, 0x181db036, 6 | BRF_GRA },           //  8
	{ "td17.8f",		0x10000, 0xc5decca3, 6 | BRF_GRA },           //  9
	{ "up01.8g",		0x10000, 0x4512cdfb, 6 | BRF_GRA },           // 10
	{ "td19.8j",		0x10000, 0xbc17ea7f, 6 | BRF_GRA },           // 11

	{ "td13.2t",		0x10000, 0x88e2e819, 8 | BRF_GRA },           // 12 32x32 Sprites
	{ "td12-1.2s",		0x10000, 0xf6f83d63, 8 | BRF_GRA },           // 13
	{ "td11.2r",		0x10000, 0xa0d53fbd, 8 | BRF_GRA },           // 14
	{ "td10-1.20",		0x10000, 0xc8c71c7b, 8 | BRF_GRA },           // 15
	{ "td9.2n",		0x10000, 0xa8979657, 8 | BRF_GRA },           // 16
	{ "td8-1.2l",		0x10000, 0x28f49182, 8 | BRF_GRA },           // 17
	{ "td7.2k",		0x10000, 0x72a5590d, 8 | BRF_GRA },           // 18
	{ "td6-1.2j",		0x10000, 0x9b6d4053, 8 | BRF_GRA },           // 19

	{ "td5.7p",		0x10000, 0x04794557, 15 | BRF_SND },          // 20 Samples
	{ "td4.7n",		0x10000, 0x155e472e, 15 | BRF_SND },          // 21
};

STD_ROM_PICK(tdfeverj)
STD_ROM_FN(tdfeverj)

struct BurnDriver BurnDrvTdfeverj = {
	"tdfeverj", "tdfever", NULL, NULL, "1987",
	"TouchDown Fever (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/*BDF_GAME_WORKING |*/0 | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tdfeverjRomInfo, tdfeverjRomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo, //TdfeverInputInfo, TdfeverDIPInfo,
	TdfeverInit, DrvExit, GwarFrame, TdfeverDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// TouchDown Fever 2

static struct BurnRomInfo tdfever2RomDesc[] = {
	{ "td02.6c",		0x10000, 0x9e3eaed8, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "td01.1c",		0x10000, 0x0ec294c0, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "td03.2j",		0x10000, 0x4092f16c, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "up03-2.8e",		0x00400, 0x1593c302, 14 | BRF_GRA },          //  3 Color Data
	{ "up03-2.8d",		0x00400, 0xac9df947, 14 | BRF_GRA },          //  4
	{ "up03-2.9e",		0x00400, 0x73cdf192, 14 | BRF_GRA },          //  5

	{ "td06.3n",		0x08000, 0xd6521b0d, 4 | BRF_GRA },           //  6 Text Characters

	{ "td15.8d",		0x10000, 0xad6e0927, 6 | BRF_GRA },           //  7 Background Tiles
	{ "td16.8e",		0x10000, 0x181db036, 6 | BRF_GRA },           //  8
	{ "td17.8f",		0x10000, 0xc5decca3, 6 | BRF_GRA },           //  9
	{ "td18.8g",		0x10000, 0x1a5a2200, 6 | BRF_GRA },           // 10
	{ "td19-2.8j",		0x10000, 0xf1081329, 6 | BRF_GRA },           // 11
	{ "td20.8k",		0x10000, 0x86cbb2e6, 6 | BRF_GRA },           // 12

	{ "td14.2t",		0x10000, 0x88e2e819, 8 | BRF_GRA },           // 13 32x32 Sprites
	{ "td13.2s",		0x10000, 0xc9bb9138, 8 | BRF_GRA },           // 14
	{ "td12.2r",		0x10000, 0xa0d53fbd, 8 | BRF_GRA },           // 15
	{ "td11.2p",		0x10000, 0xd43abc81, 8 | BRF_GRA },           // 16
	{ "td10.2n",		0x10000, 0xa8979657, 8 | BRF_GRA },           // 17
	{ "td09.2l",		0x10000, 0xc93b6cd3, 8 | BRF_GRA },           // 18
	{ "td08.2k",		0x10000, 0x72a5590d, 8 | BRF_GRA },           // 19
	{ "td07.2j",		0x10000, 0x4845e78b, 8 | BRF_GRA },           // 20

	{ "td05.7p",		0x10000, 0xe332e41f, 15 | BRF_SND },          // 21 Samples
	{ "td04.7n",		0x10000, 0x98af6d2d, 15 | BRF_SND },          // 22
	{ "td22.7l",		0x10000, 0x34b4bce9, 15 | BRF_SND },          // 23
	{ "td21.7k",		0x10000, 0xf5a96d8e, 15 | BRF_SND },          // 24
};

STD_ROM_PICK(tdfever2)
STD_ROM_FN(tdfever2)

struct BurnDriver BurnDrvTdfever2 = {
	"tdfever2", "tdfever", NULL, NULL, "1988",
	"TouchDown Fever 2\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/*BDF_GAME_WORKING |*/0 | BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, tdfever2RomInfo, tdfever2RomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo, //TdfeverInputInfo, TdfeverDIPInfo,
	TdfeverInit, DrvExit, GwarFrame, TdfeverDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// Fighting Soccer (version 4)

static struct BurnRomInfo fsoccerRomDesc[] = {
	{ "fs3_ver4.bin",	0x10000, 0x94c3f918, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "fs1_ver4.bin",	0x10000, 0x97830108, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "fs2.3j",		0x10000, 0x9ee54ea1, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "2.8e",		0x00400, 0xbf4ac706, 14 | BRF_GRA },          //  3 Color Data
	{ "1.8d",		0x00400, 0x1bac8010, 14 | BRF_GRA },          //  4
	{ "3.9e",		0x00400, 0xdbeddb14, 14 | BRF_GRA },          //  5

	{ "fs13.4n",		0x08000, 0x0de7b7ad, 4 | BRF_GRA },           //  6 Text Characters

	{ "fs14.8d",		0x10000, 0x38c38b40, 6 | BRF_GRA },           //  7 Background Tiles
	{ "fs15.8e",		0x10000, 0xa614834f, 6 | BRF_GRA },           //  8

	{ "fs12.2t",		0x10000, 0xb2442c30, 8 | BRF_GRA },           //  9 32x32 Sprites
	{ "fs11.2s",		0x10000, 0x022f3e96, 8 | BRF_GRA },           // 10
	{ "fs10.2r",		0x10000, 0xe42864d8, 8 | BRF_GRA },           // 11
	{ "fs9.2p",		0x10000, 0xd8112aa6, 8 | BRF_GRA },           // 12
	{ "fs8.2n",		0x10000, 0x11156a7d, 8 | BRF_GRA },           // 13
	{ "fs7.2l",		0x10000, 0xd584964b, 8 | BRF_GRA },           // 14
	{ "fs6.2k",		0x10000, 0x588d14b3, 8 | BRF_GRA },           // 15
	{ "fs5.2j",		0x10000, 0xdef2f1d8, 8 | BRF_GRA },           // 16

	{ "fs4.bin",		0x10000, 0x435c3716, 15 | BRF_SND },          // 17 Samples
};

STD_ROM_PICK(fsoccer)
STD_ROM_FN(fsoccer)

struct BurnDriver BurnDrvFsoccer = {
	"fsoccer", NULL, NULL, NULL, "1988",
	"Fighting Soccer (version 4)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/*BDF_GAME_WORKING |*/0, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, fsoccerRomInfo, fsoccerRomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo, //FsoccerInputInfo, FsoccerDIPInfo,
	TdfeverInit, DrvExit, GwarFrame, FsoccerDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// Fighting Soccer (Japan)

static struct BurnRomInfo fsoccerjRomDesc[] = {
	{ "fs3.6c",		0x10000, 0xc5f505fa, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "fs1.2c",		0x10000, 0x2f68e38b, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "fs2.3j",		0x10000, 0x9ee54ea1, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "2.8e",		0x00400, 0xbf4ac706, 14 | BRF_GRA },          //  3 Color Data
	{ "1.8d",		0x00400, 0x1bac8010, 14 | BRF_GRA },          //  4
	{ "3.9e",		0x00400, 0xdbeddb14, 14 | BRF_GRA },          //  5

	{ "fs13.4n",		0x08000, 0x0de7b7ad, 4 | BRF_GRA },           //  6 Text Characters

	{ "fs14.8d",		0x10000, 0x38c38b40, 6 | BRF_GRA },           //  7 Background Tiles
	{ "fs15.8e",		0x10000, 0xa614834f, 6 | BRF_GRA },           //  8

	{ "fs12.2t",		0x10000, 0xb2442c30, 8 | BRF_GRA },           //  9 32x32 Sprites
	{ "fs11.2s",		0x10000, 0x022f3e96, 8 | BRF_GRA },           // 10
	{ "fs10.2r",		0x10000, 0xe42864d8, 8 | BRF_GRA },           // 11
	{ "fs9.2p",		0x10000, 0xd8112aa6, 8 | BRF_GRA },           // 12
	{ "fs8.2n",		0x10000, 0x11156a7d, 8 | BRF_GRA },           // 13
	{ "fs7.2l",		0x10000, 0xd584964b, 8 | BRF_GRA },           // 14
	{ "fs6.2k",		0x10000, 0x588d14b3, 8 | BRF_GRA },           // 15
	{ "fs5.2j",		0x10000, 0xdef2f1d8, 8 | BRF_GRA },           // 16

	{ "fs4.7p",		0x10000, 0x435c3716, 15 | BRF_SND },          // 17 Samples
};

STD_ROM_PICK(fsoccerj)
STD_ROM_FN(fsoccerj)

struct BurnDriver BurnDrvFsoccerj = {
	"fsoccerj", "fsoccer", NULL, NULL, "1988",
	"Fighting Soccer (Japan)\0", NULL, "SNK", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	/* |*/ BDF_CLONE, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, fsoccerjRomInfo, fsoccerjRomName, NULL, NULL, ChopperInputInfo, ChopperDIPInfo, //FsoccerInputInfo, FsoccerDIPInfo,
	TdfeverInit, DrvExit, GwarFrame, FsoccerDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// Fighting Soccer (Joystick hack bootleg)

static struct BurnRomInfo fsoccerbRomDesc[] = {
	{ "ft-003.bin",		0x10000, 0x649d4448, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "ft-001.bin",		0x10000, 0x2f68e38b, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "fs2.3j",		0x10000, 0x9ee54ea1, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "2.8e",		0x00400, 0xbf4ac706, 14 | BRF_GRA },          //  3 Color Data
	{ "1.8d",		0x00400, 0x1bac8010, 14 | BRF_GRA },          //  4
	{ "3.9e",		0x00400, 0xdbeddb14, 14 | BRF_GRA },          //  5

	{ "fs13.4n",		0x08000, 0x0de7b7ad, 4 | BRF_GRA },           //  6 Text Characters

	{ "fs14.8d",		0x10000, 0x38c38b40, 6 | BRF_GRA },           //  7 Background Tiles
	{ "fs15.8e",		0x10000, 0xa614834f, 6 | BRF_GRA },           //  8

	{ "fs12.2t",		0x10000, 0xb2442c30, 8 | BRF_GRA },           //  9 32x32 Sprites
	{ "fs11.2s",		0x10000, 0x022f3e96, 8 | BRF_GRA },           // 10
	{ "fs10.2r",		0x10000, 0xe42864d8, 8 | BRF_GRA },           // 11
	{ "fs9.2p",		0x10000, 0xd8112aa6, 8 | BRF_GRA },           // 12
	{ "fs8.2n",		0x10000, 0x11156a7d, 8 | BRF_GRA },           // 13
	{ "fs7.2l",		0x10000, 0xd584964b, 8 | BRF_GRA },           // 14
	{ "fs6.2k",		0x10000, 0x588d14b3, 8 | BRF_GRA },           // 15
	{ "fs5.2j",		0x10000, 0xdef2f1d8, 8 | BRF_GRA },           // 16

	{ "fs4.bin",		0x10000, 0x435c3716, 15 | BRF_SND },          // 17 Samples
};

STD_ROM_PICK(fsoccerb)
STD_ROM_FN(fsoccerb)

struct BurnDriver BurnDrvFsoccerb = {
	"fsoccerb", "fsoccer", NULL, NULL, "1988",
	"Fighting Soccer (Joystick hack bootleg)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, fsoccerbRomInfo, fsoccerbRomName, NULL, NULL, FsoccerInputInfo, FsoccerDIPInfo,
	TdfeverInit, DrvExit, GwarFrame, FsoccerDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};


// Fighting Soccer (Joystick hack bootleg, alt)

static struct BurnRomInfo fsoccerbaRomDesc[] = {
	{ "fs3.c6",		0x10000, 0xe644d207, 1 | BRF_ESS | BRF_PRG }, //  0 Z80 #0 code

	{ "fs1_ver4.bin",	0x10000, 0x97830108, 2 | BRF_ESS | BRF_PRG }, //  1 Z80 #1 code

	{ "fs2.3j",		0x10000, 0x9ee54ea1, 3 | BRF_ESS | BRF_PRG }, //  2 Z80 #2 code

	{ "2.8e",		0x00400, 0xbf4ac706, 14 | BRF_GRA },          //  3 Color Data
	{ "1.8d",		0x00400, 0x1bac8010, 14 | BRF_GRA },          //  4
	{ "3.9e",		0x00400, 0xdbeddb14, 14 | BRF_GRA },          //  5

	{ "fs13.4n",		0x08000, 0x0de7b7ad, 4 | BRF_GRA },           //  6 Text Characters

	{ "fs14.8d",		0x10000, 0x38c38b40, 6 | BRF_GRA },           //  7 Background Tiles
	{ "fs15.8e",		0x10000, 0xa614834f, 6 | BRF_GRA },           //  8

	{ "fs12.2t",		0x10000, 0xb2442c30, 8 | BRF_GRA },           //  9 32x32 Sprites
	{ "fs11.2s",		0x10000, 0x022f3e96, 8 | BRF_GRA },           // 10
	{ "fs10.2r",		0x10000, 0xe42864d8, 8 | BRF_GRA },           // 11
	{ "fs9.2p",		0x10000, 0xd8112aa6, 8 | BRF_GRA },           // 12
	{ "fs8.2n",		0x10000, 0x11156a7d, 8 | BRF_GRA },           // 13
	{ "fs7.2l",		0x10000, 0xd584964b, 8 | BRF_GRA },           // 14
	{ "fs6.2k",		0x10000, 0x588d14b3, 8 | BRF_GRA },           // 15
	{ "fs5.2j",		0x10000, 0xdef2f1d8, 8 | BRF_GRA },           // 16

	{ "fs4.7p",		0x10000, 0x435c3716, 15 | BRF_SND },          // 17 Samples
};

STD_ROM_PICK(fsoccerba)
STD_ROM_FN(fsoccerba)

struct BurnDriver BurnDrvFsoccerba = {
	"fsoccerba", "fsoccer", NULL, NULL, "1988",
	"Fighting Soccer (Joystick hack bootleg, alt)\0", NULL, "bootleg", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HISCORE_SUPPORTED, 2, HARDWARE_MISC_PRE90S, GBF_SPORTSMISC, 0,
	NULL, fsoccerbaRomInfo, fsoccerbaRomName, NULL, NULL, FsoccerInputInfo, FsoccerDIPInfo,
	TdfeverInit, DrvExit, GwarFrame, FsoccerDraw, DrvScan, &DrvRecalc, 0x400,
	400, 224, 4, 3
};
