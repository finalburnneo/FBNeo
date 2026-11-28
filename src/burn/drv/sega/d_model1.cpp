// FB Neo Sega Model 1 driver module
// Based on MAME driver by Olivier Galibert, Dirk Best, Ariane Fugmann, R. Belmont
//
// Hardware:
//  V60 @ 16MHz, Fujitsu MB86233 "TGP" @ 40MHz (geometry coprocessor, fed by fifos)
//  System 24 tilemaps, flat shaded polygon rasterizer (simulated, MAME style)
//  I/O board: Z80 @ 4MHz + 315-5338A + MSM6253 adc + 93C45 eeprom
//   (Wing War: model1io2, TMPZ84C015 @ 9.83MHz with its CTC, same 315-5338A)
//  Sound board: 68000 @ 10MHz, YM3438, 2x MultiPCM, linked by uPD71051 usarts
//  Star Wars: digital sound board, Z80 @ 4MHz + MPEG layer 2 decoder
//
// Extras:
//  "Resolution" dip (read at start): the polygons are rasterized at 720p/1080p/1440p/2160p, the tilemaps scaled up
//   (area filtered by default, so that their pixels stay even at these non integer scales)
//  Virtua Racing: the aspect follows the game's monitor setting in the eeprom (16:9 wide monitor: anamorphic races)
//  Virtua Racing / Formula: the drive board commands (wheel vibration, jolts) are turned into rumble,
//   by default without the light road vibration and (virtua racing) outside the races ("Rumble" dip),
//   plus (virtua racing) pulses when the car hits a wall or another car, flies off or spins out
//
// Not emulated:
//  The communication board's Z80 (link play), the board reads as present but idle
//  The R360 control board (Wing War R360), its replies are simulated
//  The Polhemus head tracker board (NetMerc), its pose is written to the dpram, a second stick turns the head

#include "tiles_generic.h"
#include "v60_intf.h"
#include "mb86233_intf.h"
#include "m68000_intf.h"
#include "z80_intf.h"
#include "z80ctc.h"
#include "z80daisy.h"
#include "burn_ym2612.h"
#include "multipcm.h"
#include "model1_snd.h"
#include "model1_io.h"
#include "sega_315_5292.h"
#include "eeprom.h"
#include "sega_dsb.h"
#include <math.h>
#define THREADY_NO_GLOBAL
#include "thready.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;

static UINT8 *DrvV60ROM;
static UINT32 *DrvTGPROM;
static UINT32 *DrvCoproTables;
static UINT32 *DrvCoproData;
static UINT32 *DrvPolyROM;
static UINT8 *Drv68KROM;
static UINT8 *DrvPCMROM0;
static UINT8 *DrvPCMROM1;
static UINT8 *DrvIOROM;
static UINT8 *DrvEEPROM;

static UINT8 *DrvNVRAM;
static UINT8 *DrvMainRAM;
static UINT8 *DrvDispList0;
static UINT8 *DrvDispList1;
static UINT8 *DrvTileRAM;
static UINT8 *DrvCharRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvXlatRAM;
static UINT8 *DrvCommRAM;
static UINT8 *DrvDPRAM;
static UINT32 *DrvCoproRAM;
static UINT16 *DrvTGPRAM;
static UINT32 *DrvPolyRAM;
static UINT8 *Drv68KRAM;
static UINT8 *DrvIORAM;
static UINT8 *DrvDSBROM;
static UINT8 *DrvDSBMPEG;
static UINT8 *DrvDSBRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

// high resolution rendering: the 3d is rasterized at the chosen size, the tilemaps are scaled up
#define NATIVE_W		496
#define NATIVE_H		384
static INT32 nHiresMode;		// 0 = native
static float fHiresScaleX, fHiresScaleY;
static UINT16 *TileBack;		// background tilemaps, native resolution
static UINT16 *TileHud;			// hud tilemaps, native resolution, 0xffff = transparent
static INT32 *HiresMapX;
static INT32 *HiresMapY;
static INT32 bHiresSharp;		// 2d layers: area filtered (sharp bilinear) instead of nearest
static UINT16 *HiresWX;			// weight (0-256) of HiresMap[] pixel, the rest goes to the next one
static UINT16 *HiresWY;
static UINT32 *HiresRGB;		// blended pixels (0x00rrggbb), where pTransDraw holds HIRES_RGB
static UINT32 *DrvPalRGB;		// tile palette as 0x00rrggbb
static UINT32 HiresLut[3][256];	// BurnHighCol() of each channel, or-ed together
static INT32 bHiresRGB888;

#define HIRES_MAX_W		3840
#define HIRES_MAX_H		2160

#define M1_MAX_PARTS	8		// the render thread and up to 7 workers

struct hires_rows {				// per thread: 2 cached rows of each filtered layer
	UINT32 back[2][HIRES_MAX_W];
	UINT32 hud[2][HIRES_MAX_W];
	UINT16 alpha[2][HIRES_MAX_W];
	UINT32 out[HIRES_MAX_W];
	INT32 back_tag[2], hud_tag[2];
	UINT32 hud_mask[2];			// 16 native pixels per bit, dilated by one to the left (pixel + its right neighbour)
};
static INT32 HiresChunkX[33];		// first output column of each 16 native pixel chunk
static hires_rows *HiresRows;

#define HIRES_BACK		0x7f7f	// unused pens (tiles < 0x1000, polygons >= 0x8000):
#define HIRES_RGB		0x7e7e	// background not covered, filtered at transfer / blended pixel in HiresRGB

static UINT32 nCoproDataMask;	// in dwords

// per game configuration
static INT32 nIOBios;			// model1io bios rom (0x80 + n)
static INT32 bHasComm;
static INT32 bHasEEPROMImage;
static INT32 nBootMirror;		// boot roms also at 0: 0 = no, 1 = once, 2 = repeated over 1MB
static INT32 bHasDSB;
static INT32 bIOBoard2;			// model1io2 (tmpz84c015) instead of model1io
static INT32 bHasR360;
static INT32 bHasHMD;			// netmerc head tracker
static INT32 bHasDriveBoard;	// force feedback wheel, turned into rumble
static INT32 bHasWideMonitor;	// virtua racing has a 16:9 monitor setting
static INT32 bHasMenuWheel;		// virtua racing: stepped course select with the stick (1: vr, 2: vrext)
static void hmd_pose_write();
static INT32 nAnalogType;		// 0 = driving, 1 = none, 2 = flight sticks + throttle, 3 = wing war, 4 = wing war r360, 5 = netmerc

#define POLY_ROM_SIZE	0x400000	// dwords
#define POLY_RAM_SIZE	0x8000		// dwords
#define TGP_RAM_SIZE	(0x100000 - 0x40000)

// main board
static UINT8 irq_status;
static UINT8 irq_mask;
static INT32 last_irq;
static UINT16 timer_period[2];
static UINT16 timer_val[2];
static INT32 timer_remaining[2];	// v60 cycles
static UINT16 timer_mode;
static INT32 rom_bank;
static UINT16 listctl[2];
static UINT32 frame_number;
static INT32 v60_irq_dirty;

// coprocessor
static UINT16 v60_copro_ram_adr;
static UINT16 v60_copro_ram_latch[2];
static UINT32 v60_copro_fifo_r;
static UINT32 v60_copro_fifo_w;
static UINT32 copro_sincos_base;
static UINT32 copro_inv_base;
static UINT32 copro_isqrt_base;
static UINT32 copro_atan_base[4];
static UINT32 copro_data_base;
static UINT32 copro_ram_adr[4];

#define FIFO_SIZE	16		// the hardware fifo, what goes over it is kept (as MAME does)
#define FIFO_BUF	64

struct copro_fifo {
	UINT32 data[FIFO_BUF];
	INT32 rd, count;
};

static copro_fifo fifo_in;		// v60 -> tgp
static copro_fifo fifo_out;		// tgp -> v60
static INT32 tgp_wait_in;		// tgp halted on an empty input fifo
static INT32 tgp_wait_out;		// tgp halted on a full output fifo
static INT32 tgp_v60_waiting;	// the v60 waits for the tgp to push (1) or pop (2): the tgp run ends there
static INT32 tgp_lead;			// tgp cycles run ahead of the v60 at the end of the frame

// wing war r360
static UINT8 r360_state;
static UINT8 r360_throttle;

// drive board (not emulated), its commands drive the rumble
static UINT8 drive_vibrate;		// 0-15
static UINT8 menu_wheel_on;
static UINT8 menu_band;			// course picked on the select screen
static UINT8 menu_repeat;		// frames to the next step while the stick is held
static INT8 menu_dir;
static UINT8 drive_slide;		// frames left of the power slide pulse (going off the road)
static UINT8 drive_turn;		// frames left of a wheel rotation pulse
static UINT8 drive_crash;		// frames left of the crash pulse
static UINT16 drive_speed[2];	// virtua racing: the car's speed, the last 2 frames
static UINT8 drive_slowing;		// virtua racing: frames the car has been slowing down with the gas on
static UINT8 drive_contact;		// frames left of the contact pulse
static UINT8 drive_airborne;	// virtua racing: the car is flying after a crash

// communication board (not running)
static UINT8 comm_cn;
static UINT8 comm_fg;

static INT32 usart_ticks_done;


// inputs
static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[4];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;
static INT16 DrvAnalogPort0 = 0;
static INT16 DrvAnalogPort1 = 0;
static INT16 DrvAnalogPort2 = 0;
static INT16 DrvAnalogPort3 = 0;
static INT16 DrvAnalogPort4 = 0;
static INT16 DrvAnalogPort5 = 0;
static UINT8 analog_values[8];	// 4-7: secondary controls

#define V60_CLOCK		16000000
#define TGP_CLOCK		(40000000 / 3)
#define SND_CLOCK		M1AUDIO_CLOCK
#define IO_CLOCK		M1IO_CLOCK
#define IO2_CLOCK		M1IO2_CLOCK
#define USART_CLOCK		500000

#define HTOTAL			656
#define VTOTAL			424
#define VBLANK_LINE		384

#define V60_CYCLES_FRAME	(HTOTAL * VTOTAL)	// the pixel clock is the v60 clock

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}

static struct BurnInputInfo VrInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	A("P1 Steering",		BIT_ANALOG_REL,	&DrvAnalogPort0,"p1 x-axis"	),
	A("P1 Accelerator",		BIT_ANALOG_REL,	&DrvAnalogPort1,"p1 fire 1"	),
	A("P1 Brake",			BIT_ANALOG_REL,	&DrvAnalogPort2,"p1 fire 2"	),
	{"P1 Shift Up",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 3"	},
	{"P1 Shift Down",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 4"	},
	{"P1 VR1 (Red)",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 5"	},
	{"P1 VR2 (Blue)",		BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 6"	},
	{"P1 VR3 (Yellow)",		BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 7"	},
	{"P1 VR4 (Green)",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 8"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Vr)

// fake dips, read at start (see DrvInit)
#define RESOLUTION_DIPS(dipval) \
	{0     , 0xfd, 0   , 5   , "Resolution"         }, \
	{dipval, 0x01, 0x07, 0x00, "Original"           }, \
	{dipval, 0x01, 0x07, 0x01, "720p"               }, \
	{dipval, 0x01, 0x07, 0x02, "1080p"              }, \
	{dipval, 0x01, 0x07, 0x03, "1440p"              }, \
	{dipval, 0x01, 0x07, 0x04, "2160p"              }, \
	{0     , 0xfd, 0   , 2   , "Hi-res 2D scaling"  }, \
	{dipval, 0x82, 0x08, 0x00, "Sharp (filtered)"   }, \
	{dipval, 0x00, 0x07, 0x00, NULL                 }, \
	{dipval, 0x82, 0x08, 0x08, "Nearest"            }, \
	{dipval, 0x00, 0x07, 0x00, NULL                 },

static struct BurnDIPInfo VrDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0xff, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	RESOLUTION_DIPS(0x03)

	{0   , 0xfd, 0   , 2   , "Course select steering"	},
	{0x03, 0x01, 0x10, 0x00, "Stepped (pad)"	},
	{0x03, 0x01, 0x10, 0x10, "Absolute (wheel)"	},

	{0   , 0xfd, 0   , 3   , "Rumble"			},
	{0x03, 0x01, 0x60, 0x00, "Strong effects only"	},
	{0x03, 0x01, 0x60, 0x20, "All (road vibration)"	},
	{0x03, 0x01, 0x60, 0x40, "Off"			},
};

STDDIPINFO(Vr)

static struct BurnDIPInfo VformulaDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0xff, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	RESOLUTION_DIPS(0x03)

	{0   , 0xfd, 0   , 3   , "Rumble"			},
	{0x03, 0x01, 0x60, 0x00, "Strong effects only"	},
	{0x03, 0x01, 0x60, 0x20, "All (road vibration)"	},
	{0x03, 0x01, 0x60, 0x40, "Off"			},
};

STDDIPINFO(Vformula)

static struct BurnInputInfo VfInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	{"P1 Up",				BIT_DIGITAL,	DrvJoy2 + 5,	"p1 up"		},
	{"P1 Down",				BIT_DIGITAL,	DrvJoy2 + 4,	"p1 down"	},
	{"P1 Left",				BIT_DIGITAL,	DrvJoy2 + 7,	"p1 left"	},
	{"P1 Right",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 right"	},
	{"P1 Button 1",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Button 2",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",			BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	{"P2 Up",				BIT_DIGITAL,	DrvJoy3 + 5,	"p2 up"		},
	{"P2 Down",				BIT_DIGITAL,	DrvJoy3 + 4,	"p2 down"	},
	{"P2 Left",				BIT_DIGITAL,	DrvJoy3 + 7,	"p2 left"	},
	{"P2 Right",			BIT_DIGITAL,	DrvJoy3 + 6,	"p2 right"	},
	{"P2 Button 1",			BIT_DIGITAL,	DrvJoy3 + 0,	"p2 fire 1"	},
	{"P2 Button 2",			BIT_DIGITAL,	DrvJoy3 + 1,	"p2 fire 2"	},
	{"P2 Button 3",			BIT_DIGITAL,	DrvJoy3 + 2,	"p2 fire 3"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Vf)

static struct BurnDIPInfo VfDIPList[]=
{
	DIP_OFFSET(0x15)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0xff, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	RESOLUTION_DIPS(0x03)
};

STDDIPINFO(Vf)

static struct BurnInputInfo SwaInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	A("P1 Stick X",			BIT_ANALOG_REL,	&DrvAnalogPort0,"p1 x-axis"	),
	A("P1 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogPort1,"p1 y-axis"	),
	A("P1 Throttle",		BIT_ANALOG_REL,	&DrvAnalogPort2,"p1 z-axis"	),
	{"P1 Fire",				BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Thumb",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 Button 3",			BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 3"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},
	{"P2 Start",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 start"	},
	A("P2 Stick X",			BIT_ANALOG_REL,	&DrvAnalogPort3,"p2 x-axis"	),
	A("P2 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogPort4,"p2 y-axis"	),
	{"P2 Fire",				BIT_DIGITAL,	DrvJoy2 + 2,	"p2 fire 1"	},
	{"P2 Thumb",			BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Swa)

static struct BurnDIPInfo SwaDIPList[]=
{
	DIP_OFFSET(0x11)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0xff, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	RESOLUTION_DIPS(0x03)
};

STDDIPINFO(Swa)

static struct BurnInputInfo WingwarInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	A("P1 Stick X",			BIT_ANALOG_REL,	&DrvAnalogPort0,"p1 x-axis"	),
	A("P1 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogPort1,"p1 y-axis"	),
	A("P1 Throttle",		BIT_ANALOG_REL,	&DrvAnalogPort2,"p1 z-axis"	),
	{"P1 Machine Gun",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Missile",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Smoke",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},
	{"P1 View 1",			BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 4"	},
	{"P1 View 2",			BIT_DIGITAL,	DrvJoy1 + 6,	"p1 fire 5"	},
	{"P1 View 3",			BIT_DIGITAL,	DrvJoy1 + 7,	"p1 fire 6"	},
	{"P1 View 4",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 7"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Wingwar)

static struct BurnDIPInfo WingwarDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0xff, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	RESOLUTION_DIPS(0x03)
};

STDDIPINFO(Wingwar)

static struct BurnInputInfo Wingwar360InputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	{"P1 Start",			BIT_DIGITAL,	DrvJoy1 + 4,	"p1 start"	},
	A("P1 Stick X",			BIT_ANALOG_REL,	&DrvAnalogPort0,"p1 x-axis"	),
	A("P1 Stick Y",			BIT_ANALOG_REL,	&DrvAnalogPort1,"p1 y-axis"	),
	A("P1 Throttle",		BIT_ANALOG_REL,	&DrvAnalogPort2,"p1 z-axis"	),
	{"P1 Machine Gun",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Missile",			BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Smoke",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",				BIT_DIGITAL,	DrvJoy1 + 1,	"p2 coin"	},

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Wingwar360)

static struct BurnDIPInfo Wingwar360DIPList[]=
{
	DIP_OFFSET(0x0c)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0xff, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	RESOLUTION_DIPS(0x03)
};

STDDIPINFO(Wingwar360)

static struct BurnInputInfo NetmercInputList[] = {
	{"P1 Coin",				BIT_DIGITAL,	DrvJoy1 + 0,	"p1 coin"	},
	A("P1 Gun X",			BIT_ANALOG_REL,	&DrvAnalogPort0,"p1 x-axis"	),
	A("P1 Gun Y",			BIT_ANALOG_REL,	&DrvAnalogPort1,"p1 y-axis"	),
	{"P1 Trigger",			BIT_DIGITAL,	DrvJoy2 + 0,	"p1 fire 1"	},
	{"P1 Thumb",			BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 2"	},
	{"P1 MVD Holder",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 fire 3"	},
	A("P1 Head Yaw",		BIT_ANALOG_REL,	&DrvAnalogPort4,"p2 x-axis"	),
	A("P1 Head Pitch",		BIT_ANALOG_REL,	&DrvAnalogPort5,"p2 y-axis"	),

	{"Reset",				BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",				BIT_DIGITAL,	DrvJoy1 + 3,	"service"	},
	{"Service Mode",		BIT_DIGITAL,	DrvJoy1 + 2,	"diag"		},
	{"Dip A",				BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",				BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
	{"Dip C",				BIT_DIPSWITCH,	DrvDips + 2,	"dip"		},
	{"Dip D",				BIT_DIPSWITCH,	DrvDips + 3,	"dip"		},
};

STDINPUTINFO(Netmerc)

static struct BurnDIPInfo NetmercDIPList[]=
{
	DIP_OFFSET(0x0b)
	{0x00, 0xff, 0xff, 0xff, NULL			},
	{0x01, 0xff, 0xff, 0xff, NULL			},
	{0x02, 0xff, 0xff, 0xff, NULL			},
	{0x03, 0xff, 0xff, 0x00, NULL			},

	RESOLUTION_DIPS(0x03)
};

STDDIPINFO(Netmerc)

static inline float u2f(UINT32 v)
{
	union { UINT32 u; float f; } c;
	c.u = v;
	return c.f;
}

static inline UINT8 pal5bit(UINT8 bits)
{
	bits &= 0x1f;
	return (bits << 3) | (bits >> 2);
}

// -----------------------------------------------------------------------------
// interrupts / timers (main board)

static void irq_raise(INT32 level)
{
	irq_status |= 1 << level;
	v60_irq_dirty = 1;
}

static void update_v60_irq()
{
	v60SetIRQLine(0, irq_status ? CPU_IRQSTATUS_ACK : CPU_IRQSTATUS_NONE);
	v60_irq_dirty = 0;
}

static int v60_irq_callback(int)
{
	for (INT32 i = 0; i < 8; i++) {
		if (irq_status & (1 << i)) {
			last_irq = i;
			break;
		}
	}

	return last_irq;
}

static void irq_control_w(UINT8 data)
{
	switch (data) {
		case 0x10:
			irq_status = 0;
			v60SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;

		case 0x20:
			irq_status &= ~(1 << last_irq);
			if (irq_status == 0)
				v60SetIRQLine(0, CPU_IRQSTATUS_NONE);
			break;
	}
}


static void sound_ready_check()
{
	if ((M1UsartTxRdy(0) || M1UsartRxRdy(0)) && !(irq_mask & 0x08)) {
		irq_raise(3);
	}
}

static void irq_mask_w(UINT8 data)
{
	irq_mask = data;
	sound_ready_check();

	// can't take an interrupt in the middle of an instruction
	if (v60_irq_dirty) v60RunEnd();
}

static void timer_period_w(INT32 offset, UINT16 data)
{
	timer_period[offset] = data;
	timer_remaining[offset] = data ? (0x800 * data) : 0;
}

static UINT16 timer_r(INT32 offset)
{
	if (timer_period[offset]) {
		timer_val[offset] = timer_remaining[offset] / 0x800;
	}

	return timer_val[offset];
}

static void timers_update(INT32 cycles)
{
	for (INT32 i = 0; i < 2; i++) {
		if (timer_period[i] == 0) continue;

		timer_remaining[i] -= cycles;
		while (timer_remaining[i] <= 0) {
			if (!(irq_mask & 0x01)) irq_raise(0);
			timer_remaining[i] += 0x800 * timer_period[i];
		}
	}
}

static void bankswitch(INT32 data)
{
	if ((data & 0xf) == 0x1) {
		// 100000-1fffff data roms banking, netmerc sets bit 7 too
		rom_bank = (data >> 4) & 7;
		v60MapMemory(DrvV60ROM + 0x1000000 + 0x100000 * rom_bank, 0x100000, 0x1fffff, MAP_ROM);
	}
}

// -----------------------------------------------------------------------------
// coprocessor (TGP) and its fifos

static inline INT32 fifo_empty(copro_fifo *f) { return f->count == 0; }

static inline void fifo_push(copro_fifo *f, UINT32 v)
{
	if (f->count >= FIFO_BUF) return;	// never happens with the known programs
	f->data[(f->rd + f->count) % FIFO_BUF] = v;
	f->count++;
}

static inline UINT32 fifo_pop(copro_fifo *f)
{
	if (f->count == 0) return 0;
	UINT32 v = f->data[f->rd];
	f->rd = (f->rd + 1) % FIFO_BUF;
	f->count--;
	return v;
}

// run the tgp up to the current v60 time (it runs lazily, behind the v60)
static void tgp_sync()
{
	INT32 target = (INT32)(((INT64)v60TotalCycles() * 5) / 6);
	INT32 todo = target - mb86233TotalCycles();
	if (todo > 0) mb86233Run(todo);
}

// the v60 waits on the tgp: the tgp runs until it pushes or pops a word,
// then the v60 spends the exact time the tgp took
static void tgp_wait(INT32 for_output)
{
	tgp_sync();

	INT32 waited = 0;

	while (waited < 200000) {
		if (for_output) {
			if (!fifo_empty(&fifo_out)) break;
			if (tgp_wait_in) break;		// both waiting, would never end
		} else {
			if (fifo_in.count < FIFO_SIZE) break;
			if (tgp_wait_out) break;
		}

		tgp_v60_waiting = for_output ? 1 : 2;
		mb86233Run(400);
		tgp_v60_waiting = 0;

		INT32 behind = (INT32)(((INT64)mb86233TotalCycles() * 6 + 4) / 5) - v60TotalCycles();
		if (behind > 0) {
			v60BurnCycles(behind);
			waited += behind;
		}
	}
}

static UINT32 tgp_fifo_in_read()
{
	if (fifo_empty(&fifo_in)) {
		mb86233Stall();
		mb86233SetHalt(1);
		tgp_wait_in = 1;
		return 0;
	}

	if (tgp_v60_waiting == 2) mb86233RunEnd();

	return fifo_pop(&fifo_in);
}

static void tgp_fifo_out_write(UINT32 data)
{
	fifo_push(&fifo_out, data);

	if (tgp_v60_waiting == 1) mb86233RunEnd();

	if (fifo_out.count >= FIFO_SIZE) {
		mb86233SetHalt(1);
		tgp_wait_out = 1;
	}
}

static UINT32 v60_fifo_pop()
{
	if (fifo_empty(&fifo_out)) tgp_wait(1);

	UINT32 v = fifo_pop(&fifo_out);

	if (tgp_wait_out && fifo_out.count < FIFO_SIZE) {
		tgp_sync();
		tgp_wait_out = 0;
		mb86233SetHalt(0);
	}

	return v;
}

static void v60_fifo_push(UINT32 data)
{
	tgp_sync();

	if (fifo_in.count >= FIFO_SIZE) tgp_wait(0);

	fifo_push(&fifo_in, data);

	if (tgp_wait_in) {
		tgp_wait_in = 0;
		mb86233SetHalt(0);
	}
}

static UINT16 v60_copro_read(UINT32 address)
{
	switch (address & 0xfe0000) {
		case 0xd00000:
			return v60_copro_ram_adr;

		case 0xd20000: {
			tgp_sync();
			UINT32 v = DrvCoproRAM[v60_copro_ram_adr & 0x1fff];
			if ((address & 2) == 0) return v & 0xffff;
			if (v60_copro_ram_adr & 0x8000) v60_copro_ram_adr++;
			return v >> 16;
		}

		case 0xd80000:
			if ((address & 2) == 0) {
				v60_copro_fifo_r = v60_fifo_pop();
				return v60_copro_fifo_r & 0xffff;
			}
			return v60_copro_fifo_r >> 16;

		case 0xdc0000:
			return 0xffff;
	}

	return 0;
}

static void v60_copro_write(UINT32 address, UINT16 data)
{
	switch (address & 0xfe0000) {
		case 0xd00000:
			v60_copro_ram_adr = data;
			return;

		case 0xd20000:
			v60_copro_ram_latch[(address >> 1) & 1] = data;
			if (address & 2) {
				tgp_sync();
				DrvCoproRAM[v60_copro_ram_adr & 0x1fff] = v60_copro_ram_latch[0] | (v60_copro_ram_latch[1] << 16);
				if (v60_copro_ram_adr & 0x8000) v60_copro_ram_adr++;
			}
			return;

		case 0xd80000:
			if (address & 2) {
				v60_copro_fifo_w = (v60_copro_fifo_w & 0x0000ffff) | (data << 16);
				v60_fifo_push(v60_copro_fifo_w);
			} else {
				v60_copro_fifo_w = (v60_copro_fifo_w & 0xffff0000) | data;
			}
			return;
	}
}

// tgp memory spaces

static UINT32 tgp_data_read(UINT32 address)
{
	if (address == 0x100) return tgp_fifo_in_read();
	return 0;
}

static void tgp_data_write(UINT32 address, UINT32 data)
{
	if (address == 0x400) tgp_fifo_out_write(data);
}

static UINT32 copro_sincos_r(INT32 offset)
{
	UINT32 ang = copro_sincos_base + offset * 0x4000;
	UINT32 index = ang & 0x3fff;
	if (ang & 0x4000) {
		index = 0x4000 - index;
		if (index > 0x3fff) index = 0x3fff;
	}
	UINT32 result = DrvCoproTables[index];
	if (ang & 0x8000) result ^= 0x80000000;
	return result;
}

static UINT32 copro_inv_r(INT32 offset)
{
	UINT32 index = ((copro_inv_base >> 9) & 0x3ffe) | (offset & 1);
	UINT32 result = DrvCoproTables[index | 0x8000];
	UINT8 bexp = (copro_inv_base >> 23) & 0xff;
	UINT8 exp = (result >> 23) + (0x7f - bexp);
	result = (result & 0x807fffff) | (exp << 23);
	if (copro_inv_base & 0x80000000) result ^= 0x80000000;
	return result;
}

static UINT32 copro_isqrt_r(INT32 offset)
{
	UINT32 index = 0x2000 ^ (((copro_isqrt_base >> 10) & 0x3ffe) | (offset & 1));
	UINT32 result = DrvCoproTables[index | 0xc000];
	UINT8 bexp = (copro_isqrt_base >> 24) & 0x7f;
	UINT8 exp = (result >> 23) + (0x3f - bexp);
	result = (result & 0x807fffff) | (exp << 23);
	if (!(offset & 1)) result &= 0x7fffffff;
	return result;
}

static UINT32 copro_atan_r()
{
	UINT32 idx = copro_atan_base[3] & 0xffff;
	if (idx & 0xc000) idx = 0x3fff;
	UINT32 result = DrvCoproTables[idx | 0x4000];

	// correct for a table bug, the hardware seems to do something equivalent
	UINT16 dt = (result >> 16) + result;
	if (dt & 0x001) {
		if ((result & 0x00f) == 0x00e) result -= 0x00000001;
		else result -= 0x00010000;
	}
	if (dt & 0x010) {
		if ((result & 0x0f0) == 0x0e0) result -= 0x00000010;
		else result -= 0x00100000;
	}
	if (dt & 0x100) {
		if ((result & 0xf00) == 0xe00) result -= 0x00000100;
		else result -= 0x01000000;
	}

	INT32 s0 = (copro_atan_base[0] & 0x80000000) ? 1 : 0;
	INT32 s1 = (copro_atan_base[1] & 0x80000000) ? 1 : 0;
	INT32 s2 = (copro_atan_base[2] & 0x80000000) ? 1 : 0;

	if (s0 ^ s1 ^ s2) result >>= 16;
	if (s2) result += 0x4000;
	if ((s0 && !s2) || (s1 && s2)) result += 0x8000;

	return result & 0xffff;
}

static UINT32 tgp_io_read(UINT32 address)
{
	if (address >= 0x8000) {
		UINT32 index = (copro_data_base & ~0x7fff) | (address - 0x8000);
		return DrvCoproData[index & nCoproDataMask];
	}

	switch (address) {
		case 0x00: case 0x08: case 0x10: case 0x18:
			return copro_ram_adr[address >> 3];

		case 0x01: case 0x09: case 0x11: case 0x19: {
			UINT32 &adr = copro_ram_adr[address >> 3];
			UINT32 val = DrvCoproRAM[adr & 0x1fff];
			adr += (adr & 0x40000) ? 4 : 1;
			return val;
		}

		case 0x20: case 0x21: case 0x22: case 0x23:
			return copro_sincos_r(address & 3);

		case 0x24: case 0x25: case 0x26: case 0x27:
			return copro_atan_r();

		case 0x28: case 0x29:
			return copro_inv_r(address & 1);

		case 0x2a: case 0x2b:
			return copro_isqrt_r(address & 1);
	}

	return 0;
}

static void tgp_io_write(UINT32 address, UINT32 data)
{
	switch (address) {
		case 0x00: case 0x08: case 0x10: case 0x18:
			copro_ram_adr[address >> 3] = data;
			return;

		case 0x01: case 0x09: case 0x11: case 0x19: {
			// vertices are 4 dwords (x, y, z, radius), page 4 walks them
			UINT32 &adr = copro_ram_adr[address >> 3];
			DrvCoproRAM[adr & 0x1fff] = data;
			adr += (adr & 0x40000) ? 4 : 1;
			return;
		}

		case 0x20: case 0x21: case 0x22: case 0x23:
			copro_sincos_base = data;
			return;

		case 0x24: case 0x25: case 0x26: case 0x27:
			copro_atan_base[address & 3] = data;
			return;

		case 0x28: case 0x29:
			copro_inv_base = data;
			return;

		case 0x2a: case 0x2b:
			copro_isqrt_base = data;
			return;

		case 0x2e:
			copro_data_base = data;
			return;
	}
}

static void copro_reset()
{
	v60_copro_ram_adr = 0;
	memset(copro_ram_adr, 0, sizeof(copro_ram_adr));
	copro_sincos_base = 0;
	copro_inv_base = 0;
	copro_isqrt_base = 0;
	memset(copro_atan_base, 0, sizeof(copro_atan_base));
	copro_data_base = 0;
	memset(v60_copro_ram_latch, 0, sizeof(v60_copro_ram_latch));
	v60_copro_fifo_r = 0;
	v60_copro_fifo_w = 0;
	memset(DrvCoproRAM, 0, 0x2000 * 4);

	memset(&fifo_in, 0, sizeof(fifo_in));
	memset(&fifo_out, 0, sizeof(fifo_out));
	tgp_wait_in = 0;
	tgp_wait_out = 0;
	tgp_lead = 0;
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// main cpu memory handlers

static void palette_update_entry(INT32 offset)
{
	UINT16 v = BURN_ENDIAN_SWAP_INT16(((UINT16*)DrvPalRAM)[offset]);

	INT32 r = pal5bit(v >> 0);
	INT32 g = pal5bit(v >> 5);
	INT32 b = pal5bit(v >> 10);

	// bit 15 clear: dimmed entry
	if (!(v & 0x8000)) {
		r >>= 1;
		g >>= 1;
		b >>= 1;
	}

	DrvPalette[offset] = BurnHighCol(r, g, b, 0);
	DrvPalRGB[offset] = (r << 16) | (g << 8) | b;
}

// commands the game sends to the wheel's drive board through dpram 0x11 (sega drive board protocol)
static void drive_board_command(UINT8 data)
{
	switch (data >> 4) {
		case 0x3:	// uncentering, the road vibration (0 = off)
			drive_vibrate = data & 0xf;
			break;

		case 0x4:	// power slide sequence: when the car leaves the road
			if (data & 0xf) drive_slide = 12;
			break;

		case 0x5:	// rotate the wheel right / left: the wheel's pull while cornering off the road
		case 0x6:
			if (data & 0xf) drive_turn = 6;
			break;
	}
}

// The course select (game mode 0x10) reads the wheel's absolute position: picking a course that way
// needs a precision a self-centering stick doesn't have (vrext: ten bands of about 15 steps). There,
// each push of the stick moves to the next / previous course and the wheel stays on it.
// vrext's direction and weather screens (its phase byte 0x502295 != 0) only need left / right.
static void menu_wheel_update()
{
	static const UINT8 vr_wheel[3] = { 128, 168, 196 };	// vr: course 1 from about 151, 2 from about 186
	static const UINT8 vrext_wheel[10] = { 61, 76, 91, 106, 120, 135, 150, 165, 180, 195 };	// vrext's band centres

	if ((DrvDips[3] & 0x10) || DrvMainRAM[0x1139] != 0x10 || DrvMainRAM[0x2295] != 0) {
		menu_wheel_on = 0;
		return;
	}

	const UINT8 *wheel = (bHasMenuWheel == 2) ? vrext_wheel : vr_wheel;
	INT32 count = (bHasMenuWheel == 2) ? 10 : 3;

	INT32 d = analog_values[0] - 0x80;
	INT32 dir = (d > 0x40) ? 1 : ((d < -0x40) ? -1 : 0);

	if (!menu_wheel_on) {
		menu_wheel_on = 1;
		menu_band = (bHasMenuWheel == 2) ? 5 : 0;	// the course of a centred wheel
		menu_dir = dir;		// a stick still held from before doesn't step
		menu_repeat = 0;
	}

	if (dir == 0) {
		menu_dir = 0;
	} else if (dir != menu_dir) {
		menu_dir = dir;
		menu_repeat = 20;
		if ((dir < 0 && menu_band > 0) || (dir > 0 && menu_band < count - 1)) menu_band += dir;
	} else if (--menu_repeat == 0) {
		menu_repeat = 8;
		if ((dir < 0 && menu_band > 0) || (dir > 0 && menu_band < count - 1)) menu_band += dir;
	}

	analog_values[0] = wheel[menu_band];
}

// "Rumble" dip: 0 strong effects only, 1 all of them, 2 off
// Virtua Racing sends nothing to the drive board when the car hits a wall or another car, so it's
// seen in the player car's state (work ram 0x400d00-):
//  0xd14: speed (km/h, updated every other frame): a car slowing down with the gas on and the brake off
//   is scraping a wall, bumping into a car or on the grass, one stopping dead (60 -> 0) hit a wall head on
//  0xd56: countdown of the crash where the car flies off (0 otherwise)
//  0xe05: 0xff while the car spins out
static void drive_board_rumble()
{
	INT32 mode = (DrvDips[3] >> 5) & 3;
	INT32 racing = bHasMenuWheel && DrvMainRAM[0x1139] == 0x14;

	if (bHasMenuWheel) {
		UINT16 speed = DrvNVRAM[0xd14] | (DrvNVRAM[0xd15] << 8);
		INT32 drop = (drive_speed[0] < 0x200) ? (drive_speed[0] - speed) : 0;
		INT32 gas = analog_values[1] >= 0xa0 && analog_values[2] < 0x60;

		INT32 airborne = DrvNVRAM[0xd56] != 0;

		if (racing && drop >= 8) drive_crash = 15;
		if (racing && airborne && !drive_airborne) drive_crash = 20;
		if (racing && (airborne || DrvNVRAM[0xe05] == 0xff)) drive_contact = 8;
		drive_airborne = airborne;

		if (racing && gas && drop >= 3) {
			if (drive_slowing < 0xff) drive_slowing++;
			if (drive_slowing >= 4) drive_contact = 8;	// a gear change is shorter
		} else {
			drive_slowing = 0;
		}

		drive_speed[0] = drive_speed[1];
		drive_speed[1] = speed;
	}

	INT32 level = drive_vibrate;
	INT32 slide = drive_slide;
	INT32 turn = drive_turn;
	INT32 crash = drive_crash;
	INT32 contact = drive_contact;

	if (drive_slide) drive_slide--;
	if (drive_turn) drive_turn--;
	if (drive_crash) drive_crash--;
	if (drive_contact) drive_contact--;

	if (mode == 0) {
		if (level < 6) level = 0;	// 1-5: the road surface, growing with the speed
		turn = 0;
		if (bHasMenuWheel && !racing) level = slide = crash = contact = 0;	// virtua racing: outside a race (attract)
	} else if (mode >= 2) {
		level = slide = turn = crash = contact = 0;
	}

	INT32 strong = level * 0x0600;
	INT32 weak = level * 0x1000;

	if (turn) {
		strong = (strong > 0x4000) ? strong : 0x4000;
		weak = (weak > 0x6000) ? weak : 0x6000;
	}
	if (contact) {
		strong = (strong > 0x9000) ? strong : 0x9000;
		weak = (weak > 0xc000) ? weak : 0xc000;
	}
	if (slide) {
		strong = 0xa000;
		weak = 0x8000;
	}
	if (crash) {
		strong = 0xffff;
		weak = 0xffff;
	}

	BurnRumble(0, strong, weak);
}

// the tilemaps are drawn as the beam reaches the lines: the games write them in the vblank
static INT32 tile_lines_done;	// lines of TileBack/TileHud drawn this frame
static INT32 tile_skip;			// the frame isn't shown: its tilemaps aren't drawn

static void tile_draw_lines(INT32 y1)
{
	INT32 y0 = tile_lines_done;
	if (y1 > NATIVE_H) y1 = NATIVE_H;
	if (y1 <= y0 || tile_skip) return;

	sega_315_5292_update();

	memset(TileBack + y0 * NATIVE_W, 0, (y1 - y0) * NATIVE_W * sizeof(UINT16));	// pen 0
	if (nBurnLayer & 1) {
		sega_315_5292_draw_layer_lines(6, 1, TileBack, y0, y1);
		sega_315_5292_draw_layer_lines(4, 1, TileBack, y0, y1);
		sega_315_5292_draw_layer_lines(2, 0, TileBack, y0, y1);
		sega_315_5292_draw_layer_lines(0, 0, TileBack, y0, y1);
	}

	memset(TileHud + y0 * NATIVE_W, 0xff, (y1 - y0) * NATIVE_W * sizeof(UINT16));
	if (nBurnLayer & 4) {
		sega_315_5292_draw_layer_lines(7, 0, TileHud, y0, y1);
		sega_315_5292_draw_layer_lines(5, 0, TileHud, y0, y1);
		sega_315_5292_draw_layer_lines(3, 0, TileHud, y0, y1);
		sega_315_5292_draw_layer_lines(1, 0, TileHud, y0, y1);
	}

	tile_lines_done = y1;
}

// before a write to the tile or char ram: the lines the beam has passed use the old data
static void tile_flush()
{
	if (tile_lines_done < NATIVE_H) {
		INT32 cyc = v60TotalCycles();
		if (cyc > 0) tile_draw_lines((INT32)(((INT64)cyc * VTOTAL) / V60_CYCLES_FRAME));
	}
}

static void model1_write_word(UINT32 address, UINT16 data)
{
	address &= 0xfffffe;

	if ((address & 0xff0000) == 0x700000) {
		UINT16 *p = (UINT16*)(DrvTileRAM + (address & 0xfffe));
		if (BURN_ENDIAN_SWAP_INT16(*p) != data) {
			tile_flush();
			*p = BURN_ENDIAN_SWAP_INT16(data);
		}
		return;
	}

	if ((address & 0xf80000) == 0x780000) {
		UINT16 *p = (UINT16*)(DrvCharRAM + (address & 0x7fffe));
		if (BURN_ENDIAN_SWAP_INT16(*p) != data) {
			tile_flush();
			*p = BURN_ENDIAN_SWAP_INT16(data);
			sega_315_5292_char_written(address & 0x7fffe);
		}
		return;
	}

	if ((address & 0xffc000) == 0x900000) {
		((UINT16*)DrvPalRAM)[(address & 0x3fff) / 2] = BURN_ENDIAN_SWAP_INT16(data);
		palette_update_entry((address & 0x3fff) / 2);
		return;
	}

	if ((address & 0xfff000) == 0xc00000) {
		if (bHasDriveBoard && (address & 0xffe) == 0x022) drive_board_command(data & 0xff);
		DrvDPRAM[(address >> 1) & 0x7ff] = data & 0xff;
		return;
	}

	if (address >= 0xd00000 && address <= 0xdfffff) {
		v60_copro_write(address, data);
		return;
	}

	switch (address) {
		case 0x680000:
		case 0x680002:
			listctl[(address >> 1) & 1] = data;
			return;

		case 0x720000:
		case 0x740000:
		case 0x760000:
		case 0x770000:
			return;

		case 0xb01000:
			comm_cn = data & 1;
			if (!comm_cn) comm_fg = 0;
			return;

		case 0xb01002:
			if (comm_cn) comm_fg = data & 1;
			return;

		case 0xc40000:
			M1UsartDataWrite(0, data & 0xff);
			return;

		case 0xc40002:
			M1UsartControlWrite(0, data & 0xff);
			return;

		case 0xe00000:
			irq_control_w(data & 0xff);
			return;

		case 0xe00002:
			irq_mask_w(data & 0xff);
			return;

		case 0xe00004:
			bankswitch(data);
			return;

		case 0xe00006:
			timer_mode = data;
			return;

		case 0xe00008:
		case 0xe0000a:
			timer_period_w((address >> 1) & 1, data);
			return;

		case 0xe0000c:
		case 0xe0000e:
			return;
	}
}

static void model1_write_byte(UINT32 address, UINT8 data)
{
	address &= 0xffffff;

	if ((address & 0xff0000) == 0x700000) {
		if (DrvTileRAM[address & 0xffff] != data) {
			tile_flush();
			DrvTileRAM[address & 0xffff] = data;
		}
		return;
	}

	if ((address & 0xf80000) == 0x780000) {
		if (DrvCharRAM[address & 0x7ffff] != data) {
			tile_flush();
			DrvCharRAM[address & 0x7ffff] = data;
			sega_315_5292_char_written(address & 0x7fffe);
		}
		return;
	}

	if ((address & 0xffc000) == 0x900000) {
		DrvPalRAM[address & 0x3fff] = data;
		palette_update_entry((address & 0x3fff) / 2);
		return;
	}

	if ((address & 0xfff000) == 0xc00000) {
		if (bHasDriveBoard && (address & 0xfff) == 0x022) drive_board_command(data);
		if ((address & 1) == 0) DrvDPRAM[(address >> 1) & 0x7ff] = data;
		return;
	}

	switch (address) {
		case 0xb01000:
		case 0xb01002:
		case 0xc40000:
		case 0xc40002:
		case 0xe00000:
		case 0xe00002:
			model1_write_word(address, data);
			return;
	}

	if (address >= 0xd00000 && address <= 0xdfffff) {
		// the copro registers are 16-bit
		model1_write_word(address & ~1, (address & 1) ? (data << 8) : data);
		return;
	}

	if ((address & 0xfffff0) == 0xe00000 || (address & 0xfffffc) == 0x680000) {
		model1_write_word(address & ~1, data);
		return;
	}
}

static UINT16 model1_read_word(UINT32 address)
{
	address &= 0xfffffe;

	if ((address & 0xfff000) == 0xc00000) {
		v60BurnCycles(1);	// wait state
		return DrvDPRAM[(address >> 1) & 0x7ff];
	}

	if (address >= 0xd00000 && address <= 0xdfffff) {
		return v60_copro_read(address);
	}

	switch (address) {
		case 0x680000:
			return listctl[0] | 0x30;

		case 0x680002:
			return listctl[1];

		case 0xb01000:
			return bHasComm ? (comm_cn | 0xfe) : 0;

		case 0xb01002:
			return bHasComm ? (comm_fg | 0x80 | 0x7e) : 0;	// the comm cpu never sets zfg

		case 0xc40000:
		case 0xc40002: {
			UINT8 r = M1UsartRead(0, (address >> 1) & 1);
			return r;
		}

		case 0xe00002:
			return irq_mask;

		case 0xe0000c:
		case 0xe0000e:
			return timer_r((address >> 1) & 1);
	}

	return 0;
}

static UINT8 model1_read_byte(UINT32 address)
{
	address &= 0xffffff;

	if ((address & 0xfff000) == 0xc00000) {
		v60BurnCycles(1);	// wait state
		return (address & 1) ? 0 : DrvDPRAM[(address >> 1) & 0x7ff];
	}

	UINT16 w = model1_read_word(address & ~1);
	return (address & 1) ? (w >> 8) : (w & 0xff);
}

// the v60 i/o space only has the coprocessor
static UINT16 model1_io_read_word(UINT32 address)
{
	address &= 0xfffffe;
	if (address >= 0xd00000 && address <= 0xdfffff) return v60_copro_read(address);
	return 0;
}

static UINT8 model1_io_read_byte(UINT32 address)
{
	UINT16 w = model1_io_read_word(address & ~1);
	return (address & 1) ? (w >> 8) : (w & 0xff);
}

static void model1_io_write_word(UINT32 address, UINT16 data)
{
	address &= 0xfffffe;
	if (address >= 0xd00000 && address <= 0xdfffff) v60_copro_write(address, data);
}

static void model1_io_write_byte(UINT32 address, UINT8 data)
{
	model1_io_write_word(address & ~1, (address & 1) ? (data << 8) : data);
}

// -----------------------------------------------------------------------------
// i/o board hooks (wing war r360: the feedback board is on the lcd port)

static void r360_w(UINT8 data)
{
	// feedback board protocol, command group b are the game states
	switch (data) {
		case 0xbf: case 0xbe: case 0xba: case 0xb9:
			r360_state = ~0x40;		// default status
			break;
		case 0xbd: r360_state = ~0x44; break;	// lever up
		case 0xbc: r360_state = ~0x45; break;	// emergency button pushed
		case 0xbb: r360_state = ~0x46; break;	// game start
		case 0xaf: r360_state = ~r360_throttle; break;
	}
}

static INT32 io_port_in_hook(INT32 port)
{
	if (bIOBoard2 && bHasR360 && port == 2) return r360_state;
	return -1;
}

static void io_port_out_hook(INT32 port, UINT8 data)
{
	if (bIOBoard2 && bHasR360 && port == 4) r360_w(data);
}

// -----------------------------------------------------------------------------
// sound board

// -----------------------------------------------------------------------------
// 3d rendering (the geometrizer and rasterizer are simulated)

#define FRAC_SHIFT	16

struct m1_point {
	float x, y, z;
	float xx, yy;
	INT32 sx, sy;
	float fx, fy;		// unrounded screen position, for the high resolution modes
};

struct m1_quad {			// while building
	m1_point *p[4];
	float z;
	INT32 col;			// palette index, -1 = none
	INT32 moire;
};

struct m1_quad_out {		// what's drawn
	INT32 sx[4], sy[4];
	float fx[4], fy[4];
	float z;
	UINT16 col;
	UINT8 moire;
	UINT32 order;
};

struct m1_quad_segment {
	INT32 start, count;
	INT32 x1, x2, y1, y2;	// viewport
};

struct m1_lightparam {
	float a, d, s;
	INT32 p;
};

struct m1_view {
	INT32 xc, yc, x1, y1, x2, y2;
	float zoomx, zoomy, viewx, viewy;
	float a_bottom, a_top, a_left, a_right;
	float translation[12];
	float light[3];
	m1_lightparam lightparams[256];
	INT32 spec_enable;
};

static m1_view view;			// the geometrizer state, it persists between frames

#define MAX_POINTS		0x40000
#define MAX_QUADS		0x20000
#define MAX_SEGMENTS	0x400

static m1_point *pointdb;
static INT32 point_count;
static m1_quad_out *quaddb;
static INT32 quad_count;
static INT32 quad_segment_start;
static m1_quad_segment *segments[2];
static INT32 segment_count[2];

// the polygons of the last two lists: the screen shows the list of two frames before (the
// rasterizer draws a list in the frame after the flip, then the frame buffers swap). a frame's
// list is built at the start of the next one, so the state holds what the build needs
struct m1_frame3d {
	m1_quad_out *quads;
	m1_quad_segment *segs[2];
	INT32 seg_count[2];
	UINT32 serial;		// changes with the contents
};

static UINT32 frame3d_serials;

// what a build read: to build the list again after a state load (the tgp and polygon rams are
// the current ones, only lists that write them can come out differently)
struct m1_build_input {
	UINT16 *list;		// 0x8000 words
	UINT16 *pal;		// 0x2000 words, the second half is used
	UINT16 *xlat;		// 0x6000 words
	m1_view view;
	UINT16 listctl0;
	UINT32 frame;
	INT32 valid;
};

static m1_frame3d frames3d[2];
static m1_build_input builds[2];
static INT32 frame3d_new;		// the last list's frames3d[], the screen shows the other one
static INT32 frame3d_lost[2];	// to build again (state load)
static INT32 build_pending;		// a frame ended: its list is built at the start of the next one
static const m1_frame3d *draw3d;
static UINT16 *Dst3D;			// where the polygons are drawn
static UINT16 *Frame3D;			// the 3d under the hud, F3D_NONE where it doesn't draw
#define F3D_NONE		0x7d7d	// an unused pen
static INT32 render_pass;

static UINT16 *dlist_current;

enum { RENDER_BELOW_HUD = 0, RENDER_ABOVE_HUD = 1 };

static inline UINT32 readi(INT32 adr)
{
	return BURN_ENDIAN_SWAP_INT16(dlist_current[adr & 0x7fff]) | (BURN_ENDIAN_SWAP_INT16(dlist_current[(adr + 1) & 0x7fff]) << 16);
}

static inline INT16 readi16(INT32 adr)
{
	return (INT16)BURN_ENDIAN_SWAP_INT16(dlist_current[adr & 0x7fff]);
}

static inline float readf(INT32 adr)
{
	return u2f(readi(adr));
}

// vertices meant to be on a pixel edge (the bottom of the screen at 383.0) come out of the float
// projection a hair short of it, truncated they lose a line: MAME leaves the last line unfilled
#define SNAP_EPSILON	0.002f

static inline INT32 screen_coord(float v)
{
	// nan/inf and huge values (degenerate projections) must not overflow the 16.16 rasterizer
	if (!(v > -32768.0f)) return -32768;
	if (!(v < 32767.0f)) return 32767;
	return (INT32)v;
}

static void transform_point(m1_point *p)
{
	m1_point q = *p;
	float *t = view.translation;
	p->x = t[0] * q.x + t[3] * q.y + t[6] * q.z + t[ 9];
	p->y = t[1] * q.x + t[4] * q.y + t[7] * q.z + t[10];
	p->z = t[2] * q.x + t[5] * q.y + t[8] * q.z + t[11];
}

static void transform_vector(float *v)
{
	float *t = view.translation;
	float x = v[0], y = v[1], z = v[2];
	v[0] = t[0] * x + t[3] * y + t[6] * z;
	v[1] = t[1] * x + t[4] * y + t[7] * z;
	v[2] = t[2] * x + t[5] * y + t[8] * z;
}

static void normalize(float *v)
{
	float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] /= l;
	v[1] /= l;
	v[2] /= l;
}

static void project_point(m1_point *p)
{
	p->xx = p->x / p->z;
	p->yy = p->y / p->z;
	p->fx = view.xc + (p->xx * view.zoomx + view.viewx);
	p->fy = view.yc - (p->yy * view.zoomy + view.viewy);
	p->sx = screen_coord(p->fx + SNAP_EPSILON);
	p->sy = screen_coord(p->fy + SNAP_EPSILON);
}

static void project_point_direct(m1_point *p)
{
	p->xx = p->x;
	p->yy = p->y;
	p->fx = view.xc + p->xx;
	p->fy = view.yc - p->yy;
	p->sx = screen_coord(p->fx + SNAP_EPSILON);
	p->sy = screen_coord(p->fy + SNAP_EPSILON);
}

static float view_determinant(const m1_point *p1, const m1_point *p2, const m1_point *p3)
{
	float x1 = p2->x - p1->x;
	float y1 = p2->y - p1->y;
	float z1 = p2->z - p1->z;
	float x2 = p3->x - p1->x;
	float y2 = p3->y - p1->y;
	float z2 = p3->z - p1->z;

	return p1->x * (y1 * z2 - y2 * z1) + p1->y * (z1 * x2 - z2 * x1) + p1->z * (x1 * y2 - x2 * y1);
}

static void recompute_frustum()
{
	view.a_left   = ( view.x1 - view.xc - view.viewx) / view.zoomx;
	view.a_right  = ( view.x2 - view.xc - view.viewx) / view.zoomx;
	view.a_bottom = (-view.y1 + view.yc - view.viewy) / view.zoomy;
	view.a_top    = (-view.y2 + view.yc - view.viewy) / view.zoomy;
}

static INT32 fclip_isclipped(INT32 level, m1_point *p)
{
	switch (level) {
		case 0: return p->y > (p->z * view.a_bottom);
		case 1: return p->y < (p->z * view.a_top);
		case 2: return p->x < (p->z * view.a_left);
		case 3: return p->x > (p->z * view.a_right);
	}
	return 0;
}

static void fclip_clip(INT32 level, m1_point *pt, m1_point *p1, m1_point *p2)
{
	float t = 0;

	switch (level) {
		case 0: t = (p2->z * view.a_bottom - p2->y) / ((p2->z - p1->z) * view.a_bottom - (p2->y - p1->y)); break;
		case 1: t = (p2->z * view.a_top    - p2->y) / ((p2->z - p1->z) * view.a_top    - (p2->y - p1->y)); break;
		case 2: t = (p2->z * view.a_left   - p2->x) / ((p2->z - p1->z) * view.a_left   - (p2->x - p1->x)); break;
		case 3: t = (p2->z * view.a_right  - p2->x) / ((p2->z - p1->z) * view.a_right  - (p2->x - p1->x)); break;
	}

	pt->x = p1->x * t + p2->x * (1 - t);
	pt->y = p1->y * t + p2->y * (1 - t);
	pt->z = p1->z * t + p2->z * (1 - t);
	project_point(pt);

	// the new point is on the viewport edge, don't let float rounding put it one pixel inside
	// (MAME leaves the last column / line unfilled this way)
	switch (level) {
		case 0: pt->sy = view.y1; pt->fy = (float)view.y1; break;
		case 1: pt->sy = view.y2; pt->fy = view.y2 + 0.999f; break;
		case 2: pt->sx = view.x1; pt->fx = (float)view.x1; break;
		case 3: pt->sx = view.x2; pt->fx = view.x2 + 0.999f; break;
	}
}

static m1_point *new_point()
{
	if (point_count >= MAX_POINTS) {
		return &pointdb[MAX_POINTS];	// scratch entry, the frame is too complex
	}
	return &pointdb[point_count++];
}

static void store_quad(m1_quad &q)
{
	if (quad_count >= MAX_QUADS) return;

	m1_quad_out *o = &quaddb[quad_count];
	for (INT32 i = 0; i < 4; i++) {
		o->sx[i] = q.p[i]->sx;
		o->sy[i] = q.p[i]->sy;
		o->fx[i] = q.p[i]->fx;
		o->fy[i] = q.p[i]->fy;
	}
	o->z = q.z;
	o->col = q.col;
	o->moire = q.moire;
	o->order = quad_count;
	quad_count++;
}

static void fclip_push_quad(INT32 level, m1_quad &q);

static void fclip_push_quad_next(INT32 level, m1_quad &q, m1_point *p1, m1_point *p2, m1_point *p3, m1_point *p4)
{
	m1_quad cquad = q;
	cquad.p[0] = p1;
	cquad.p[1] = p2;
	cquad.p[2] = p3;
	cquad.p[3] = p4;
	fclip_push_quad(level + 1, cquad);
}

static void fclip_push_quad(INT32 level, m1_quad &q)
{
	if (level == 4) {
		store_quad(q);
		return;
	}

	INT32 is_out[4];
	for (INT32 i = 0; i < 4; i++)
		is_out[i] = fclip_isclipped(level, q.p[i]);

	// no clipping
	if (!is_out[0] && !is_out[1] && !is_out[2] && !is_out[3]) {
		fclip_push_quad(level + 1, q);
		return;
	}

	// full clipping
	if (is_out[0] && is_out[1] && is_out[2] && is_out[3])
		return;

	// find n so that point n is clipped and n-1 isn't
	INT32 i;
	for (i = 0; i < 4; i++)
		if (is_out[i] && !is_out[(i - 1) & 3])
			break;

	m1_point *pt[4];
	INT32 is_out2[4];
	for (INT32 j = 0; j < 4; j++) {
		pt[j] = q.p[(i + j) & 3];
		is_out2[j] = is_out[(i + j) & 3];
	}

	// pt[0] is clipped out and pt[3] isn't
	if (is_out2[1]) {
		if (is_out2[2]) {
			// pt 0,1,2 clipped out, one triangle left
			m1_point *pi1 = new_point(); fclip_clip(level, pi1, pt[2], pt[3]);
			m1_point *pi2 = new_point(); fclip_clip(level, pi2, pt[3], pt[0]);
			fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
		} else {
			// pt 0,1 clipped out, one quad left
			m1_point *pi1 = new_point(); fclip_clip(level, pi1, pt[1], pt[2]);
			m1_point *pi2 = new_point(); fclip_clip(level, pi2, pt[3], pt[0]);
			fclip_push_quad_next(level, q, pi1, pt[2], pt[3], pi2);
		}
	} else {
		if (is_out2[2]) {
			// pt 0,2 clipped out, shouldn't happen, two triangles
			m1_point *pi1 = new_point(); fclip_clip(level, pi1, pt[0], pt[1]);
			m1_point *pi2 = new_point(); fclip_clip(level, pi2, pt[1], pt[2]);
			fclip_push_quad_next(level, q, pi1, pt[1], pi2, pi2);
			pi1 = new_point(); fclip_clip(level, pi1, pt[2], pt[3]);
			pi2 = new_point(); fclip_clip(level, pi2, pt[3], pt[0]);
			fclip_push_quad_next(level, q, pi1, pt[3], pi2, pi2);
		} else {
			// pt 0 clipped out, split in quad + triangle
			m1_point *pi1 = new_point(); fclip_clip(level, pi1, pt[0], pt[1]);
			m1_point *pi2 = new_point(); fclip_clip(level, pi2, pt[3], pt[0]);
			fclip_push_quad_next(level, q, pi1, pt[1], pt[2], pt[3]);
			fclip_push_quad_next(level, q, pt[3], pi2, pi1, pi1);
		}
	}
}

static float min4f(float a, float b, float c, float d)
{
	float m = a;
	if (b < m) m = b;
	if (c < m) m = c;
	if (d < m) m = d;
	return m;
}

static float max4f(float a, float b, float c, float d)
{
	float m = a;
	if (b > m) m = b;
	if (c > m) m = c;
	if (d > m) m = d;
	return m;
}

static float compute_specular(float *normal, float *light, float diffuse, INT32 lmode)
{
	if (!view.spec_enable)
		return 0.0f;

	m1_lightparam *lp = &view.lightparams[lmode];
	if (lp->p == 0 || lp->s <= 0.0f)
		return 0.0f;

	// z of the reflected light vector, raised to a power of two
	float s = (2.0f * diffuse * normal[2]) - light[2];
	if (s <= 0.0f) return 0.0f;
	if (lp->p >= 2) s *= s;
	if (lp->p >= 4) s *= s;
	if (lp->p >= 7) s *= s;
	s *= lp->s;
	return (s < 1.0f) ? s : 1.0f;
}

static inline UINT16 tgp_ram_entry(UINT32 adr)
{
	adr -= 0x40000;
	return (adr < TGP_RAM_SIZE) ? DrvTGPRAM[adr] : 0;
}

static INT32 shade_color(UINT16 tex_data, INT32 lumval, INT32 blink)
{
	UINT16 *pal = (UINT16*)DrvPalRAM;
	UINT16 *xlat = (UINT16*)DrvXlatRAM;

	INT32 color = BURN_ENDIAN_SWAP_INT16(pal[0x1000 | (tex_data & 0x3ff)]);
	INT32 r = (color >> 0x0) & 0x1f;
	INT32 g = (color >> 0x5) & 0x1f;
	INT32 b = (color >> 0xa) & 0x1f;

	if (blink) {
		INT32 tmp = b;
		b = r;
		r = g;
		g = tmp;
	}

	if (lumval > 0x3f) lumval = 0x3f;
	else if (lumval < 0) lumval = 0;

	r = (BURN_ENDIAN_SWAP_INT16(xlat[(r << 8) | lumval | 0x0000]) >> 3) & 0x1f;
	g = (BURN_ENDIAN_SWAP_INT16(xlat[(g << 8) | lumval | 0x2000]) >> 3) & 0x1f;
	b = (BURN_ENDIAN_SWAP_INT16(xlat[(b << 8) | lumval | 0x4000]) >> 3) & 0x1f;

	return 0x8000 | (r << 10) | (g << 5) | b;
}

// old_z is the flat-z "reuse" register, it persists across objects
static void push_object(UINT32 tex_adr, UINT32 poly_adr, UINT32 size, float &old_z)
{
	// some games send objects with out of range addresses
	if ((tex_adr + 1 - 0x40000) > (0x100000 - 0x40000) || size >= 0x1000000)
		return;

	float *poly_data;
	UINT32 poly_limit;
	if (poly_adr & 0x800000) {
		poly_data = (float*)DrvPolyRAM;
		poly_limit = POLY_RAM_SIZE;
	} else {
		poly_data = (float*)DrvPolyROM;
		poly_limit = POLY_ROM_SIZE;
	}

	poly_adr &= 0x7fffff;
	if (poly_adr + 6 > poly_limit)
		return;

	if (!size) size = 0xffffffff;

	m1_point *old_p0 = new_point();
	m1_point *old_p1 = new_point();

	old_p0->x = poly_data[poly_adr + 0];
	old_p0->y = poly_data[poly_adr + 1];
	old_p0->z = poly_data[poly_adr + 2];
	old_p1->x = poly_data[poly_adr + 3];
	old_p1->y = poly_data[poly_adr + 4];
	old_p1->z = poly_data[poly_adr + 5];
	transform_point(old_p0);
	transform_point(old_p1);

	if (old_p0->z > 0) project_point(old_p0);
	else { old_p0->sx = old_p0->sy = 0; old_p0->fx = old_p0->fy = 0; }

	if (old_p1->z > 0) project_point(old_p1);
	else { old_p1->sx = old_p1->sy = 0; old_p1->fx = old_p1->fy = 0; }

	poly_adr += 6;

	for (UINT32 i = 0; i < size; i++) {
		if (poly_adr + 10 > poly_limit)
			break;

		UINT32 flags = BURN_ENDIAN_SWAP_INT32(((UINT32*)poly_data)[poly_adr]);

		INT32 type = flags & 3;
		if (!type)
			break;

		if (flags & 0x00001000)
			tex_adr++;

		// bit 22 selects the second light parameter bank (netmerc)
		INT32 lightmode = ((flags >> 17) & 15) | ((flags & 0x00400000) ? 0x80 : 0);

		m1_point *p0 = new_point();
		m1_point *p1 = new_point();

		float vn[3];
		vn[0] = poly_data[poly_adr + 1];
		vn[1] = poly_data[poly_adr + 2];
		vn[2] = poly_data[poly_adr + 3];
		p0->x = poly_data[poly_adr + 4];
		p0->y = poly_data[poly_adr + 5];
		p0->z = poly_data[poly_adr + 6];
		p1->x = poly_data[poly_adr + 7];
		p1->y = poly_data[poly_adr + 8];
		p1->z = poly_data[poly_adr + 9];

		if (type == 2) {
			p1->x = p0->x;
			p1->y = p0->y;
			p1->z = p0->z;
		}

		INT32 link = (flags >> 8) & 3;

		transform_vector(vn);

		transform_point(p0);
		transform_point(p1);

		if (p0->z > 0) project_point(p0);
		else { p0->sx = p0->sy = 0; p0->fx = p0->fy = 0; }

		if (p1->z > 0) project_point(p1);
		else { p1->sx = p1->sy = 0; p1->fx = p1->fy = 0; }

		if (link && ((flags & 0x00004000) || view_determinant(old_p1, old_p0, p0) <= 0)) {
			m1_quad cquad;

			normalize(vn);

			cquad.p[0] = old_p1;
			cquad.p[1] = old_p0;
			cquad.p[2] = p0;
			cquad.p[3] = p1;

			switch ((flags >> 10) & 3) {
				case 0:
					cquad.z = old_z;
					break;
				case 1:
					cquad.z = old_z = min4f(old_p1->z, old_p0->z, p0->z, p1->z);
					break;
				case 2:
					cquad.z = old_z = max4f(old_p1->z, old_p0->z, p0->z, p1->z);
					break;
				default:
					cquad.z = 0.0f;
					break;
			}

			float dif = vn[0] * view.light[0] + vn[1] * view.light[1] + vn[2] * view.light[2];
			float spec = compute_specular(vn, view.light, dif, lightmode);
			float ln = view.lightparams[lightmode].a + view.lightparams[lightmode].d * ((dif > 0.0f) ? dif : 0.0f) + spec;
			INT32 lumval = (INT32)(255.0f * ((ln < 1.0f) ? ln : 1.0f));

			// bits 11-10 of the tile word: bit 10 = flat element drawn unlit,
			// mode 01 also blinks by rotating the channels every other frame
			UINT16 tex_data = tgp_ram_entry(tex_adr);

			lumval >>= 2;
			if (tex_data & 0x400) lumval = 0x3f;

			cquad.col = shade_color(tex_data, lumval, (((tex_data >> 10) & 3) == 1) && (frame_number & 1));
			cquad.moire = (flags & 0x00002000) ? 1 : 0;

			fclip_push_quad(0, cquad);
		}

		poly_adr += 10;

		switch (link) {
			case 0:
			case 2:
				old_p0 = p0;
				old_p1 = p1;
				break;
			case 1:
				old_p1 = p0;
				break;
			case 3:
				old_p0 = p1;
				break;
		}
	}
}

static INT32 push_direct(INT32 list_offset)
{
	UINT32 tex_adr = readi(list_offset + 2);

	m1_point *old_p0 = new_point();
	m1_point *old_p1 = new_point();

	old_p0->x = readf(list_offset + 2 + 4);
	old_p0->y = readf(list_offset + 2 + 6);
	old_p0->z = readf(list_offset + 2 + 8);
	old_p1->x = readf(list_offset + 2 + 12);
	old_p1->y = readf(list_offset + 2 + 14);
	old_p1->z = readf(list_offset + 2 + 16);

	// screen space polys, z is ignored
	project_point_direct(old_p0);
	project_point_direct(old_p1);

	list_offset += 18;

	for (;;) {
		UINT32 flags = readi(list_offset + 2);

		INT32 type = flags & 3;
		if (!type)
			break;

		if (flags & 0x00001000)
			tex_adr++;

		m1_point *p0 = new_point();
		m1_point *p1 = new_point();

		UINT32 lum = readi(list_offset + 2 + 2);

		float z = 0;
		if (type == 2) {
			p0->x = readf(list_offset + 2 + 6);
			p0->y = readf(list_offset + 2 + 8);
			p0->z = readf(list_offset + 2 + 10);
			z = p0->z;
			*p1 = *p0;
			list_offset += 12;
		} else {
			p0->x = readf(list_offset + 2 + 6);
			p0->y = readf(list_offset + 2 + 8);
			p0->z = readf(list_offset + 2 + 10);
			p1->x = readf(list_offset + 2 + 14);
			p1->y = readf(list_offset + 2 + 16);
			p1->z = readf(list_offset + 2 + 18);
			z = readf(list_offset + 2 + 12);
			list_offset += 20;
		}

		INT32 link = (flags >> 8) & 3;

		project_point_direct(p0);
		project_point_direct(p1);

		if (link) {
			m1_quad cquad;
			cquad.p[0] = old_p1;
			cquad.p[1] = old_p0;
			cquad.p[2] = p0;
			cquad.p[3] = p1;
			cquad.z = z;

			INT32 lumval = (INT32)(((float)(lum >> 24)) * 2.0f);
			lumval >>= 2;

			UINT16 tex_data = tgp_ram_entry(tex_adr);
			cquad.col = shade_color(tex_data, lumval, 0);
			cquad.moire = 0;

			INT32 skip = 0;

			if (flags & 0x00002000) {
				cquad.moire = 1;

				// a stippled poly with the transparent pen fades toward the backdrop
				if ((tex_data & 0x3ff) == 0)
					cquad.col = 0;

				// degenerate moire record seen in wingwar, not drawn on hardware
				if (cquad.p[0]->sx == cquad.p[1]->sx && cquad.p[1]->sx == cquad.p[2]->sx && cquad.p[2]->sx == cquad.p[3]->sx &&
					cquad.p[0]->sy == cquad.p[1]->sy && cquad.p[1]->sy == cquad.p[2]->sy && cquad.p[2]->sy == cquad.p[3]->sy)
					skip = 1;
			}

			// no frustum clipping, the rasterizer scissors to the viewport
			if (!skip) store_quad(cquad);
		}

		switch (link) {
			case 0:
			case 2:
				old_p0 = p0;
				old_p1 = p1;
				break;
			case 1:
				old_p1 = p0;
				break;
			case 3:
				old_p0 = p1;
				break;
		}
	}

	list_offset += 4;
	return list_offset;
}

static INT32 skip_direct(INT32 list_offset)
{
	list_offset += 18;

	for (;;) {
		UINT32 flags = readi(list_offset + 2);

		INT32 type = flags & 3;
		if (!type)
			break;

		list_offset += (type == 2) ? 12 : 20;
	}

	list_offset += 4;
	return list_offset;
}

static int quad_compare(const void *a, const void *b)
{
	const m1_quad_out *q1 = (const m1_quad_out*)a;
	const m1_quad_out *q2 = (const m1_quad_out*)b;

	// far to near, in list order when equal
	if (q1->z < q2->z) return +1;
	if (q1->z > q2->z) return -1;
	return (q1->order < q2->order) ? -1 : +1;
}

// close the current group of quads: sorted together, drawn in the current viewport
static void draw_objects()
{
	INT32 count = quad_count - quad_segment_start;

	if (count > 0 && segment_count[render_pass] < MAX_SEGMENTS) {
		qsort(&quaddb[quad_segment_start], count, sizeof(m1_quad_out), quad_compare);

		m1_quad_segment *s = &segments[render_pass][segment_count[render_pass]++];
		s->start = quad_segment_start;
		s->count = count;
		s->x1 = view.x1;
		s->x2 = view.x2;
		s->y1 = view.y1;
		s->y2 = view.y2;
	}

	quad_segment_start = quad_count;
	point_count = 0;
}

static UINT16 *dlist_override;	// building again from a copy

static UINT16 *get_current_list()
{
	if (dlist_override) return dlist_override;

	if (!(listctl[0] & 4))
		listctl[0] = (listctl[0] & ~0x40) | ((listctl[0] & 8) ? 0x40 : 0);

	return (UINT16*)((listctl[0] & 0x40) ? DrvDispList1 : DrvDispList0);
}

static void tgp_render(INT32 pass)
{
	render_pass = pass;

	if ((listctl[1] & 0x1f) != 0x1f)
		return;

	dlist_current = get_current_list();

	memset(view.translation, 0, sizeof(view.translation));
	view.translation[0] = 1.0f;
	view.translation[4] = 1.0f;
	view.translation[8] = 1.0f;

	float old_z = 0;
	INT32 list_offset = 0;

	for (INT32 guard = 0; guard < 0x10000; guard++) {
		INT32 type = readi(list_offset + 0);

		switch (type) {
			case 0:
				list_offset += 2;
				break;

			case 1:		// object below the hud tilemaps
				if (pass == RENDER_BELOW_HUD)
					push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), old_z);
				list_offset += 8;
				break;

			case 0x41:	// object above the hud (swa radar blips)
				if (pass == RENDER_ABOVE_HUD)
					push_object(readi(list_offset + 2), readi(list_offset + 4), readi(list_offset + 6), old_z);
				list_offset += 8;
				break;

			case 2:		// direct (2d projected) polys, sorted with the objects
				if (pass == RENDER_BELOW_HUD)
					list_offset = push_direct(list_offset);
				else
					list_offset = skip_direct(list_offset);
				break;

			case 3: {	// viewport
				draw_objects();

				float xc = readi16(list_offset + 4);
				float yc = 383 - (readi16(list_offset + 6) - 39);
				float x1 = readi16(list_offset + 8);
				float y2 = 383 - (readi16(list_offset + 10) - 39);
				float x2 = readi16(list_offset + 12);
				float y1 = 383 - (readi16(list_offset + 14) - 39);

				view.xc = (INT32)xc;
				view.yc = (INT32)yc;
				view.x1 = (INT32)x1;
				view.x2 = (INT32)x2;
				view.y1 = (INT32)y1;
				view.y2 = (INT32)y2;
				recompute_frustum();

				list_offset += 16;
				break;
			}

			case 4: {	// color write
				INT32 adr = readi(list_offset + 2);
				INT32 len = readi(list_offset + 4) + 1;
				for (INT32 i = 0; i < len; i++) {
					UINT32 a = adr - 0x40000 + i;
					if (a < TGP_RAM_SIZE) DrvTGPRAM[a] = readi16(list_offset + 6 + 2 * i);
				}
				list_offset += 6 + len * 2;
				break;
			}

			case 5: {	// polygon ram write
				INT32 adr = readi(list_offset + 2);
				INT32 len = readi(list_offset + 4);
				for (INT32 i = 0; i < len; i++) {
					DrvPolyRAM[(adr - 0x800000 + i) & (POLY_RAM_SIZE - 1)] = BURN_ENDIAN_SWAP_INT32(readi(list_offset + 2 * i + 6));
				}
				list_offset += 6 + len * 2;
				break;
			}

			case 6: {	// light parameters
				INT32 adr = readi(list_offset + 2);
				INT32 len = readi(list_offset + 4);
				for (INT32 i = 0; i < len; i++) {
					UINT32 v = readi(list_offset + 6 + i * 2);
					m1_lightparam *lp = &view.lightparams[(i + adr) & 0xff];
					lp->d = (float)(v & 0xff) / 255.0f;
					lp->a = (float)((v >> 8) & 0xff) / 255.0f;
					lp->s = (float)((v >> 16) & 0xff) / 255.0f;
					lp->p = (v >> 24) & 0xff;
				}
				list_offset += 6 + len * 2;
				break;
			}

			case 7:		// mode word, bit 0 enables the specular term
				view.spec_enable = readi(list_offset + 2) & 1;
				list_offset += 4;
				break;

			case 8:		// select mode
				list_offset += 4;
				break;

			case 9:		// zoom
				view.zoomx = readf(list_offset + 2) * 4;
				view.zoomy = readf(list_offset + 4) * 4;
				recompute_frustum();
				list_offset += 6;
				break;

			case 0xa: {	// light vector
				view.light[0] = readf(list_offset + 2);
				view.light[1] = readf(list_offset + 4);
				view.light[2] = readf(list_offset + 6);
				normalize(view.light);
				list_offset += 8;
				break;
			}

			case 0xb:	// matrix
				for (INT32 i = 0; i < 12; i++)
					view.translation[i] = readf(list_offset + 2 + 2 * i);
				list_offset += 26;
				break;

			case 0xc:	// view translation
				view.viewx = readf(list_offset + 2);
				view.viewy = readf(list_offset + 4);
				recompute_frustum();
				list_offset += 6;
				break;

			case 0xf:
			default:
				guard = 0x10000;
				break;
		}
	}

	draw_objects();
}

// build the polygons of a list into frames3d[n]
static void build_list(INT32 n)
{
	m1_frame3d *f = &frames3d[n];
	quaddb = f->quads;
	segments[0] = f->segs[0];
	segments[1] = f->segs[1];

	quad_count = 0;
	quad_segment_start = 0;
	point_count = 0;
	segment_count[0] = segment_count[1] = 0;

	tgp_render(RENDER_BELOW_HUD);
	tgp_render(RENDER_ABOVE_HUD);

	f->seg_count[0] = segment_count[0];
	f->seg_count[1] = segment_count[1];
	f->serial = ++frame3d_serials;
}

static void build_input_take(m1_build_input *b)
{
	UINT16 c = listctl[0];
	UINT16 *list = get_current_list();
	listctl[0] = c;

	memcpy(b->list, list, 0x10000);
	memcpy(b->pal, DrvPalRAM, 0x4000);
	memcpy(b->xlat, DrvXlatRAM, 0xc000);
	b->view = view;
	b->listctl0 = listctl[0];
	b->frame = frame_number - 1;
	b->valid = 1;
}

// the last frame's list, at the start of the next one (nothing runs in between)
static void build_frame()
{
	if (!build_pending) return;
	build_pending = 0;

	INT32 n = frame3d_new ^ 1;
	build_input_take(&builds[n]);

	frame_number--;	// the frame of the list
	build_list(n);
	frame_number++;

	frame3d_new = n;
	frame3d_lost[n] = 0;
}

// after a state load: a list built again from what its build read, without changing the state
static void build_again(INT32 n)
{
	m1_build_input *b = &builds[n];
	frame3d_lost[n] = 0;

	if (!b->valid) {	// not in the state (runahead, netplay): from the current list
		build_input_take(b);
		b->frame = frame_number;
	}

	static UINT16 *tgp_copy = NULL;
	static UINT32 *poly_copy = NULL;
	if (!tgp_copy) {
		tgp_copy = (UINT16*)BurnMalloc(TGP_RAM_SIZE * sizeof(UINT16));
		poly_copy = (UINT32*)BurnMalloc(POLY_RAM_SIZE * sizeof(UINT32));
	}

	m1_view live_view = view;
	UINT16 live_listctl[2] = { listctl[0], listctl[1] };
	UINT32 live_frame = frame_number;
	UINT16 *live_tgp = DrvTGPRAM;
	UINT32 *live_poly = DrvPolyRAM;
	UINT8 *live_pal = DrvPalRAM;
	UINT8 *live_xlat = DrvXlatRAM;

	memcpy(tgp_copy, DrvTGPRAM, TGP_RAM_SIZE * sizeof(UINT16));
	memcpy(poly_copy, DrvPolyRAM, POLY_RAM_SIZE * sizeof(UINT32));
	DrvTGPRAM = tgp_copy;
	DrvPolyRAM = poly_copy;
	DrvPalRAM = (UINT8*)b->pal;
	DrvXlatRAM = (UINT8*)b->xlat;
	dlist_override = b->list;
	view = b->view;
	listctl[0] = b->listctl0;
	frame_number = b->frame;

	build_list(n);

	DrvTGPRAM = live_tgp;
	DrvPolyRAM = live_poly;
	DrvPalRAM = live_pal;
	DrvXlatRAM = live_xlat;
	dlist_override = NULL;
	view = live_view;
	listctl[0] = live_listctl[0];
	listctl[1] = live_listctl[1];
	frame_number = live_frame;
}

// the list the screen shows: of two frames before
static void draw3d_pick()
{
	INT32 n = frame3d_new ^ 1;
	if (frame3d_lost[n]) build_again(n);
	draw3d = &frames3d[n];
}

static void end_frame()
{
	if ((listctl[0] & 4) && (frame_number & 1))
		listctl[0] ^= 0x40;
}

// rasterizer

static void draw_hline(INT32 x1, INT32 x2, INT32 y, INT32 color, INT32 moire)
{
	UINT16 *base = Dst3D + y * nScreenWidth;

	if (moire) {
		for (; x1 <= x2; x1++) {
			if (!((x1 ^ y) & 1))
				base[x1] = color;
		}
	} else {
		// 4 pixels per store (memcpy keeps it aliasing safe)
		UINT64 v = (UINT64)(UINT16)color * 0x0001000100010001ULL;
		for (; x1 <= x2 && (x1 & 3); x1++)
			base[x1] = color;
		for (; x1 + 3 <= x2; x1 += 4)
			memcpy(base + x1, &v, 8);
		for (; x1 <= x2; x1++)
			base[x1] = color;
	}
}

// the screen rows are shared out between the render threads: row % nRenderParts
static INT32 nRenderParts = 1;

static void fill_slope(m1_quad_segment *v, INT32 color, INT32 moire, INT32 x1, INT32 x2, INT32 sl1, INT32 sl2, INT32 y1, INT32 y2, INT32 *nx1, INT32 *nx2, INT32 part)
{
	if (y1 > v->y2)
		return;

	if (y2 <= v->y1) {
		INT32 delta = y2 - y1;
		*nx1 = x1 + delta * sl1;
		*nx2 = x2 + delta * sl2;
		return;
	}

	if (y2 > v->y2)
		y2 = v->y2 + 1;

	if (y1 < v->y1) {
		INT32 delta = v->y1 - y1;
		x1 += delta * sl1;
		x2 += delta * sl2;
		y1 = v->y1;
	}

	if (x1 > x2 || (x1 == x2 && sl1 > sl2)) {
		INT32 t = x1; x1 = x2; x2 = t;
		t = sl1; sl1 = sl2; sl2 = t;
		INT32 *tp = nx1; nx1 = nx2; nx2 = tp;
	}

	INT32 own = (y1 - part) % nRenderParts;	// 0 on the rows of this thread
	if (own < 0) own += nRenderParts;

	while (y1 < y2) {
		if (own == 0 && y1 >= v->y1) {
			INT32 xx1 = x1 >> FRAC_SHIFT;
			INT32 xx2 = x2 >> FRAC_SHIFT;
			if (xx1 <= v->x2 || xx2 >= v->x1) {
				if (xx1 < v->x1) xx1 = v->x1;
				if (xx2 > v->x2) xx2 = v->x2;
				draw_hline(xx1, xx2, y1, color, moire);
			}
		}

		x1 += sl1;
		x2 += sl2;
		y1++;
		if (++own == nRenderParts) own = 0;
	}

	*nx1 = x1;
	*nx2 = x2;
}

static void fill_line(m1_quad_segment *v, INT32 color, INT32 moire, INT32 y, INT32 x1, INT32 x2, INT32 part)
{
	INT32 xx1 = x1 >> FRAC_SHIFT;
	INT32 xx2 = x2 >> FRAC_SHIFT;

	if (y > v->y2 || y < v->y1 || (y % nRenderParts) != part)
		return;

	if (xx1 <= v->x2 || xx2 >= v->x1) {
		if (xx1 < v->x1) xx1 = v->x1;
		if (xx2 > v->x2) xx2 = v->x2;
		draw_hline(xx1, xx2, y, color, moire);
	}
}

// wireframe primitives come as degenerate quads (A,A,B,B)
static void draw_wireframe_line(m1_quad_segment *v, INT32 x1, INT32 y1, INT32 x2, INT32 y2, INT32 color, INT32 moire, INT32 part)
{
	// Liang-Barsky clip to the viewport
	const double lx1 = x1, ly1 = y1;
	const double cdx = (double)x2 - lx1, cdy = (double)y2 - ly1;
	const double p[4] = { -cdx, cdx, -cdy, cdy };
	const double q[4] = { lx1 - v->x1, v->x2 - lx1, ly1 - v->y1, v->y2 - ly1 };
	double t0 = 0.0, t1 = 1.0;

	for (INT32 i = 0; i < 4; i++) {
		if (p[i] == 0.0) {
			if (q[i] < 0.0) return;
		} else {
			const double t = q[i] / p[i];
			if (p[i] < 0.0) {
				if (t > t0) t0 = t;
			} else {
				if (t < t1) t1 = t;
			}
		}
	}
	if (t0 > t1) return;

	if (t1 < 1.0) {
		x2 = (INT32)floor(lx1 + t1 * cdx + 0.5);
		y2 = (INT32)floor(ly1 + t1 * cdy + 0.5);
	}
	if (t0 > 0.0) {
		x1 = (INT32)floor(lx1 + t0 * cdx + 0.5);
		y1 = (INT32)floor(ly1 + t0 * cdy + 0.5);
	}

	INT32 dx = abs(x2 - x1), dy = abs(y2 - y1);
	INT32 sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
	INT32 err = dx - dy;

	for (;;) {
		if (x1 >= 0 && x1 < nScreenWidth && y1 >= 0 && y1 < nScreenHeight && (y1 % nRenderParts) == part)
			if (!moire || !((x1 ^ y1) & 1))
				Dst3D[y1 * nScreenWidth + x1] = color;
		if (x1 == x2 && y1 == y2)
			break;
		INT32 e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x1 += sx; }
		if (e2 < dx)  { err += dx; y1 += sy; }
	}
}

static void fill_quad(m1_quad_segment *v, m1_quad_out *q, INT32 part)
{
	INT32 color = q->col;
	INT32 moire = q->moire;

	// wireframe: only two distinct vertices
	{
		INT32 ax = q->sx[0], ay = q->sy[0];
		INT32 bx = ax, by = ay, ndist = 1;
		for (INT32 i = 1; i < 4; i++) {
			INT32 vx = q->sx[i], vy = q->sy[i];
			if (vx == ax && vy == ay)
				continue;
			if (ndist == 1) {
				bx = vx;
				by = vy;
				ndist = 2;
			} else if (vx != bx || vy != by) {
				ndist = 3;
				break;
			}
		}
		if (ndist == 2) {
			draw_wireframe_line(v, ax, ay, bx, by, color, moire, part);
			return;
		}
	}

	struct { INT32 x, y; } p[8];

	for (INT32 i = 0; i < 4; i++) {
		p[i].x = p[i + 4].x = q->sx[i] << FRAC_SHIFT;
		p[i].y = p[i + 4].y = q->sy[i];
	}

	INT32 pmin = 0;
	INT32 pmax = 0;
	for (INT32 i = 1; i < 4; i++) {
		if (p[i].y < p[pmin].y) pmin = i;
		if (p[i].y > p[pmax].y) pmax = i;
	}

	INT32 cury = p[pmin].y;
	INT32 limy = p[pmax].y;

	INT32 x1, x2;
	if (cury == limy) {
		x1 = p[0].x;
		x2 = p[0].x;
		for (INT32 i = 1; i < 4; i++) {
			if (p[i].x < x1) x1 = p[i].x;
			if (p[i].x > x2) x2 = p[i].x;
		}
		fill_line(v, color, moire, cury, x1, x2, part);
		return;
	}

	if (cury > v->y2) return;
	if (limy <= v->y1) return;
	if (limy > v->y2) limy = v->y2;

	INT32 ps1 = pmin + 4;
	INT32 ps2 = pmin;
	INT32 sl1, sl2;

	while (p[ps1 - 1].y == cury) ps1--;
	while (p[ps2 + 1].y == cury) ps2++;
	x1 = p[ps1].x;
	x2 = p[ps2].x;
	sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
	sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);

	for (;;) {
		if (p[ps1 - 1].y == p[ps2 + 1].y) {
			fill_slope(v, color, moire, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2, part);
			cury = p[ps1 - 1].y;
			if (cury >= limy) break;
			ps1--;
			ps2++;

			while (p[ps1 - 1].y == cury) ps1--;
			while (p[ps2 + 1].y == cury) ps2++;
			x1 = p[ps1].x;
			x2 = p[ps2].x;
			sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
			sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
		} else if (p[ps1 - 1].y < p[ps2 + 1].y) {
			fill_slope(v, color, moire, x1, x2, sl1, sl2, cury, p[ps1 - 1].y, &x1, &x2, part);
			cury = p[ps1 - 1].y;
			if (cury >= limy) break;
			ps1--;
			while (p[ps1 - 1].y == cury) ps1--;
			x1 = p[ps1].x;
			sl1 = (x1 - p[ps1 - 1].x) / (cury - p[ps1 - 1].y);
		} else {
			fill_slope(v, color, moire, x1, x2, sl1, sl2, cury, p[ps2 + 1].y, &x1, &x2, part);
			cury = p[ps2 + 1].y;
			if (cury >= limy) break;
			ps2++;
			while (p[ps2 + 1].y == cury) ps2++;
			x2 = p[ps2].x;
			sl2 = (x2 - p[ps2 + 1].x) / (cury - p[ps2 + 1].y);
		}
	}

	if (cury == limy)
		fill_line(v, color, moire, cury, x1, x2, part);
}

static void draw_pass_part(INT32 pass, INT32 part)
{
	for (INT32 s = 0; s < draw3d->seg_count[pass]; s++) {
		m1_quad_segment v = draw3d->segs[pass][s];

		if (nHiresMode) {
			// same rasterizer, on vertices projected at the high resolution
			m1_quad_segment hv = v;
			hv.x1 = (INT32)floorf(v.x1 * fHiresScaleX);
			hv.x2 = (INT32)floorf((v.x2 + 1) * fHiresScaleX) - 1;
			hv.y1 = (INT32)floorf(v.y1 * fHiresScaleY);
			hv.y2 = (INT32)floorf((v.y2 + 1) * fHiresScaleY) - 1;
			if (hv.x1 < 0) hv.x1 = 0;
			if (hv.x2 > nScreenWidth - 1) hv.x2 = nScreenWidth - 1;
			if (hv.y1 < 0) hv.y1 = 0;
			if (hv.y2 > nScreenHeight - 1) hv.y2 = nScreenHeight - 1;
			if (hv.x1 > hv.x2 || hv.y1 > hv.y2) continue;

			for (INT32 i = 0; i < v.count; i++) {
				m1_quad_out q = draw3d->quads[v.start + i];
				for (INT32 k = 0; k < 4; k++) {
					// a native pixel is several high resolution ones: a vertex on a viewport edge covers it all
					INT32 nx = q.sx[k], ny = q.sy[k];
					q.sx[k] = screen_coord(floorf(q.fx[k] * fHiresScaleX + SNAP_EPSILON));
					q.sy[k] = screen_coord(floorf(q.fy[k] * fHiresScaleY + SNAP_EPSILON));
					if (nx == v.x2) q.sx[k] = (INT32)floorf((v.x2 + 1) * fHiresScaleX) - 1;
					if (ny == v.y2) q.sy[k] = (INT32)floorf((v.y2 + 1) * fHiresScaleY) - 1;
				}
				fill_quad(&hv, &q, part);
			}
			continue;
		}

		// clip the viewport to the screen
		if (v.x1 < 0) v.x1 = 0;
		if (v.x2 > nScreenWidth - 1) v.x2 = nScreenWidth - 1;
		if (v.y1 < 0) v.y1 = 0;
		if (v.y2 > nScreenHeight - 1) v.y2 = nScreenHeight - 1;
		if (v.x1 > v.x2 || v.y1 > v.y2) continue;

		for (INT32 i = 0; i < v.count; i++)
			fill_quad(&v, &draw3d->quads[v.start + i], part);
	}
}

// render threads (thready.h): every job is split by rows, the output doesn't depend on their number

static threadystruct M1Workers[M1_MAX_PARTS - 1];
static INT32 nM1Workers = 0;
static void (*RenderJob)(INT32 part);

static INT32 RenderBase = 1;	// 1: the main thread does part 0, 0: only the workers draw
static INT32 render_busy;

template <INT32 N>
static void RenderWorker()
{
	RenderJob(N + RenderBase);
}

static void (*const RenderWorkers[M1_MAX_PARTS - 1])() = {
	RenderWorker<0>, RenderWorker<1>, RenderWorker<2>, RenderWorker<3>, RenderWorker<4>, RenderWorker<5>, RenderWorker<6>
};

static void render_threads_init()
{
	nM1Workers = thready_cpu_count() - 1;
	if (nM1Workers > M1_MAX_PARTS - 1) nM1Workers = M1_MAX_PARTS - 1;
	if (nM1Workers < 0) nM1Workers = 0;

	for (INT32 i = 0; i < nM1Workers; i++) {
		M1Workers[i].init(RenderWorkers[i]);
		if (!M1Workers[i].is_threaded()) {
			nM1Workers = i;
			break;
		}
	}
	render_busy = 0;
}

static void render_finish();

static void render_threads_exit()
{
	render_finish();
	for (INT32 i = 0; i < nM1Workers; i++) M1Workers[i].exit();
	nM1Workers = 0;
}

static void render_finish()
{
	if (!render_busy) return;
	for (INT32 i = 0; i < nM1Workers; i++) M1Workers[i].notify_wait();
	render_busy = 0;
}

static void render_run(void (*job)(INT32 part))
{
	render_finish();

	RenderJob = job;
	RenderBase = 1;
	nRenderParts = nM1Workers + 1;

	for (INT32 i = 0; i < nM1Workers; i++) M1Workers[i].notify();
	job(0);
	for (INT32 i = 0; i < nM1Workers; i++) M1Workers[i].notify_wait();
}

// the workers alone, while the main thread goes on
static void render_start(void (*job)(INT32 part))
{
	RenderJob = job;
	RenderBase = 0;
	nRenderParts = nM1Workers;

	for (INT32 i = 0; i < nM1Workers; i++) M1Workers[i].notify();
	render_busy = 1;
}

// band of rows of a part, for the 2d jobs (better locality than interleaved rows)
static inline void render_band(INT32 part, INT32 *y1, INT32 *y2)
{
	*y1 = nScreenHeight * part / nRenderParts;
	*y2 = nScreenHeight * (part + 1) / nRenderParts;
}

static INT32 nPassToDraw;

static void draw_pass_job(INT32 part)
{
	draw_pass_part(nPassToDraw, part);
}

static void draw_pass(INT32 pass)
{
	if (draw3d->seg_count[pass] == 0) return;
	nPassToDraw = pass;
	Dst3D = pTransDraw;
	render_run(draw_pass_job);
}

// the 3d under the hud goes to Frame3D, drawn by the workers while the frame runs
static UINT32 frame3d_serial;	// the list in Frame3D (0: none)
static UINT32 async_serial;

static void draw_below_job(INT32 part)
{
	for (INT32 y = part; y < nScreenHeight; y += nRenderParts) {
		UINT16 *d = Frame3D + y * nScreenWidth;
		for (INT32 x = 0; x < nScreenWidth; x++) d[x] = F3D_NONE;
	}
	draw_pass_part(RENDER_BELOW_HUD, part);
}

static void draw_below_kick()
{
	if (render_busy || nM1Workers == 0 || !(nBurnLayer & 2)) return;

	draw3d_pick();
	if (frame3d_serial == draw3d->serial) return;

	Dst3D = Frame3D;
	async_serial = draw3d->serial;
	render_start(draw_below_job);
}

static void draw_below()
{
	if (render_busy) {
		render_finish();
		frame3d_serial = async_serial;
	}

	if (!(nBurnLayer & 2)) {
		for (INT32 i = 0; i < nScreenWidth * nScreenHeight; i++) Frame3D[i] = F3D_NONE;
		frame3d_serial = 0;
		return;
	}

	if (frame3d_serial != draw3d->serial) {
		Dst3D = Frame3D;
		render_run(draw_below_job);
		frame3d_serial = draw3d->serial;
	}
}

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 0x2000; i++) {
		palette_update_entry(i);
	}

	// flat shaded polygons, xRGB 555
	for (INT32 i = 0; i < 0x8000; i++) {
		DrvPalette[0x8000 + i] = BurnHighCol(pal5bit(i >> 10), pal5bit(i >> 5), pal5bit(i), 0);
	}

	for (INT32 i = 0; i < 256; i++) {
		HiresLut[0][i] = BurnHighCol(i, 0, 0, 0);
		HiresLut[1][i] = BurnHighCol(0, i, 0, 0);
		HiresLut[2][i] = BurnHighCol(0, 0, i, 0);
	}
	bHiresRGB888 = (nBurnBpp == 4) && (BurnHighCol(0x12, 0x34, 0x56, 0) == 0x123456);
}

static inline UINT32 rgb_lerp(UINT32 a, UINT32 b, UINT32 w)	// w (0-256) for a
{
	if (a == b) return a;	// flat areas are the common case
	UINT32 rb = (((a & 0xff00ff) * w + (b & 0xff00ff) * (256 - w)) >> 8) & 0xff00ff;
	UINT32 g  = (((a & 0x00ff00) * w + (b & 0x00ff00) * (256 - w)) >> 8) & 0x00ff00;
	return rb | g;
}

static inline UINT32 rgb_scale(UINT32 a, UINT32 w)
{
	return ((((a & 0xff00ff) * w) >> 8) & 0xff00ff) | ((((a & 0x00ff00) * w) >> 8) & 0x00ff00);
}

static inline UINT32 pen_rgb(UINT16 c)
{
	if (c & 0x8000) return (pal5bit(c >> 10) << 16) | (pal5bit(c >> 5) << 8) | pal5bit(c);
	return DrvPalRGB[c & 0x1fff];
}

// The 2d layers are area filtered: a hires pixel straddles at most two native pixels per axis.
// Both passes are separable, the horizontally filtered native rows are cached (sy & 1 slot).

static UINT32 *back_row(hires_rows *r, INT32 sy)
{
	INT32 slot = sy & 1;
	UINT32 *out = r->back[slot];
	if (r->back_tag[slot] == sy) return out;
	r->back_tag[slot] = sy;

	UINT16 *src = TileBack + sy * NATIVE_W;
	for (INT32 x = 0; x < nScreenWidth; x++) {
		UINT16 *s = src + HiresMapX[x];
		UINT32 wx = HiresWX[x];
		out[x] = (wx == 256) ? DrvPalRGB[s[0]] : rgb_lerp(DrvPalRGB[s[0]], DrvPalRGB[s[1]], wx);
	}
	return out;
}

// a single background pixel, when filtering whole rows isn't worth it
static UINT32 hires_back_pixel(INT32 x, INT32 y)
{
	UINT32 wx = HiresWX[x], wy = HiresWY[y];
	UINT16 *src = TileBack + HiresMapY[y] * NATIVE_W + HiresMapX[x];

	UINT32 top = (wx == 256) ? DrvPalRGB[src[0]] : rgb_lerp(DrvPalRGB[src[0]], DrvPalRGB[src[1]], wx);
	if (wy == 256) return top;

	src += NATIVE_W;
	UINT32 bot = (wx == 256) ? DrvPalRGB[src[0]] : rgb_lerp(DrvPalRGB[src[0]], DrvPalRGB[src[1]], wx);
	return rgb_lerp(top, bot, wy);
}

static inline UINT32 hires_pixel_rgb(INT32 x, INT32 y, UINT16 c)
{
	if (c == HIRES_RGB) return HiresRGB[y * nScreenWidth + x];
	if (c == HIRES_BACK) return hires_back_pixel(x, y);
	return pen_rgb(c);
}

static void hires_rows_reset(hires_rows *r)
{
	r->back_tag[0] = r->back_tag[1] = -1;
	r->hud_tag[0] = r->hud_tag[1] = -1;
}

// premultiplied colour and coverage (0-256) of a hud row, only valid in the chunks of *mask
static UINT32 *hud_row(hires_rows *r, INT32 sy, UINT16 **alpha, UINT32 *mask)
{
	INT32 slot = sy & 1;
	UINT32 *out = r->hud[slot];
	*alpha = r->alpha[slot];
	if (r->hud_tag[slot] != sy) {
		r->hud_tag[slot] = sy;

		UINT16 *src = TileHud + sy * NATIVE_W;
		UINT32 m = 0;
		for (INT32 c = 0; c < NATIVE_W / 16; c++) {
			const UINT16 *t = src + c * 16;
			UINT16 all = 0xffff;
			for (INT32 x = 0; x < 16; x++) all &= t[x];
			if (all != 0xffff) m |= 1 << c;
		}
		m |= m >> 1;
		r->hud_mask[slot] = m;

		for (INT32 c = 0; c < NATIVE_W / 16; c++) {
			if (!(m & (1 << c))) continue;

			UINT16 *a = *alpha;
			for (INT32 x = HiresChunkX[c]; x < HiresChunkX[c + 1]; x++) {
				UINT16 *s = src + HiresMapX[x];
				UINT32 wx = HiresWX[x];
				UINT32 a0 = (s[0] != 0xffff) ? wx : 0;
				UINT32 a1 = (wx != 256 && s[1] != 0xffff) ? (256 - wx) : 0;
				UINT32 c0 = a0 ? DrvPalRGB[s[0]] : 0;
				UINT32 c1 = a1 ? DrvPalRGB[s[1]] : 0;
				a[x] = a0 + a1;
				out[x] = ((((c0 & 0xff00ff) * a0 + (c1 & 0xff00ff) * a1) >> 8) & 0xff00ff) |
						 ((((c0 & 0x00ff00) * a0 + (c1 & 0x00ff00) * a1) >> 8) & 0x00ff00);
			}
		}
	}
	*mask = r->hud_mask[slot];
	return out;
}

// hud blended over the frame, with its coverage as alpha (no fringe around the characters)
static void hires_hud_blend_job(INT32 part)
{
	hires_rows *r = HiresRows + part;
	hires_rows_reset(r);

	INT32 ys, ye;
	render_band(part, &ys, &ye);

	for (INT32 y = ys; y < ye; y++) {
		INT32 sy = HiresMapY[y];
		UINT32 wy = HiresWY[y];
		UINT16 *at, *ab = NULL;
		UINT32 mt, mb = 0;
		UINT32 *ptr = hud_row(r, sy, &at, &mt);
		UINT32 *pbr = (wy != 256) ? hud_row(r, sy + 1, &ab, &mb) : NULL;
		if ((mt | mb) == 0) continue;

		UINT16 *native = TileHud + sy * NATIVE_W;
		UINT16 *dst = pTransDraw + y * nScreenWidth;
		UINT32 *rgb = HiresRGB + y * nScreenWidth;

		for (INT32 c = 0; c < NATIVE_W / 16; c++) {
			UINT32 bit = 1 << c;
			if (!((mt | mb) & bit)) continue;
			UINT32 *pt = (mt & bit) ? ptr : NULL;
			UINT32 *pb = (mb & bit) ? pbr : NULL;

			for (INT32 x = HiresChunkX[c]; x < HiresChunkX[c + 1]; x++) {
				UINT32 a, p;
				if (pb == NULL) {
					if (wy == 256) {
						a = at[x]; p = pt[x];
					} else {
						a = (at[x] * wy) >> 8; p = rgb_scale(pt[x], wy);
					}
				} else if (pt == NULL) {
					a = (ab[x] * (256 - wy)) >> 8; p = rgb_scale(pb[x], 256 - wy);
				} else {
					a = (at[x] * wy + ab[x] * (256 - wy)) >> 8; p = rgb_lerp(pt[x], pb[x], wy);
				}
				if (a == 0) continue;

				if (a == 256 && wy == 256 && HiresWX[x] == 256) {
					dst[x] = native[HiresMapX[x]];
					continue;
				}

				rgb[x] = (a == 256) ? p : (p + rgb_scale(hires_pixel_rgb(x, y, dst[x]), 256 - a));
				dst[x] = HIRES_RGB;
			}
		}
	}
}

template <INT32 rgb888>
static void hires_transfer_rows(hires_rows *r, INT32 ys, INT32 ye)
{
	for (INT32 y = ys; y < ye; y++) {
		UINT16 *src = pTransDraw + y * nScreenWidth;
		UINT32 *rgbline = HiresRGB + y * nScreenWidth;
		UINT32 *line = (nBurnBpp == 4) ? (UINT32*)(pBurnDraw + y * nBurnPitch) : r->out;
		UINT32 wy = HiresWY[y];
		UINT32 *bt = NULL, *bb = NULL;

		for (INT32 x = 0; x < nScreenWidth; x++) {
			UINT16 c = src[x];
			if ((UINT16)(c - 0x1000) >= 0x7000) {	// a pen: tiles < 0x1000, polygons >= 0x8000
				line[x] = DrvPalette[c];
				continue;
			}

			UINT32 rgb;
			if (c == HIRES_RGB) {
				rgb = rgbline[x];
			} else {
				if (bt == NULL) {
					bt = back_row(r, HiresMapY[y]);
					if (wy != 256) bb = back_row(r, HiresMapY[y] + 1);
				}
				rgb = bb ? rgb_lerp(bt[x], bb[x], wy) : bt[x];
			}
			line[x] = rgb888 ? rgb : (HiresLut[0][rgb >> 16] | HiresLut[1][(rgb >> 8) & 0xff] | HiresLut[2][rgb & 0xff]);
		}

		UINT8 *dst = pBurnDraw + y * nBurnPitch;
		if (nBurnBpp == 2) {
			for (INT32 x = 0; x < nScreenWidth; x++) ((UINT16*)dst)[x] = line[x];
		} else if (nBurnBpp == 3) {
			for (INT32 x = 0; x < nScreenWidth; x++) {
				dst[x * 3 + 0] = line[x]; dst[x * 3 + 1] = line[x] >> 8; dst[x * 3 + 2] = line[x] >> 16;
			}
		}
	}
}

static void hires_transfer_job(INT32 part)
{
	hires_rows *r = HiresRows + part;
	hires_rows_reset(r);

	INT32 ys, ye;
	render_band(part, &ys, &ye);

	if (bHiresRGB888) {
		hires_transfer_rows<1>(r, ys, ye);
	} else {
		hires_transfer_rows<0>(r, ys, ye);
	}
}

// background layers: marked for the filtered transfer, or scaled up (nearest)
static void hires_back_job(INT32 part)
{
	INT32 ys, ye;
	render_band(part, &ys, &ye);

	for (INT32 y = ys; y < ye; y++) {
		UINT16 *src = TileBack + HiresMapY[y] * NATIVE_W;
		UINT16 *f3d = Frame3D + y * nScreenWidth;
		UINT16 *dst = pTransDraw + y * nScreenWidth;
		for (INT32 x = 0; x < nScreenWidth; x++) {
			if (f3d[x] != F3D_NONE) dst[x] = f3d[x];
			else dst[x] = bHiresSharp ? HIRES_BACK : src[HiresMapX[x]];
		}
	}
}

static void hires_hud_nearest_job(INT32 part)
{
	INT32 ys, ye;
	render_band(part, &ys, &ye);

	for (INT32 y = ys; y < ye; y++) {
		UINT16 *src = TileHud + HiresMapY[y] * NATIVE_W;
		UINT16 *dst = pTransDraw + y * nScreenWidth;
		for (INT32 x = 0; x < nScreenWidth; x++) {
			UINT16 c = src[HiresMapX[x]];
			if (c != 0xffff) dst[x] = c;
		}
	}
}

// objects drawn over the hud, only through its dark/transparent pixels
static void draw_above_hud()
{
	if ((nBurnLayer & 8) && draw3d->seg_count[RENDER_ABOVE_HUD]) {
		INT32 len = nScreenWidth * nScreenHeight;
		UINT16 *snapshot = (UINT16*)BurnMalloc(len * sizeof(UINT16));

		if (snapshot) {
			memcpy(snapshot, pTransDraw, len * sizeof(UINT16));

			draw_pass(RENDER_ABOVE_HUD);

			UINT16 *pal = (UINT16*)DrvPalRAM;
			for (INT32 i = 0; i < len; i++) {
				UINT16 c = snapshot[i];
				INT32 r, g, b;
				if (c >= 0x1000 && c < 0x8000) {
					UINT32 rgb = hires_pixel_rgb(i % nScreenWidth, i / nScreenWidth, c);
					r = rgb >> 16; g = (rgb >> 8) & 0xff; b = rgb & 0xff;
				} else if (c & 0x8000) {
					r = pal5bit(c >> 10); g = pal5bit(c >> 5); b = pal5bit(c);
				} else {
					UINT16 v = BURN_ENDIAN_SWAP_INT16(pal[c & 0x1fff]);
					r = pal5bit(v); g = pal5bit(v >> 5); b = pal5bit(v >> 10);
					if (!(v & 0x8000)) { r >>= 1; g >>= 1; b >>= 1; }
				}
				if (r > 8 || g > 8 || b > 8) pTransDraw[i] = c;
			}

			BurnFree(snapshot);
		}
}
}

static INT32 DrvDrawHires()
{
	// background tilemaps scaled up, 3d objects and direct polys
	draw_below();
	render_run(hires_back_job);

	// hud tilemaps, only their opaque pixels
	if (nBurnLayer & 4) render_run(bHiresSharp ? hires_hud_blend_job : hires_hud_nearest_job);

	draw_above_hud();

	if (bHiresSharp) {
		pBurnDrvPalette = DrvPalette;
		render_run(hires_transfer_job);
	} else {
		BurnTransferCopy(DrvPalette);
	}

	return 0;
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	// not drawn by the last frame (state load, frame not shown): from the current tile ram
	if (tile_lines_done < NATIVE_H) {
		tile_lines_done = 0;
		tile_skip = 0;
		tile_draw_lines(NATIVE_H);
	}

	draw3d_pick();

	if (nHiresMode) {
		return DrvDrawHires();
	}

	// background tilemaps, 3d objects and direct polys
	draw_below();
	for (INT32 i = 0; i < NATIVE_W * NATIVE_H; i++) {
		pTransDraw[i] = (Frame3D[i] != F3D_NONE) ? Frame3D[i] : TileBack[i];
	}

	// hud tilemaps
	if (nBurnLayer & 4) {
		for (INT32 i = 0; i < NATIVE_W * NATIVE_H; i++) {
			if (TileHud[i] != 0xffff) pTransDraw[i] = TileHud[i];
		}
	}

	draw_above_hud();

	BurnTransferCopy(DrvPalette);

	return 0;
}

// -----------------------------------------------------------------------------

static INT32 DrvDoReset(INT32 clear_mem)
{
	if (clear_mem) {
		memset(AllRam, 0, RamEnd - AllRam);

		sega_315_5292_reset();
	}

	sega_315_5292_invalidate();

	v60Open(0);
	bankswitch(0x01);
	v60Reset();
	v60SetIRQLine(0, CPU_IRQSTATUS_NONE);
	v60Close();

	mb86233Reset();
	mb86233SetHalt(0);
	copro_reset();

	M1AudioReset();

	ZetOpen(0);
	ZetReset();
	ZetClose();


	EEPROMReset();
	irq_status = 0;
	irq_mask = 0xff;
	last_irq = 0;
	v60_irq_dirty = 0;
	memset(timer_period, 0, sizeof(timer_period));
	memset(timer_val, 0, sizeof(timer_val));
	memset(timer_remaining, 0, sizeof(timer_remaining));
	timer_mode = 0;
	listctl[0] = listctl[1] = 0;
	frame_number = 0;

	M1IOReset();

	comm_cn = 0;
	comm_fg = 0;

	r360_state = ~0x40;

	drive_vibrate = 0;
	drive_slide = 0;
	drive_turn = 0;
	drive_crash = 0;
	drive_speed[0] = drive_speed[1] = 0;
	drive_slowing = 0;
	drive_contact = 0;
	drive_airborne = 0;
	menu_wheel_on = 0;
	menu_band = 5;
	menu_repeat = 0;
	menu_dir = 0;

	if (bHasHMD) hmd_pose_write();

	M1UsartReset(0);
	M1UsartReset(1);
	M1UsartReset(2);

	if (bHasDSB) {
		ZetOpen(1);
		ZetReset();
		ZetClose();

		DsbReset();
	}
	usart_ticks_done = 0;

	memset(&view, 0, sizeof(view));
	for (INT32 i = 0; i < 2; i++) {
		frames3d[i].seg_count[0] = frames3d[i].seg_count[1] = 0;
		frames3d[i].serial = ++frame3d_serials;
		frame3d_lost[i] = 0;
		builds[i].valid = 0;
	}
	frame3d_new = 0;
	build_pending = 0;
	draw3d = &frames3d[1];

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvV60ROM		= Next; Next += 0x1800000;
	DrvTGPROM		= (UINT32*)Next; Next += 0x002000;
	DrvCoproTables	= (UINT32*)Next; Next += 0x040000;
	DrvCoproData	= (UINT32*)Next; Next += 0x200000;
	DrvPolyROM		= (UINT32*)Next; Next += 0x1000000;
	Drv68KROM		= Next; Next += 0x0c0000;
	DrvPCMROM0		= Next; Next += 0x400000;
	DrvPCMROM1		= Next; Next += 0x400000;
	DrvIOROM		= Next; Next += 0x010000;
	DrvEEPROM		= Next; Next += 0x000080;

	if (bHasDSB) {
		DrvDSBROM	= Next; Next += 0x020000;
		DrvDSBMPEG	= Next; Next += 0x800000;
	}


	DrvPalette		= (UINT32*)Next; Next += 0x10000 * sizeof(UINT32);
	DrvPalRGB		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);


	TileBack		= (UINT16*)Next; Next += NATIVE_W * NATIVE_H * sizeof(UINT16);
	TileHud			= (UINT16*)Next; Next += NATIVE_W * NATIVE_H * sizeof(UINT16);

	if (nHiresMode) {
		HiresMapX	= (INT32*)Next; Next += HIRES_MAX_W * sizeof(INT32);
		HiresMapY	= (INT32*)Next; Next += HIRES_MAX_H * sizeof(INT32);
		HiresWX		= (UINT16*)Next; Next += HIRES_MAX_W * sizeof(UINT16);
		HiresWY		= (UINT16*)Next; Next += HIRES_MAX_H * sizeof(UINT16);
		HiresRGB	= (UINT32*)Next; Next += HIRES_MAX_W * HIRES_MAX_H * sizeof(UINT32);
		HiresRows	= (hires_rows*)Next; Next += M1_MAX_PARTS * sizeof(hires_rows);
	}

	pointdb			= (m1_point*)Next; Next += (MAX_POINTS + 1) * sizeof(m1_point);
	for (INT32 i = 0; i < 2; i++) {
		frames3d[i].quads	= (m1_quad_out*)Next; Next += MAX_QUADS * sizeof(m1_quad_out);
		frames3d[i].segs[0]	= (m1_quad_segment*)Next; Next += MAX_SEGMENTS * sizeof(m1_quad_segment);
		frames3d[i].segs[1]	= (m1_quad_segment*)Next; Next += MAX_SEGMENTS * sizeof(m1_quad_segment);
		builds[i].list		= (UINT16*)Next; Next += 0x10000;
		builds[i].pal		= (UINT16*)Next; Next += 0x4000;
		builds[i].xlat		= (UINT16*)Next; Next += 0xc000;
	}

	DrvNVRAM		= Next; Next += 0x010000;	// saved as nvram

	AllRam			= Next;

	DrvMainRAM		= Next; Next += 0x040000;
	DrvDispList0	= Next; Next += 0x010000;
	DrvDispList1	= Next; Next += 0x010000;
	DrvTileRAM		= Next; Next += 0x010000;
	DrvCharRAM		= Next; Next += 0x080000;
	DrvPalRAM		= Next; Next += 0x004000;
	DrvXlatRAM		= Next; Next += 0x00c000;
	DrvCommRAM		= Next; Next += 0x001000;
	DrvDPRAM		= Next; Next += 0x000800;
	DrvCoproRAM		= (UINT32*)Next; Next += 0x2000 * sizeof(UINT32);
	DrvTGPRAM		= (UINT16*)Next; Next += TGP_RAM_SIZE * sizeof(UINT16);
	DrvPolyRAM		= (UINT32*)Next; Next += POLY_RAM_SIZE * sizeof(UINT32);
	Drv68KRAM		= Next; Next += 0x010000;
	DrvIORAM		= Next; Next += 0x002000;

	if (bHasDSB) {
		DrvDSBRAM	= Next; Next += 0x008000;
	}

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static void swap32_region(UINT32 *p, INT32 dwords)
{
#ifndef LSB_FIRST
	for (INT32 i = 0; i < dwords; i++) p[i] = BURN_ENDIAN_SWAP_INT32(p[i]);
#else
	(void)p; (void)dwords;
#endif
}

// rom types
#define M1_PRG		0x01	// v60 program, byte pairs at 0x200000
#define M1_BOOT		0x02	// v60 boot roms, packed at the top of the address space
#define M1_DATA		0x03	// v60 banked data, byte pairs at 0x1000000
#define M1_TGP		0x04	// tgp program
#define M1_68K		0x05	// sound board 68k
#define M1_PCM0		0x06	// multipcm #1 samples
#define M1_PCM1		0x07	// multipcm #2 samples
#define M1_POLY		0x08	// polygon models, word pairs
#define M1_TGPDATA	0x09	// tgp data, byte quads
#define M1_EEPROM	0x0a	// i/o board eeprom
#define M1_TABLES	0x0b	// tgp tables, word pair
#define M1_DSBPRG	0x0c	// digital sound board z80
#define M1_DSBDATA	0x0d	// digital sound board mpeg data
#define M1_NVRAM	0x0e	// default backup ram

static INT32 M1LoadRoms()
{
	memset(DrvV60ROM, 0xff, 0x1800000);

	char *pRomName;
	struct BurnRomInfo ri;

	INT32 boot_len = 0;
	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);
		if ((ri.nType & 0xff) == M1_BOOT) boot_len += ri.nLen;
	}

	INT32 prg = 0, boot = 0x1000000 - boot_len, data = 0, snd = 0, poly = 0, tgpdata = 0, tables = 0;
	INT32 pcm[2] = { 0, 0 };
	INT32 dsb_data = 0;

	for (INT32 i = 0; !BurnDrvGetRomName(&pRomName, i, 0); i++) {
		BurnDrvGetRomInfo(&ri, i);

		switch (ri.nType & 0xff) {
			case M1_PRG:
				if (BurnLoadRom(DrvV60ROM + 0x200000 + (prg >> 1) * ri.nLen * 2 + (prg & 1), i, 2)) return 1;
				prg++;
				break;

			case M1_BOOT:
				if (BurnLoadRom(DrvV60ROM + boot, i, 1)) return 1;
				boot += ri.nLen;
				break;

			case M1_DATA:
				if (BurnLoadRom(DrvV60ROM + 0x1000000 + (data >> 1) * ri.nLen * 2 + (data & 1), i, 2)) return 1;
				data++;
				break;

			case M1_TGP:
				if (BurnLoadRom((UINT8*)DrvTGPROM, i, 1)) return 1;
				break;

			case M1_68K:
				if (BurnLoadRom(Drv68KROM + snd, i, 1)) return 1;
				snd += ri.nLen;
				break;

			case M1_PCM0:
			case M1_PCM1: {
				INT32 n = (ri.nType & 0xff) - M1_PCM0;
				if (BurnLoadRom((n ? DrvPCMROM1 : DrvPCMROM0) + pcm[n], i, 1)) return 1;
				pcm[n] += ri.nLen;
				break;
			}

			case M1_POLY:
				if (BurnLoadRomExt((UINT8*)DrvPolyROM + (poly >> 1) * ri.nLen * 2 + (poly & 1) * 2, i, 4, LD_GROUP(2))) return 1;
				poly++;
				break;

			case M1_TGPDATA:
				if (BurnLoadRom((UINT8*)DrvCoproData + tgpdata, i, 4)) return 1;
				tgpdata++;
				break;

			case M1_EEPROM:
				if (BurnLoadRom(DrvEEPROM, i, 1)) return 1;
				bHasEEPROMImage = 1;
				break;

			case M1_TABLES:
				if (BurnLoadRomExt((UINT8*)DrvCoproTables + tables * 2, i, 4, LD_GROUP(2))) return 1;
				tables++;
				break;

			case M1_NVRAM:
				if (BurnLoadRom(DrvNVRAM, i, 1)) return 1;
				break;

			case M1_DSBPRG:
				if (BurnLoadRom(DrvDSBROM, i, 1)) return 1;
				break;

			case M1_DSBDATA:
				if (BurnLoadRom(DrvDSBMPEG + dsb_data, i, 1)) return 1;
				dsb_data += ri.nLen;
				break;
		}
	}

	// the boot roms are also seen at 0 by some games
	if (nBootMirror) {
		for (INT32 ofs = 0; ofs < 0x100000; ofs += boot_len) {
			memcpy(DrvV60ROM + ofs, DrvV60ROM + 0x1000000 - boot_len, boot_len);
			if (nBootMirror == 1) break;
		}
	}

	nCoproDataMask = (0x200000 / 4) - 1;

	// i/o board bios
	if (BurnLoadRom(DrvIOROM, 0x80 + nIOBios, 1)) return 1;

	swap32_region(DrvTGPROM, 0x800);
	swap32_region(DrvPolyROM, POLY_ROM_SIZE);
	swap32_region(DrvCoproData, 0x200000 / 4);
	swap32_region(DrvCoproTables, 0x10000);

	return 0;
}

// output pixel [d, d+1) covers native [d/scale, (d+1)/scale): its first native pixel and how much of it
static void hires_filter_map(INT32 *map, UINT16 *weight, INT32 dst_len, INT32 src_len)
{
	double scale = (double)dst_len / src_len;

	for (INT32 d = 0; d < dst_len; d++) {
		INT32 s = (INT32)(d / scale);
		if (s > src_len - 1) s = src_len - 1;
		double w = (s + 1) * scale - d;
		INT32 wi = (INT32)(w * 256.0 + 0.5);
		if (wi > 256 || s == src_len - 1) wi = 256;
		if (wi < 1) wi = 1;
		map[d] = s;
		weight[d] = wi;
	}
}

static INT32 DrvInit(INT32 io_bios, INT32 has_comm, INT32 boot_mirror)
{
	BurnSetRefreshRate((double)V60_CLOCK / (HTOTAL * VTOTAL));

	// the "Resolution" dip is read only here, when the game starts
	nHiresMode = DrvDips[3] & 7;
	if (nHiresMode > 4) nHiresMode = 0;
	bHiresSharp = nHiresMode && !(DrvDips[3] & 8);

	nIOBios = io_bios;
	bHasComm = has_comm;
	nBootMirror = boot_mirror;
	bHasEEPROMImage = 0;

	BurnAllocMemIndex();

	memset(DrvNVRAM, 0xff, 0x10000);	// vf needs it 1-filled for the rankings

	if (M1LoadRoms()) return 1;


	v60Init();
	v60Open(0);
	v60MapMemory(DrvV60ROM + 0x0000000,	0x000000, 0x0fffff, MAP_ROM);
	v60MapMemory(DrvV60ROM + 0x1000000,	0x100000, 0x1fffff, MAP_ROM);
	v60MapMemory(DrvV60ROM + 0x0200000,	0x200000, 0x2fffff, MAP_ROM);
	v60MapMemory(DrvNVRAM,				0x400000, 0x40ffff, MAP_RAM);
	v60MapMemory(DrvMainRAM,			0x500000, 0x53ffff, MAP_RAM);
	v60MapMemory(DrvDispList0,			0x600000, 0x60ffff, MAP_RAM);
	v60MapMemory(DrvDispList1,			0x610000, 0x61ffff, MAP_RAM);
	v60MapMemory(DrvTileRAM,			0x700000, 0x70ffff, MAP_ROM);	// writes are drawn up to the beam first
	v60MapMemory(DrvCharRAM,			0x780000, 0x7fffff, MAP_ROM);	// writes expand the tiles
	v60MapMemory(DrvPalRAM,				0x900000, 0x903fff, MAP_ROM);	// writes update the palette
	v60MapMemory(DrvXlatRAM,			0x910000, 0x91bfff, MAP_RAM);
	if (bHasComm) {
		v60MapMemory(DrvCommRAM,		0xb00000, 0xb00fff, MAP_RAM);
	}
	v60MapMemory(DrvV60ROM + 0x0f80000,	0xf80000, 0xffffff, MAP_ROM);
	v60SetWriteWordHandler(model1_write_word);
	v60SetWriteByteHandler(model1_write_byte);
	v60SetReadWordHandler(model1_read_word);
	v60SetReadByteHandler(model1_read_byte);
	v60SetIOWriteWordHandler(model1_io_write_word);
	v60SetIOWriteByteHandler(model1_io_write_byte);
	v60SetIOReadWordHandler(model1_io_read_word);
	v60SetIOReadByteHandler(model1_io_read_byte);
	v60SetIRQCallback(v60_irq_callback);
	v60Close();

	mb86233Init();
	mb86233SetProgram(DrvTGPROM, 0x7ff);
	mb86233SetDataHandlers(tgp_data_read, tgp_data_write);
	mb86233SetIOHandlers(tgp_io_read, tgp_io_write);

	M1AudioInit(0, Drv68KROM, Drv68KRAM, DrvPCMROM0, DrvPCMROM1, bHasDSB);

	M1IOInit(bIOBoard2, DrvIOROM, DrvIORAM, DrvDPRAM, DrvInputs, DrvDips, analog_values);
	M1IOSetHooks(io_port_in_hook, io_port_out_hook);

	if (bHasDSB) {
		DsbInit(1, DrvDSBROM, DrvDSBRAM, DrvDSBMPEG);
	}

	EEPROMInit(&eeprom_interface_93C46);
	if (EEPROMAvailable() == 0 && bHasEEPROMImage) {
		// 16-bit cells, the dump is little endian
		UINT8 tmp[0x80];
		for (INT32 i = 0; i < 0x80; i += 2) {
			tmp[i + 0] = DrvEEPROM[i + 1];
			tmp[i + 1] = DrvEEPROM[i + 0];
		}
		EEPROMFill(tmp, 0, 0x80);
	}

	// virtua racing's monitor setting (test mode, game system) is in the eeprom: 0 = 4:3, 1 = 16:9,
	// the races are then drawn anamorphic. A change takes effect when the game is started again.
	INT32 wide = bHasWideMonitor && (EEPROMReadByte(0x0b) == 1);
	if (wide) BurnDrvSetAspect(16, 9);	// restored by BurnDrvExit()

	if (nHiresMode) {
		static const INT32 hires_heights[5] = { NATIVE_H, 720, 1080, 1440, 2160 };
		INT32 hires_h = hires_heights[nHiresMode];
		INT32 hires_w = wide ? (hires_h * 16 / 9) : (hires_h * 4 / 3);
		BurnDrvSetVisibleSize(hires_w, hires_h);	// restored by BurnDrvExit()
		fHiresScaleX = (float)hires_w / NATIVE_W;
		fHiresScaleY = (float)hires_h / NATIVE_H;

		if (bHiresSharp) {
			hires_filter_map(HiresMapX, HiresWX, hires_w, NATIVE_W);
			hires_filter_map(HiresMapY, HiresWY, hires_h, NATIVE_H);

			for (INT32 c = 0, x = 0; c <= NATIVE_W / 16; c++) {
				while (x < hires_w && HiresMapX[x] < c * 16) x++;
				HiresChunkX[c] = x;
			}
			HiresChunkX[NATIVE_W / 16] = hires_w;
		} else {
			for (INT32 x = 0; x < hires_w; x++) {
				INT32 sx = (INT32)((x + 0.5f) / fHiresScaleX);
				HiresMapX[x] = (sx < NATIVE_W) ? sx : (NATIVE_W - 1);
			}
			for (INT32 y = 0; y < hires_h; y++) {
				INT32 sy = (INT32)((y + 0.5f) / fHiresScaleY);
				HiresMapY[y] = (sy < NATIVE_H) ? sy : (NATIVE_H - 1);
			}
		}
	}

	GenericTilesInit();
	Frame3D = (UINT16*)BurnMalloc(nScreenWidth * nScreenHeight * sizeof(UINT16));
	sega_315_5292_init(DrvTileRAM, DrvCharRAM, 0x80000, 0x3fff, NATIVE_W, NATIVE_H);
	render_threads_init();
	BurnTransferInit();

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	render_threads_exit();
	BurnFree(Frame3D);
	GenericTilesExit();
	sega_315_5292_exit();

	v60Exit();
	mb86233Exit();
	SekExit();
	ZetExit();

	M1AudioExit();
	EEPROMExit();

	if (bHasDSB) DsbExit();

	bHasDSB = 0;
	bIOBoard2 = 0;
	bHasR360 = 0;
	bHasHMD = 0;
	bHasDriveBoard = 0;
	bHasWideMonitor = 0;
	bHasMenuWheel = 0;
	nHiresMode = 0;
	nAnalogType = 0;

	BurnFreeMemIndex();

	return 0;
}

// the head tracker (polhemus board, not emulated) gives the camera pose at dpram 0x80-0x8b:
// x, y, z, yaw, roll, pitch, radians * 8192, forward is 0, 0, 0, pi/2, pi, pi/2
static void hmd_pose_write()
{
	INT32 yaw = 12868, pitch = 12868;

	if (bHasHMD > 1) {
		yaw += (ProcessAnalog(DrvAnalogPort4, 0, INPUT_DEADZONE, 0x00, 0xff) - 0x80) * 101;	// +-90 degrees
		pitch -= (ProcessAnalog(DrvAnalogPort5, 0, INPUT_DEADZONE, 0x00, 0xff) - 0x80) * 50;	// +-45 degrees
	}

	const INT16 pose[6] = { 0, 0, 0, (INT16)yaw, 25736, (INT16)pitch };
	for (INT32 i = 0; i < 6; i++) {
		DrvDPRAM[0x80 + i * 2] = pose[i] & 0xff;
		DrvDPRAM[0x81 + i * 2] = (pose[i] >> 8) & 0xff;
	}
}

static void DrvMakeInputs()
{
	memset(DrvInputs, 0xff, sizeof(DrvInputs));

	for (INT32 i = 0; i < 8; i++) {
		DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
		DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		DrvInputs[2] ^= (DrvJoy3[i] & 1) << i;
	}

	memset(analog_values, 0xff, sizeof(analog_values));

	switch (nAnalogType) {
		case 0:	// wheel, accelerator, brake
			analog_values[0] = ProcessAnalog(DrvAnalogPort0, 0, INPUT_DEADZONE, 0x00, 0xff);
			analog_values[1] = ProcessAnalog(DrvAnalogPort1, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x30, 0xff);
			analog_values[2] = ProcessAnalog(DrvAnalogPort2, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x30, 0xff);
			if (bHasMenuWheel) menu_wheel_update();
			break;

		case 2:	// two flight sticks, throttle (200 released, 28 full)
			analog_values[0] = ProcessAnalog(DrvAnalogPort0, 1, INPUT_DEADZONE, 27, 227);
			analog_values[1] = ProcessAnalog(DrvAnalogPort1, 0, INPUT_DEADZONE, 27, 227);
			analog_values[2] = 228 - ProcessAnalog(DrvAnalogPort2, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 28, 200);
			analog_values[4] = ProcessAnalog(DrvAnalogPort3, 1, INPUT_DEADZONE, 27, 227);
			analog_values[5] = ProcessAnalog(DrvAnalogPort4, 0, INPUT_DEADZONE, 27, 227);
			break;

		case 3:	// stick, throttle
			analog_values[0] = ProcessAnalog(DrvAnalogPort0, 1, INPUT_DEADZONE, 0x00, 0xff);
			analog_values[1] = ProcessAnalog(DrvAnalogPort1, 0, INPUT_DEADZONE, 0x00, 0xff);
			analog_values[2] = ProcessAnalog(DrvAnalogPort2, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x01, 0xff);
			break;

		case 4:	// stick, the throttle goes through the r360 control board
			analog_values[0] = ProcessAnalog(DrvAnalogPort0, 0, INPUT_DEADZONE, 0x00, 0xff);
			analog_values[1] = ProcessAnalog(DrvAnalogPort1, 1, INPUT_DEADZONE, 0x00, 0xff);
			analog_values[2] = 0;
			r360_throttle = ProcessAnalog(DrvAnalogPort2, 0, INPUT_DEADZONE | INPUT_LINEAR | INPUT_MIGHTBEDIGITAL, 0x01, 0xff);
			break;

		case 5:	// gun stick on channels 0 and 2
			analog_values[0] = ProcessAnalog(DrvAnalogPort0, 0, INPUT_DEADZONE, 0x00, 0xff);
			analog_values[2] = ProcessAnalog(DrvAnalogPort1, 1, INPUT_DEADZONE, 0x00, 0xff);
			break;
	}
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	DrvMakeInputs();

	if (bHasHMD > 1) hmd_pose_write();

	v60NewFrame();
	mb86233NewFrame();
	mb86233Idle(tgp_lead);
	SekNewFrame();
	ZetNewFrame();

	const INT32 nInterleave = VTOTAL;
	INT32 nCyclesTotal[4] = { V60_CYCLES_FRAME, (INT32)(((INT64)(bIOBoard2 ? IO2_CLOCK : IO_CLOCK) * V60_CYCLES_FRAME) / V60_CLOCK), (INT32)(((INT64)SND_CLOCK * V60_CYCLES_FRAME) / V60_CLOCK), (INT32)(((INT64)DSB_CLOCK * V60_CYCLES_FRAME) / V60_CLOCK) };
	INT32 nCyclesDone[4] = { 0, 0, 0, 0 };

	v60Open(0);
	SekOpen(0);
	ZetOpen(0);

	build_frame();
	if (pBurnDraw) draw_below_kick();

	tile_lines_done = 0;
	tile_skip = (pBurnDraw == NULL);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		if (i == VBLANK_LINE) {
			tile_draw_lines(NATIVE_H);
			end_frame();
			if (!(irq_mask & 0x02)) irq_raise(1);
		}

		if (v60_irq_dirty) update_v60_irq();

		INT32 v60_before = v60TotalCycles();
		CPU_RUN(0, v60);
		tgp_sync();
		timers_update(v60TotalCycles() - v60_before);

		CPU_RUN(1, Zet);

		// usarts, 500KHz clock
		INT32 ticks = (v60TotalCycles() / 32) - usart_ticks_done;
		usart_ticks_done += ticks;
		M1UsartAdvance(0, ticks);
		M1UsartAdvance(1, ticks);
		sound_ready_check();
		M1AudioIRQUpdate();

		if (bHasDSB) {
			M1UsartAdvance(2, ticks);
			ZetClose();
			ZetOpen(1);
			DsbIRQUpdate();
			CPU_RUN(3, Zet);
			ZetClose();
			ZetOpen(0);
		}

		BurnTimerUpdate((i + 1) * nCyclesTotal[2] / nInterleave);
	}

	BurnTimerEndFrame(nCyclesTotal[2]);

	// the tgp keeps what it ran ahead of the v60
	tgp_sync();
	tgp_lead = mb86233TotalCycles() - (INT32)(((INT64)v60TotalCycles() * 5) / 6);
	if (tgp_lead < 0) tgp_lead = 0;
	usart_ticks_done -= v60TotalCycles() / 32;

	if (pBurnSoundOut) {
		M1AudioUpdate(pBurnSoundOut, nBurnSoundLen);
	}

	if (bHasDSB) {
		DsbRender(pBurnSoundOut, nBurnSoundLen);
	}

	if (bHasDriveBoard) {
		drive_board_rumble();
	}

	ZetClose();
	SekClose();
	v60Close();

	build_pending = 1;
	frame_number++;

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin != NULL) {
		*pnMin = 0x029743;
	}

	if (nAction & ACB_MEMORY_RAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);
	}

	if (nAction & ACB_DRIVER_DATA) {
		v60Scan(nAction);
		mb86233Scan(nAction);
		SekScan(nAction);
		ZetScan(nAction);

		M1UsartScan(nAction);
		M1AudioScan(nAction, pnMin);
		EEPROMScan(nAction, pnMin);

		SCAN_VAR(irq_status);
		SCAN_VAR(irq_mask);
		SCAN_VAR(last_irq);
		SCAN_VAR(timer_period);
		SCAN_VAR(timer_val);
		SCAN_VAR(timer_remaining);
		SCAN_VAR(timer_mode);
		SCAN_VAR(rom_bank);
		SCAN_VAR(listctl);
		SCAN_VAR(frame_number);
		SCAN_VAR(v60_irq_dirty);

		SCAN_VAR(v60_copro_ram_adr);
		SCAN_VAR(v60_copro_ram_latch);
		SCAN_VAR(v60_copro_fifo_r);
		SCAN_VAR(v60_copro_fifo_w);
		SCAN_VAR(copro_sincos_base);
		SCAN_VAR(copro_inv_base);
		SCAN_VAR(copro_isqrt_base);
		SCAN_VAR(copro_atan_base);
		SCAN_VAR(copro_data_base);
		SCAN_VAR(copro_ram_adr);
		SCAN_VAR(fifo_in);
		SCAN_VAR(fifo_out);
		SCAN_VAR(tgp_wait_in);
		SCAN_VAR(tgp_wait_out);
		SCAN_VAR(tgp_lead);

		M1IOScan(nAction);

		SCAN_VAR(r360_state);

		SCAN_VAR(drive_vibrate);
		SCAN_VAR(menu_wheel_on);
		SCAN_VAR(menu_band);
		SCAN_VAR(menu_repeat);
		SCAN_VAR(menu_dir);
		SCAN_VAR(drive_slide);
		SCAN_VAR(drive_turn);
		SCAN_VAR(drive_crash);
		SCAN_VAR(drive_speed);
		SCAN_VAR(drive_slowing);
		SCAN_VAR(drive_contact);
		SCAN_VAR(drive_airborne);

		SCAN_VAR(comm_cn);
		SCAN_VAR(comm_fg);


		if (bHasDSB) DsbScan(nAction);
		SCAN_VAR(usart_ticks_done);

		SCAN_VAR(view);
		SCAN_VAR(build_pending);
		SCAN_VAR(frame3d_new);

		// for a redraw or the frame right after a load only
		if (!(nAction & (ACB_RUNAHEAD | ACB_NET_OPT))) {
			for (INT32 i = 0; i < 2; i++) {
				m1_build_input *b = &builds[i];
				ScanVar(b->list, 0x10000, (char*)"build list");
				ScanVar(b->pal + 0x1000, 0x2000, (char*)"build palette");
				ScanVar(b->xlat, 0xc000, (char*)"build color tables");
				ScanVar(&b->view, sizeof(m1_view), (char*)"build view");
				SCAN_VAR(b->listctl0);
				SCAN_VAR(b->frame);
				SCAN_VAR(b->valid);
			}
		}
		if (!(nAction & (ACB_RUNAHEAD | ACB_2RUNAHEAD | ACB_NET_OPT))) {
			ScanVar(TileBack, NATIVE_W * NATIVE_H * sizeof(UINT16), (char*)"tilemaps back");
			ScanVar(TileHud, NATIVE_W * NATIVE_H * sizeof(UINT16), (char*)"tilemaps hud");
			SCAN_VAR(tile_lines_done);
		}
	}

	if (nAction & ACB_NVRAM) {
		memset(&ba, 0, sizeof(ba));
		ba.Data	  = DrvNVRAM;
		ba.nLen	  = 0x10000;
		ba.szName = "NV RAM";
		BurnAcb(&ba);
	}

	if (nAction & ACB_WRITE) {
		v60Open(0);
		bankswitch(0x01 | (rom_bank << 4));
		v60Close();

		sega_315_5292_recalc();

		DrvRecalc = 1;

		if (nAction & ACB_DRIVER_DATA) {
			frame3d_lost[0] = frame3d_lost[1] = 1;
			if (nAction & (ACB_RUNAHEAD | ACB_NET_OPT)) builds[0].valid = builds[1].valid = 0;
			if (nAction & (ACB_RUNAHEAD | ACB_2RUNAHEAD | ACB_NET_OPT)) tile_lines_done = 0;
		}
	}

	return 0;
}


// Model 1 I/O board BIOS

static struct BurnRomInfo emptyRomDesc[] = {
	{ "",                    0,          0, 0 },
};

static struct BurnRomInfo model1ioRomDesc[] = {
	{ "epr-14869.25",		0x10000, 0x6187cd7a, BRF_BIOS | BRF_ESS },	//  0 Virtua Racing
	{ "epr-14869b.25",		0x10000, 0x2d093304, BRF_BIOS | BRF_OPT },	//  1 Virtua Fighter, Star Wars Arcade
	{ "epr-14869c.25",		0x10000, 0x24b68e64, BRF_BIOS | BRF_OPT },	//  2 Daytona USA
};

STD_ROM_PICK(model1io)
STD_ROM_FN(model1io)

struct BurnDriver BurnDrvModel1io = {
	"model1io", NULL, NULL, NULL, "1992",
	"Sega Model 1 I/O Board BIOS\0", "BIOS only", "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SEGA_MISC, GBF_BIOS, 0,
	NULL, model1ioRomInfo, model1ioRomName, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, 0,
	496, 384, 4, 3
};


// Model 1 I/O Board (Advanced) BIOS

static struct BurnRomInfo model1io2RomDesc[] = {
	{ "epr-16891.6",		0x10000, 0xa33f84d1, BRF_BIOS | BRF_ESS },	//  0 Wing War
	{ "epr-18021.6",		0x10000, 0x5551837e, BRF_BIOS | BRF_OPT },	//  1 NetMerc
	{ "epr-17181.6",		0x10000, 0x1add2b82, BRF_BIOS | BRF_OPT },	//  2 Virtua Cop
};

STD_ROM_PICK(model1io2)
STD_ROM_FN(model1io2)

struct BurnDriver BurnDrvModel1io2 = {
	"model1io2", NULL, NULL, NULL, "1994",
	"Sega Model 1 I/O Board (Advanced) BIOS\0", "BIOS only", "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_BOARDROM, 0, HARDWARE_SEGA_MISC, GBF_BIOS, 0,
	NULL, model1io2RomInfo, model1io2RomName, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, 0,
	496, 384, 4, 3
};


// Virtua Racing

static struct BurnRomInfo vrRomDesc[] = {
	{ "epr-14882.14",        0x080000, 0x547d75ad, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-14883.15",        0x080000, 0x6bfad8b1, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-14878a.4",        0x020000, 0x6d69e695, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "epr-14879a.5",        0x020000, 0xd45af9dd, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "mpr-14880.6",         0x080000, 0xadc7c208, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "mpr-14881.7",         0x080000, 0xe5ab89df, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "mpr-14884.8",         0x080000, 0x6cf9c026, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "mpr-14885.9",         0x080000, 0xf65c9262, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "mpr-14886.10",        0x080000, 0x92868734, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "mpr-14887.11",        0x080000, 0x10c7c636, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "mpr-14888.12",        0x080000, 0x04bfdc5b, M1_DATA | BRF_PRG | BRF_ESS },	// 10
	{ "mpr-14889.13",        0x080000, 0xc49f0486, M1_DATA | BRF_PRG | BRF_ESS },	// 11
	{ "315-5573.bin",        0x002000, 0x3335a19b, M1_TGP | BRF_PRG | BRF_ESS },	// 12 TGP Program
	{ "epr-14870a.7",        0x020000, 0x919d9b75, M1_68K | BRF_PRG | BRF_ESS },	// 13 68K Code
	{ "mpr-14873.32",        0x200000, 0xb1965190, M1_PCM0 | BRF_SND          },	// 14 MultiPCM #1 Samples
	{ "mpr-14876.4",         0x200000, 0xba6b2327, M1_PCM1 | BRF_SND          },	// 15 MultiPCM #2 Samples
	{ "mpr-14890.26",        0x200000, 0xdcbe006b, M1_POLY | BRF_GRA          },	// 16 Polygon Models
	{ "mpr-14891.27",        0x200000, 0x25832b38, M1_POLY | BRF_GRA          },	// 17
	{ "mpr-14892.28",        0x200000, 0x5136f3ba, M1_POLY | BRF_GRA          },	// 18
	{ "mpr-14893.29",        0x200000, 0x1c531ada, M1_POLY | BRF_GRA          },	// 19
	{ "mpr-14894.30",        0x200000, 0x830a71bc, M1_POLY | BRF_GRA          },	// 20
	{ "mpr-14895.31",        0x200000, 0xaf027ac5, M1_POLY | BRF_GRA          },	// 21
	{ "mpr-14896.32",        0x200000, 0x382091dc, M1_POLY | BRF_GRA          },	// 22
	{ "mpr-14897.33",        0x200000, 0x74873195, M1_POLY | BRF_GRA          },	// 23
	{ "mpr-14898.39",        0x080000, 0x61da2bb6, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 24 TGP Data
	{ "mpr-14899.40",        0x080000, 0x2cd58bee, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 25
	{ "mpr-14900.41",        0x080000, 0xaa7c017d, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 26
	{ "mpr-14901.42",        0x080000, 0x175b7a9a, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 27
	{ "93c45.bin",           0x000080, 0x65aac303, M1_EEPROM | BRF_PRG | BRF_ESS },	// 28 I/O Board EEPROM
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 29 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 30
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 31 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 32
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 33
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 34
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 35
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 36
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 37
};

STDROMPICKEXT(vr, vr, model1io)
STD_ROM_FN(vr)

static INT32 VrInit()
{
	bHasDriveBoard = 1;
	bHasWideMonitor = 1;
	bHasMenuWheel = 1;

	return DrvInit(0, 1, 0);
}

static INT32 VformulaInit()
{
	bHasDriveBoard = 1;

	return DrvInit(0, 1, 0);
}

struct BurnDriver BurnDrvVr = {
	"vr", NULL, "model1io", NULL, "1992",
	"Virtua Racing\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, vrRomInfo, vrRomName, NULL, NULL, NULL, NULL, VrInputInfo, VrDIPInfo,
	VrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};

// Virtua Racing Extended: courses converted from the Saturn port, reverse courses, weather
// (https://github.com/ppeltriaux/virtua-racing-extended), built from the vr set and the
// European Saturn disc: other versions or build options (--tune, --no-weather) change the crcs

static struct BurnRomInfo vrextRomDesc[] = {
	{ "vrext-14882.14",      0x080000, 0xc54769a9, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "vrext-14883.15",      0x080000, 0x6135c451, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "vrext-14878a.4",      0x020000, 0x49af3025, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "vrext-14879a.5",      0x020000, 0xb8d172ba, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "vrext-14880.6",       0x080000, 0x1b068638, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "vrext-14881.7",       0x080000, 0x4608136f, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "vrext-14884.8",       0x080000, 0xefd48c7a, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "vrext-14885.9",       0x080000, 0x95fa1bd1, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "vrext-14886.10",      0x080000, 0x4505638c, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "vrext-14887.11",      0x080000, 0x6efa188a, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "vrext-14888.12",      0x080000, 0x762b3bde, M1_DATA | BRF_PRG | BRF_ESS },	// 10
	{ "vrext-14889.13",      0x080000, 0x41c6488d, M1_DATA | BRF_PRG | BRF_ESS },	// 11
	{ "315-5573.bin",        0x002000, 0x3335a19b, M1_TGP | BRF_PRG | BRF_ESS },	// 12 TGP Program
	{ "vrext-14870a.7",      0x020000, 0x8c3e6961, M1_68K | BRF_PRG | BRF_ESS },	// 13 68K Code
	{ "vrext-14873.32",      0x200000, 0x048048be, M1_PCM0 | BRF_SND          },	// 14 MultiPCM #1 Samples
	{ "vrext-14876.4",       0x200000, 0x239c2bee, M1_PCM1 | BRF_SND          },	// 15 MultiPCM #2 Samples
	{ "vrext-14890.26",      0x200000, 0x41233f25, M1_POLY | BRF_GRA          },	// 16 Polygon Models
	{ "vrext-14891.27",      0x200000, 0x0e8f89f6, M1_POLY | BRF_GRA          },	// 17
	{ "vrext-14892.28",      0x200000, 0xdbf3877a, M1_POLY | BRF_GRA          },	// 18
	{ "vrext-14893.29",      0x200000, 0x470209de, M1_POLY | BRF_GRA          },	// 19
	{ "vrext-14894.30",      0x200000, 0xe19d175c, M1_POLY | BRF_GRA          },	// 20
	{ "vrext-14895.31",      0x200000, 0x1be3c79b, M1_POLY | BRF_GRA          },	// 21
	{ "vrext-14896.32",      0x200000, 0x5759850c, M1_POLY | BRF_GRA          },	// 22
	{ "vrext-14897.33",      0x200000, 0x6ec9f9c4, M1_POLY | BRF_GRA          },	// 23
	{ "vrext-14898.39",      0x080000, 0x1c419411, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 24 TGP Data
	{ "vrext-14899.40",      0x080000, 0x0b936e73, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 25
	{ "vrext-14900.41",      0x080000, 0x2e11115c, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 26
	{ "vrext-14901.42",      0x080000, 0x1218eac7, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 27
	{ "93c45.bin",           0x000080, 0x65aac303, M1_EEPROM | BRF_PRG | BRF_ESS },	// 28 I/O Board EEPROM
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 29 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 30
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 31 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 32
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 33
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 34
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 35
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 36
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 37
};

STDROMPICKEXT(vrext, vrext, model1io)
STD_ROM_FN(vrext)

static INT32 VrextInit()
{
	INT32 rc = VrInit();
	bHasMenuWheel = 2;

	return rc;
}

struct BurnDriver BurnDrvVrext = {
	"vrext", "vr", "model1io", NULL, "2026",
	"Virtua Racing Extended (v0.4)\0", NULL, "hack (ppeltriaux)", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_HACK, 1, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, vrextRomInfo, vrextRomName, NULL, NULL, NULL, NULL, VrInputInfo, VrDIPInfo,
	VrextInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};


// Virtua Formula

static struct BurnRomInfo vformulaRomDesc[] = {
	{ "epr-15638.14",        0x080000, 0xb9db21a2, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-15639.15",        0x080000, 0x4c3796f5, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-15623.4",         0x020000, 0x155fa5be, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "epr-15622.5",         0x020000, 0x18007f6f, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "epr-15641.6",         0x080000, 0xbf01e4d5, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "epr-15640.7",         0x080000, 0x3e6d83dc, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "mpr-14884.8",         0x080000, 0x6cf9c026, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "mpr-14885.9",         0x080000, 0xf65c9262, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "mpr-14886.10",        0x080000, 0x92868734, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "mpr-14887.11",        0x080000, 0x10c7c636, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "mpr-14888.12",        0x080000, 0x04bfdc5b, M1_DATA | BRF_PRG | BRF_ESS },	// 10
	{ "mpr-14889.13",        0x080000, 0xc49f0486, M1_DATA | BRF_PRG | BRF_ESS },	// 11
	{ "315-5573.bin",        0x002000, 0x3335a19b, M1_TGP | BRF_PRG | BRF_ESS },	// 12 TGP Program
	{ "epr-14870a.7",        0x020000, 0x919d9b75, M1_68K | BRF_PRG | BRF_ESS },	// 13 68K Code
	{ "mpr-14873.32",        0x200000, 0xb1965190, M1_PCM0 | BRF_SND          },	// 14 MultiPCM #1 Samples
	{ "mpr-14876.4",         0x200000, 0xba6b2327, M1_PCM1 | BRF_SND          },	// 15 MultiPCM #2 Samples
	{ "mpr-14890.26",        0x200000, 0xdcbe006b, M1_POLY | BRF_GRA          },	// 16 Polygon Models
	{ "mpr-14891.27",        0x200000, 0x25832b38, M1_POLY | BRF_GRA          },	// 17
	{ "mpr-14892.28",        0x200000, 0x5136f3ba, M1_POLY | BRF_GRA          },	// 18
	{ "mpr-14893.29",        0x200000, 0x1c531ada, M1_POLY | BRF_GRA          },	// 19
	{ "mpr-14894.30",        0x200000, 0x830a71bc, M1_POLY | BRF_GRA          },	// 20
	{ "mpr-14895.31",        0x200000, 0xaf027ac5, M1_POLY | BRF_GRA          },	// 21
	{ "mpr-14896.32",        0x200000, 0x382091dc, M1_POLY | BRF_GRA          },	// 22
	{ "mpr-14897.33",        0x200000, 0x74873195, M1_POLY | BRF_GRA          },	// 23
	{ "mpr-14898.39",        0x080000, 0x61da2bb6, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 24 TGP Data
	{ "mpr-14899.40",        0x080000, 0x2cd58bee, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 25
	{ "mpr-14900.41",        0x080000, 0xaa7c017d, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 26
	{ "mpr-14901.42",        0x080000, 0x175b7a9a, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 27
	{ "epr-15624.17",        0x020000, 0x9b3ba315, 0 | BRF_OPT                },	// 28 Unused
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 29 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 30
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 31 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 32
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 33
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 34
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 35
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 36
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 37
};

STDROMPICKEXT(vformula, vformula, model1io)
STD_ROM_FN(vformula)

struct BurnDriver BurnDrvVformula = {
	"vformula", "vr", "model1io", NULL, "1993",
	"Virtua Formula\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_MISC, GBF_RACING, 0,
	NULL, vformulaRomInfo, vformulaRomName, NULL, NULL, NULL, NULL, VrInputInfo, VformulaDIPInfo,
	VformulaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};


// Virtua Fighter

static struct BurnRomInfo vfRomDesc[] = {
	{ "epr-16082.14",        0x080000, 0xb23f22ee, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-16083.15",        0x080000, 0xd12c77f8, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-16080.4",         0x020000, 0x3662e1a5, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "epr-16081.5",         0x020000, 0x6dec06ce, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "mpr-16084.6",         0x080000, 0x483f453b, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "mpr-16085.7",         0x080000, 0x5fa01277, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "mpr-16086.8",         0x080000, 0xdeac47a1, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "mpr-16087.9",         0x080000, 0x7a64daac, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "mpr-16088.10",        0x080000, 0xfcda2d1e, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "mpr-16089.11",        0x080000, 0x39befbe0, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "mpr-16090.12",        0x080000, 0x90c76831, M1_DATA | BRF_PRG | BRF_ESS },	// 10
	{ "mpr-16091.13",        0x080000, 0x53115448, M1_DATA | BRF_PRG | BRF_ESS },	// 11
	{ "315-5724.bin",        0x002000, 0x4b4f330e, M1_TGP | BRF_PRG | BRF_ESS },	// 12 TGP Program
	{ "epr-16120.7",         0x020000, 0x2bff8378, M1_68K | BRF_PRG | BRF_ESS },	// 13 68K Code
	{ "epr-16121.8",         0x020000, 0xff6723f9, M1_68K | BRF_PRG | BRF_ESS },	// 14
	{ "mpr-16122.32",        0x200000, 0x568bc64e, M1_PCM0 | BRF_SND          },	// 15 MultiPCM #1 Samples
	{ "mpr-16123.33",        0x200000, 0x15d78844, M1_PCM0 | BRF_SND          },	// 16
	{ "mpr-16124.4",         0x200000, 0x45520ba1, M1_PCM1 | BRF_SND          },	// 17 MultiPCM #2 Samples
	{ "mpr-16125.5",         0x200000, 0x9b4998b6, M1_PCM1 | BRF_SND          },	// 18
	{ "mpr-16096.26",        0x200000, 0xa92b0bf3, M1_POLY | BRF_GRA          },	// 19 Polygon Models
	{ "mpr-16097.27",        0x200000, 0x0232955a, M1_POLY | BRF_GRA          },	// 20
	{ "mpr-16098.28",        0x200000, 0xcf2e1b84, M1_POLY | BRF_GRA          },	// 21
	{ "mpr-16099.29",        0x200000, 0x20e46854, M1_POLY | BRF_GRA          },	// 22
	{ "mpr-16100.30",        0x200000, 0xe13e983d, M1_POLY | BRF_GRA          },	// 23
	{ "mpr-16101.31",        0x200000, 0x0dbed94d, M1_POLY | BRF_GRA          },	// 24
	{ "mpr-16102.32",        0x200000, 0x4cb41fb6, M1_POLY | BRF_GRA          },	// 25
	{ "mpr-16103.33",        0x200000, 0x526d1c76, M1_POLY | BRF_GRA          },	// 26
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 27 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 28
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 29 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 30
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 31
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 32
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 33
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 34
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 35
};

STDROMPICKEXT(vf, vf, model1io)
STD_ROM_FN(vf)

static INT32 VfInit()
{
	nAnalogType = 1;

	return DrvInit(1, 0, 0);
}

struct BurnDriver BurnDrvVf = {
	"vf", NULL, "model1io", NULL, "1993",
	"Virtua Fighter\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_VSFIGHT, 0,
	NULL, vfRomInfo, vfRomName, NULL, NULL, NULL, NULL, VfInputInfo, VfDIPInfo,
	VfInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};


// Star Wars (Sega, US)

static struct BurnRomInfo swaRomDesc[] = {
	{ "epr-16669.14",        0x080000, 0x52e5e7e4, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-16670.15",        0x080000, 0x1e7ecabd, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-16668.5",         0x080000, 0x9e112425, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "315-5711.bin",        0x002000, 0x6a21f304, M1_TGP | BRF_PRG | BRF_ESS },	//  3 TGP Program
	{ "epr-16470.7",         0x020000, 0x7da18cf7, M1_68K | BRF_PRG | BRF_ESS },	//  4 68K Code
	{ "mpr-16486.32",        0x200000, 0x7df50533, M1_PCM0 | BRF_SND          },	//  5 MultiPCM #1 Samples
	{ "mpr-16487.33",        0x200000, 0x31b28dfa, M1_PCM0 | BRF_SND          },	//  6
	{ "mpr-16484.4",         0x200000, 0x9d4c334d, M1_PCM1 | BRF_SND          },	//  7 MultiPCM #2 Samples
	{ "mpr-16485.5",         0x200000, 0x95aadcad, M1_PCM1 | BRF_SND          },	//  8
	{ "epr-16471.2",         0x020000, 0xf4ee84a4, M1_DSBPRG | BRF_PRG | BRF_ESS },	//  9 DSB Z80 Code
	{ "mpr-16514.57",        0x200000, 0x3175b0be, M1_DSBDATA | BRF_SND       },	// 10 DSB MPEG Data
	{ "mpr-16515.58",        0x200000, 0x3114d748, M1_DSBDATA | BRF_SND       },	// 11
	{ "mpr-16476.26",        0x200000, 0xd48609ae, M1_POLY | BRF_GRA          },	// 12 Polygon Models
	{ "mpr-16477.27",        0x200000, 0xb979b082, M1_POLY | BRF_GRA          },	// 13
	{ "mpr-16478.28",        0x200000, 0x80c780f7, M1_POLY | BRF_GRA          },	// 14
	{ "mpr-16479.29",        0x200000, 0xe43183b3, M1_POLY | BRF_GRA          },	// 15
	{ "mpr-16480.30",        0x200000, 0x3185547a, M1_POLY | BRF_GRA          },	// 16
	{ "mpr-16481.31",        0x200000, 0xce8d76fe, M1_POLY | BRF_GRA          },	// 17
	{ "mpr-16472.39",        0x080000, 0x5a0d7553, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 18 TGP Data
	{ "mpr-16473.40",        0x080000, 0x876c5399, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 19
	{ "mpr-16474.41",        0x080000, 0x5864a26f, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 20
	{ "mpr-16475.42",        0x080000, 0xb9266be9, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 21
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 22 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 23
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 24 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 25
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 26
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 27
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 28
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 29
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 30
};

STDROMPICKEXT(swa, swa, model1io)
STD_ROM_FN(swa)

static INT32 SwaInit()
{
	nAnalogType = 2;
	bHasDSB = 1;

	return DrvInit(1, 0, 2);
}

struct BurnDriver BurnDrvSwa = {
	"swa", NULL, "model1io", NULL, "1994",
	"Star Wars (Sega, US)\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, swaRomInfo, swaRomName, NULL, NULL, NULL, NULL, SwaInputInfo, SwaDIPInfo,
	SwaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};


// Star Wars (Sega, Japan)

static struct BurnRomInfo swajRomDesc[] = {
	{ "epr-16468.14",        0x080000, 0x681d03c0, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-16469.15",        0x080000, 0x6f281f7c, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-16467.5",         0x080000, 0x605068f5, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "315-5711.bin",        0x002000, 0x6a21f304, M1_TGP | BRF_PRG | BRF_ESS },	//  3 TGP Program
	{ "epr-16470.7",         0x020000, 0x7da18cf7, M1_68K | BRF_PRG | BRF_ESS },	//  4 68K Code
	{ "mpr-16486.32",        0x200000, 0x7df50533, M1_PCM0 | BRF_SND          },	//  5 MultiPCM #1 Samples
	{ "mpr-16487.33",        0x200000, 0x31b28dfa, M1_PCM0 | BRF_SND          },	//  6
	{ "mpr-16484.4",         0x200000, 0x9d4c334d, M1_PCM1 | BRF_SND          },	//  7 MultiPCM #2 Samples
	{ "mpr-16485.5",         0x200000, 0x95aadcad, M1_PCM1 | BRF_SND          },	//  8
	{ "epr-16471.2",         0x020000, 0xf4ee84a4, M1_DSBPRG | BRF_PRG | BRF_ESS },	//  9 DSB Z80 Code
	{ "mpr-16514.57",        0x200000, 0x3175b0be, M1_DSBDATA | BRF_SND       },	// 10 DSB MPEG Data
	{ "mpr-16515.58",        0x200000, 0x3114d748, M1_DSBDATA | BRF_SND       },	// 11
	{ "mpr-16476.26",        0x200000, 0xd48609ae, M1_POLY | BRF_GRA          },	// 12 Polygon Models
	{ "mpr-16477.27",        0x200000, 0xb979b082, M1_POLY | BRF_GRA          },	// 13
	{ "mpr-16478.28",        0x200000, 0x80c780f7, M1_POLY | BRF_GRA          },	// 14
	{ "mpr-16479.29",        0x200000, 0xe43183b3, M1_POLY | BRF_GRA          },	// 15
	{ "mpr-16480.30",        0x200000, 0x3185547a, M1_POLY | BRF_GRA          },	// 16
	{ "mpr-16481.31",        0x200000, 0xce8d76fe, M1_POLY | BRF_GRA          },	// 17
	{ "mpr-16472.39",        0x080000, 0x5a0d7553, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 18 TGP Data
	{ "mpr-16473.40",        0x080000, 0x876c5399, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 19
	{ "mpr-16474.41",        0x080000, 0x5864a26f, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 20
	{ "mpr-16475.42",        0x080000, 0xb9266be9, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 21
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 22 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 23
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 24 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 25
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 26
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 27
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 28
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 29
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 30
};

STDROMPICKEXT(swaj, swaj, model1io)
STD_ROM_FN(swaj)

struct BurnDriver BurnDrvSwaj = {
	"swaj", "swa", "model1io", NULL, "1994",
	"Star Wars (Sega, Japan)\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, swajRomInfo, swajRomName, NULL, NULL, NULL, NULL, SwaInputInfo, SwaDIPInfo,
	SwaInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};

static INT32 WingwarInit()
{
	nAnalogType = 3;
	bIOBoard2 = 1;

	return DrvInit(0, 1, 1);
}

static INT32 Wingwar360Init()
{
	nAnalogType = 4;
	bIOBoard2 = 1;
	bHasR360 = 1;

	return DrvInit(0, 1, 1);
}

// Wing War (World)

static struct BurnRomInfo wingwarRomDesc[] = {
	{ "epr-16729.14",        0x080000, 0x7edec2cc, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-16730.15",        0x080000, 0xbab24dee, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-16953.4",         0x020000, 0xc821a920, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "epr-16952.5",         0x020000, 0x03a3ecc5, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "mpr-16738.6",         0x080000, 0x51518ffa, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "mpr-16737.7",         0x080000, 0x37b1379c, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "mpr-16736.8",         0x080000, 0x10b6a025, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "mpr-16735.9",         0x080000, 0xc82fd198, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "mpr-16734.10",        0x080000, 0xf76371c1, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "mpr-16733.11",        0x080000, 0xe105847b, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "315-5711.bin",        0x002000, 0x6a21f304, M1_TGP | BRF_PRG | BRF_ESS },	// 10 TGP Program
	{ "epr-17126.7",         0x020000, 0x50178e40, M1_68K | BRF_PRG | BRF_ESS },	// 11 68K Code
	{ "epr-16752.8",         0x020000, 0x6541c48f, M1_68K | BRF_PRG | BRF_ESS },	// 12
	{ "mpr-16753.32",        0x200000, 0x324a8333, M1_PCM0 | BRF_SND          },	// 13 MultiPCM #1 Samples
	{ "mpr-16754.33",        0x200000, 0x144f3cf5, M1_PCM0 | BRF_SND          },	// 14
	{ "mpr-16755.4",         0x200000, 0x4baaf878, M1_PCM1 | BRF_SND          },	// 15 MultiPCM #2 Samples
	{ "mpr-16756.5",         0x200000, 0xd9c40672, M1_PCM1 | BRF_SND          },	// 16
	{ "mpr-16743.26",        0x200000, 0xa710d33c, M1_POLY | BRF_GRA          },	// 17 Polygon Models
	{ "mpr-16744.27",        0x200000, 0xde796e1f, M1_POLY | BRF_GRA          },	// 18
	{ "mpr-16745.28",        0x200000, 0x905b689c, M1_POLY | BRF_GRA          },	// 19
	{ "mpr-16746.29",        0x200000, 0x163b312e, M1_POLY | BRF_GRA          },	// 20
	{ "mpr-16747.30",        0x200000, 0x7353bb12, M1_POLY | BRF_GRA          },	// 21
	{ "mpr-16748.31",        0x200000, 0x8ce98d3a, M1_POLY | BRF_GRA          },	// 22
	{ "mpr-16749.32",        0x200000, 0x0e36dc1a, M1_POLY | BRF_GRA          },	// 23
	{ "mpr-16750.33",        0x200000, 0xe4f0b98d, M1_POLY | BRF_GRA          },	// 24
	{ "mpr-16741.39",        0x080000, 0x84b2ffd8, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 25 TGP Data
	{ "mpr-16742.40",        0x080000, 0xe9cc12bb, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 26
	{ "mpr-16739.41",        0x080000, 0x6c73e98f, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 27
	{ "mpr-16740.42",        0x080000, 0x44b31007, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 28
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 29 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 30
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 31 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 32
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 33
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 34
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 35
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 36
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 37
};

STDROMPICKEXT(wingwar, wingwar, model1io2)
STD_ROM_FN(wingwar)

struct BurnDriver BurnDrvWingwar = {
	"wingwar", NULL, "model1io2", NULL, "1994",
	"Wing War (World)\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, wingwarRomInfo, wingwarRomName, NULL, NULL, NULL, NULL, WingwarInputInfo, WingwarDIPInfo,
	WingwarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};

// Wing War (US)

static struct BurnRomInfo wingwaruRomDesc[] = {
	{ "epr-16729.14",        0x080000, 0x7edec2cc, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-16730.15",        0x080000, 0xbab24dee, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-16951.4",         0x020000, 0xfbe12d15, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "epr-16950.5",         0x020000, 0x841e2195, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "mpr-16738.6",         0x080000, 0x51518ffa, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "mpr-16737.7",         0x080000, 0x37b1379c, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "mpr-16736.8",         0x080000, 0x10b6a025, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "mpr-16735.9",         0x080000, 0xc82fd198, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "mpr-16734.10",        0x080000, 0xf76371c1, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "mpr-16733.11",        0x080000, 0xe105847b, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "315-5711.bin",        0x002000, 0x6a21f304, M1_TGP | BRF_PRG | BRF_ESS },	// 10 TGP Program
	{ "epr-16751.7",         0x020000, 0x23ba5ebc, M1_68K | BRF_PRG | BRF_ESS },	// 11 68K Code
	{ "epr-16752.8",         0x020000, 0x6541c48f, M1_68K | BRF_PRG | BRF_ESS },	// 12
	{ "mpr-16753.32",        0x200000, 0x324a8333, M1_PCM0 | BRF_SND          },	// 13 MultiPCM #1 Samples
	{ "mpr-16754.33",        0x200000, 0x144f3cf5, M1_PCM0 | BRF_SND          },	// 14
	{ "mpr-16755.4",         0x200000, 0x4baaf878, M1_PCM1 | BRF_SND          },	// 15 MultiPCM #2 Samples
	{ "mpr-16756.5",         0x200000, 0xd9c40672, M1_PCM1 | BRF_SND          },	// 16
	{ "mpr-16743.26",        0x200000, 0xa710d33c, M1_POLY | BRF_GRA          },	// 17 Polygon Models
	{ "mpr-16744.27",        0x200000, 0xde796e1f, M1_POLY | BRF_GRA          },	// 18
	{ "mpr-16745.28",        0x200000, 0x905b689c, M1_POLY | BRF_GRA          },	// 19
	{ "mpr-16746.29",        0x200000, 0x163b312e, M1_POLY | BRF_GRA          },	// 20
	{ "mpr-16747.30",        0x200000, 0x7353bb12, M1_POLY | BRF_GRA          },	// 21
	{ "mpr-16748.31",        0x200000, 0x8ce98d3a, M1_POLY | BRF_GRA          },	// 22
	{ "mpr-16749.32",        0x200000, 0x0e36dc1a, M1_POLY | BRF_GRA          },	// 23
	{ "mpr-16750.33",        0x200000, 0xe4f0b98d, M1_POLY | BRF_GRA          },	// 24
	{ "mpr-16741.39",        0x080000, 0x84b2ffd8, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 25 TGP Data
	{ "mpr-16742.40",        0x080000, 0xe9cc12bb, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 26
	{ "mpr-16739.41",        0x080000, 0x6c73e98f, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 27
	{ "mpr-16740.42",        0x080000, 0x44b31007, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 28
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 29 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 30
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 31 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 32
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 33
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 34
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 35
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 36
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 37
};

STDROMPICKEXT(wingwaru, wingwaru, model1io2)
STD_ROM_FN(wingwaru)

struct BurnDriver BurnDrvWingwaru = {
	"wingwaru", "wingwar", "model1io2", NULL, "1994",
	"Wing War (US)\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, wingwaruRomInfo, wingwaruRomName, NULL, NULL, NULL, NULL, WingwarInputInfo, WingwarDIPInfo,
	WingwarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};

// Wing War (Japan)

static struct BurnRomInfo wingwarjRomDesc[] = {
	{ "epr-16729.14",        0x080000, 0x7edec2cc, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-16730.15",        0x080000, 0xbab24dee, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-16728.4",         0x020000, 0x194d58ff, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "epr-16727.5",         0x020000, 0x430380eb, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "mpr-16738.6",         0x080000, 0x51518ffa, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "mpr-16737.7",         0x080000, 0x37b1379c, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "mpr-16736.8",         0x080000, 0x10b6a025, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "mpr-16735.9",         0x080000, 0xc82fd198, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "mpr-16734.10",        0x080000, 0xf76371c1, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "mpr-16733.11",        0x080000, 0xe105847b, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "315-5711.bin",        0x002000, 0x6a21f304, M1_TGP | BRF_PRG | BRF_ESS },	// 10 TGP Program
	{ "epr-16751.7",         0x020000, 0x23ba5ebc, M1_68K | BRF_PRG | BRF_ESS },	// 11 68K Code
	{ "epr-16752.8",         0x020000, 0x6541c48f, M1_68K | BRF_PRG | BRF_ESS },	// 12
	{ "mpr-16753.32",        0x200000, 0x324a8333, M1_PCM0 | BRF_SND          },	// 13 MultiPCM #1 Samples
	{ "mpr-16754.33",        0x200000, 0x144f3cf5, M1_PCM0 | BRF_SND          },	// 14
	{ "mpr-16755.4",         0x200000, 0x4baaf878, M1_PCM1 | BRF_SND          },	// 15 MultiPCM #2 Samples
	{ "mpr-16756.5",         0x200000, 0xd9c40672, M1_PCM1 | BRF_SND          },	// 16
	{ "mpr-16743.26",        0x200000, 0xa710d33c, M1_POLY | BRF_GRA          },	// 17 Polygon Models
	{ "mpr-16744.27",        0x200000, 0xde796e1f, M1_POLY | BRF_GRA          },	// 18
	{ "mpr-16745.28",        0x200000, 0x905b689c, M1_POLY | BRF_GRA          },	// 19
	{ "mpr-16746.29",        0x200000, 0x163b312e, M1_POLY | BRF_GRA          },	// 20
	{ "mpr-16747.30",        0x200000, 0x7353bb12, M1_POLY | BRF_GRA          },	// 21
	{ "mpr-16748.31",        0x200000, 0x8ce98d3a, M1_POLY | BRF_GRA          },	// 22
	{ "mpr-16749.32",        0x200000, 0x0e36dc1a, M1_POLY | BRF_GRA          },	// 23
	{ "mpr-16750.33",        0x200000, 0xe4f0b98d, M1_POLY | BRF_GRA          },	// 24
	{ "mpr-16741.39",        0x080000, 0x84b2ffd8, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 25 TGP Data
	{ "mpr-16742.40",        0x080000, 0xe9cc12bb, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 26
	{ "mpr-16739.41",        0x080000, 0x6c73e98f, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 27
	{ "mpr-16740.42",        0x080000, 0x44b31007, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 28
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 29 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 30
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 31 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 32
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 33
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 34
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 35
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 36
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 37
};

STDROMPICKEXT(wingwarj, wingwarj, model1io2)
STD_ROM_FN(wingwarj)

struct BurnDriver BurnDrvWingwarj = {
	"wingwarj", "wingwar", "model1io2", NULL, "1994",
	"Wing War (Japan)\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, wingwarjRomInfo, wingwarjRomName, NULL, NULL, NULL, NULL, WingwarInputInfo, WingwarDIPInfo,
	WingwarInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};

// Wing War R360 (US)

static struct BurnRomInfo wingwar360RomDesc[] = {
	{ "epr-16729.14",        0x080000, 0x7edec2cc, M1_PRG | BRF_PRG | BRF_ESS },	//  0 V60 Code
	{ "epr-16730.15",        0x080000, 0xbab24dee, M1_PRG | BRF_PRG | BRF_ESS },	//  1
	{ "epr-17052.4",         0x020000, 0x0f4743e7, M1_BOOT | BRF_PRG | BRF_ESS },	//  2 V60 Boot
	{ "epr-17053.5",         0x020000, 0x83af2415, M1_BOOT | BRF_PRG | BRF_ESS },	//  3
	{ "epr-17056.6",         0x080000, 0x5216de4d, M1_DATA | BRF_PRG | BRF_ESS },	//  4 V60 Data
	{ "epr-17057.7",         0x080000, 0xb21aa2db, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "mpr-16736.8",         0x080000, 0x10b6a025, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "mpr-16735.9",         0x080000, 0xc82fd198, M1_DATA | BRF_PRG | BRF_ESS },	//  7
	{ "mpr-16734.10",        0x080000, 0xf76371c1, M1_DATA | BRF_PRG | BRF_ESS },	//  8
	{ "mpr-16733.11",        0x080000, 0xe105847b, M1_DATA | BRF_PRG | BRF_ESS },	//  9
	{ "315-5711.bin",        0x002000, 0x6a21f304, M1_TGP | BRF_PRG | BRF_ESS },	// 10 TGP Program
	{ "epr-17126.7",         0x020000, 0x50178e40, M1_68K | BRF_PRG | BRF_ESS },	// 11 68K Code
	{ "epr-16752.8",         0x020000, 0x6541c48f, M1_68K | BRF_PRG | BRF_ESS },	// 12
	{ "mpr-16753.32",        0x200000, 0x324a8333, M1_PCM0 | BRF_SND          },	// 13 MultiPCM #1 Samples
	{ "mpr-16754.33",        0x200000, 0x144f3cf5, M1_PCM0 | BRF_SND          },	// 14
	{ "mpr-16755.4",         0x200000, 0x4baaf878, M1_PCM1 | BRF_SND          },	// 15 MultiPCM #2 Samples
	{ "mpr-16756.5",         0x200000, 0xd9c40672, M1_PCM1 | BRF_SND          },	// 16
	{ "mpr-16743.26",        0x200000, 0xa710d33c, M1_POLY | BRF_GRA          },	// 17 Polygon Models
	{ "mpr-16744.27",        0x200000, 0xde796e1f, M1_POLY | BRF_GRA          },	// 18
	{ "mpr-16745.28",        0x200000, 0x905b689c, M1_POLY | BRF_GRA          },	// 19
	{ "mpr-16746.29",        0x200000, 0x163b312e, M1_POLY | BRF_GRA          },	// 20
	{ "mpr-16747.30",        0x200000, 0x7353bb12, M1_POLY | BRF_GRA          },	// 21
	{ "mpr-16748.31",        0x200000, 0x8ce98d3a, M1_POLY | BRF_GRA          },	// 22
	{ "mpr-16749.32",        0x200000, 0x0e36dc1a, M1_POLY | BRF_GRA          },	// 23
	{ "mpr-16750.33",        0x200000, 0xe4f0b98d, M1_POLY | BRF_GRA          },	// 24
	{ "mpr-16741.39",        0x080000, 0x84b2ffd8, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 25 TGP Data
	{ "mpr-16742.40",        0x080000, 0xe9cc12bb, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 26
	{ "mpr-16739.41",        0x080000, 0x6c73e98f, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 27
	{ "mpr-16740.42",        0x080000, 0x44b31007, M1_TGPDATA | BRF_PRG | BRF_ESS },	// 28
	{ "epr-18851.ic22",      0x020000, 0x85f75bd7, 0 | BRF_OPT                },	// 29 Unused
	{ "epr-18850.ic67",      0x008000, 0x45a3e9fc, 0 | BRF_OPT                },	// 30
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 31 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 32
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 33 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 34
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 35
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 36
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 37
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 38
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 39
};

STDROMPICKEXT(wingwar360, wingwar360, model1io2)
STD_ROM_FN(wingwar360)

struct BurnDriver BurnDrvWingwar360 = {
	"wingwar360", "wingwar", "model1io2", NULL, "1994",
	"Wing War R360 (US)\0", NULL, "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 1, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, wingwar360RomInfo, wingwar360RomName, NULL, NULL, NULL, NULL, Wingwar360InputInfo, Wingwar360DIPInfo,
	Wingwar360Init, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};


// Sega NetMerc

static struct BurnRomInfo netmercRomDesc[] = {
	{ "epr-18120.ic5",       0x080000, 0xde489762, M1_BOOT | BRF_PRG | BRF_ESS },	//  0 V60 Boot
	{ "epr-18122.ic6",       0x080000, 0x10581ab3, M1_DATA | BRF_PRG | BRF_ESS },	//  1 V60 Data
	{ "epr-18123.ic7",       0x080000, 0x27e0d848, M1_DATA | BRF_PRG | BRF_ESS },	//  2
	{ "epr-18124.ic8",       0x080000, 0x5a0e5618, M1_DATA | BRF_PRG | BRF_ESS },	//  3
	{ "epr-18125.ic9",       0x080000, 0xbd47284f, M1_DATA | BRF_PRG | BRF_ESS },	//  4
	{ "epr-18126.ic10",      0x080000, 0x43dc5ca8, M1_DATA | BRF_PRG | BRF_ESS },	//  5
	{ "epr-18127.ic11",      0x080000, 0xd307a4ca, M1_DATA | BRF_PRG | BRF_ESS },	//  6
	{ "315-5711.bin",        0x002000, 0x6a21f304, M1_TGP | BRF_PRG | BRF_ESS },	//  7 TGP Program
	{ "epr-18121.ic7",       0x080000, 0x113285b5, M1_68K | BRF_PRG | BRF_ESS },	//  8 68K Code
	{ "mpr-18134.ic32",      0x200000, 0x9a4109e5, M1_PCM0 | BRF_SND          },	//  9 MultiPCM #1 Samples
	{ "mpr-18135.ic33",      0x200000, 0x291d17e4, M1_PCM0 | BRF_SND          },	// 10
	{ "mpr-18136.ic4",       0x200000, 0x2a9c0aaf, M1_PCM1 | BRF_SND          },	// 11 MultiPCM #2 Samples
	{ "mpr-18128.ic26",      0x200000, 0x70d8028c, M1_POLY | BRF_GRA          },	// 12 Polygon Models
	{ "mpr-18129.ic27",      0x200000, 0x134feea3, M1_POLY | BRF_GRA          },	// 13
	{ "mpr-18130.ic28",      0x200000, 0xc7aeae3e, M1_POLY | BRF_GRA          },	// 14
	{ "mpr-18131.ic29",      0x200000, 0x32683b33, M1_POLY | BRF_GRA          },	// 15
	{ "mpr-18132.ic30",      0x200000, 0xa17e3ac2, M1_POLY | BRF_GRA          },	// 16
	{ "mpr-18133.ic31",      0x200000, 0xf56354dd, M1_POLY | BRF_GRA          },	// 17
	{ "u1",                  0x004000, 0x7073a312, 0 | BRF_OPT                },	// 18 Unused
	{ "u2",                  0x004000, 0xc589f428, 0 | BRF_OPT                },	// 19
	{ "netmerc_nvram.bin",   0x010000, 0x09866826, M1_NVRAM | BRF_PRG | BRF_ESS },	// 20 Default NVRAM
	{ "opr14742.bin",        0x020000, 0x446a1085, M1_TABLES | BRF_PRG | BRF_ESS },	// 21 TGP Tables
	{ "opr14743.bin",        0x020000, 0xe8953554, M1_TABLES | BRF_PRG | BRF_ESS },	// 22
	{ "opr-14744.58",        0x020000, 0x730ea9e0, 0 | BRF_OPT                },	// 23 Unused
	{ "opr-14745.59",        0x020000, 0x4c934d96, 0 | BRF_OPT                },	// 24
	{ "opr-14746.62",        0x020000, 0x2a266cbd, 0 | BRF_OPT                },	// 25
	{ "opr-14747.63",        0x020000, 0xa4ad5e19, 0 | BRF_OPT                },	// 26
	{ "opr14748.bin",        0x020000, 0x4a532cb8, 0 | BRF_OPT                },	// 27
	{ "315-5571.bin",        0x002000, 0x1233db2a, 0 | BRF_OPT                },	// 28
	{ "315-5572.bin",        0x002000, 0x66785906, 0 | BRF_OPT                },	// 29
};

STDROMPICKEXT(netmerc, netmerc, model1io2)
STD_ROM_FN(netmerc)

static INT32 NetmercInit()
{
	nAnalogType = 5;
	bIOBoard2 = 1;
	bHasHMD = 2;

	return DrvInit(1, 0, 1);
}

struct BurnDriver BurnDrvNetmerc = {
	"netmerc", NULL, "model1io2", NULL, "1993",
	"Sega NetMerc\0", "Sound is incomplete (bad dump of a sample rom)", "Sega", "Model 1",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 1, HARDWARE_SEGA_MISC, GBF_SHOOT, 0,
	NULL, netmercRomInfo, netmercRomName, NULL, NULL, NULL, NULL, NetmercInputInfo, NetmercDIPInfo,
	NetmercInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x10000,
	496, 384, 4, 3
};
