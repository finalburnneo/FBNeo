// based on MESS/MAME driver by David Haywood

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "bitswap.h"

static UINT8 DrvJoy0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy1[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy2[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy3[8] = {0, 0, 0, 0, 0, 0, 0, 0};
static UINT8 DrvJoy4[8] = {0, 0, 0, 0, 0, 0, 0, 0};

static UINT8 DrvInput[5];
static UINT8 DrvDip[2];
static UINT8 DrvReset;
static UINT8 DrvRecalc;

static INT32 DrvWheel = 0;
static INT32 DrvAccel = 0;

static INT32 nCyclesDone, nCyclesTotal;
static INT32 nCyclesSegment;

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRAM;
static UINT8 *DrvMainROM;
static UINT32 *DrvPalette;
static UINT32 *Palette;
static UINT8 *cache_bitmap;

static UINT8 segae_8000bank;
static UINT8 port_fa_last;
static UINT8 rombank;

static UINT8 hintcount;			/* line interrupt counter, decreased each scanline */
UINT8 vintpending;
UINT8 hintpending;

//UINT8 m_port_select;
UINT8 currentLine = 0;

UINT8 leftcolumnblank = 0; // most games need this, except tetris

#define CHIPS 2							/* There are 2 VDP Chips */

UINT8  segae_vdp_cmdpart[CHIPS];		/* VDP Command Part Counter */
UINT16 segae_vdp_command[CHIPS];		/* VDP Command Word */

UINT8  segae_vdp_accessmode[CHIPS];		/* VDP Access Mode (VRAM, CRAM) */
UINT16 segae_vdp_accessaddr[CHIPS];		/* VDP Access Address */
UINT8  segae_vdp_readbuffer[CHIPS];		/* VDP Read Buffer */

UINT8 *segae_vdp_vram[CHIPS];			/* Pointer to VRAM */
UINT8 *segae_vdp_cram[CHIPS];			/* Pointer to the VDP's CRAM */
UINT8 *segae_vdp_regs[CHIPS];			/* Pointer to the VDP's Registers */

UINT8 segae_vdp_vrambank[CHIPS];		/* Current VRAM Bank number (from writes to Port 0xf7) */

static struct BurnInputInfo TransfrmInputList[] = {

	{"P1 Coin",		BIT_DIGITAL,	DrvJoy0 + 0,	"p1 coin"},
	{"P1 Start",	BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"},
	{"P2 Coin",		BIT_DIGITAL,	DrvJoy0 + 1,	"p2 coin"},
	{"P2 Start",	BIT_DIGITAL,	DrvJoy0 + 7,	"p2 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy0 + 3,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};
STDINPUTINFO(Transfrm)


static struct BurnDIPInfo TransfrmDIPList[]=
{
	{0x0c, 0xff, 0xff, 0xff, NULL		},
	{0x0d, 0xff, 0xff, 0xfc, NULL		},

	{0   , 0xfe, 0   ,    2, "1 Player Only"		},
	{0x0d, 0x01, 0x01, 0x00, "Off"		},
	{0x0d, 0x01, 0x01, 0x01, "On"		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0d, 0x01, 0x02, 0x02, "Off"		},
	{0x0d, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Lives"		},
	{0x0d, 0x01, 0x0c, 0x0c, "3"		},
	{0x0d, 0x01, 0x0c, 0x08, "4"		},
	{0x0d, 0x01, 0x0c, 0x04, "5"		},
	{0x0d, 0x01, 0x0c, 0x00, "Infinite (Cheat)"		},

	{0   , 0xfe, 0   ,    4, "Bonus Life"		},
	{0x0d, 0x01, 0x30, 0x20, "10k, 30k, 50k and 70k"		},
	{0x0d, 0x01, 0x30, 0x30, "20k, 60k, 100k and 140k"		},
	{0x0d, 0x01, 0x30, 0x10, "30k, 80k, 130k and 180k"		},
	{0x0d, 0x01, 0x30, 0x00, "50k, 150k and 250k"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0d, 0x01, 0xc0, 0x40, "Easy"		},
	{0x0d, 0x01, 0xc0, 0xc0, "Medium"		},
	{0x0d, 0x01, 0xc0, 0x80, "Hard"		},
	{0x0d, 0x01, 0xc0, 0x00, "Hardest"		},
};

STDDIPINFO(Transfrm)

#define A(a, b, c, d) {a, b, (UINT8*)(c), d}
static struct BurnInputInfo HangonjrInputList[] = {

    {"P1 Coin",     BIT_DIGITAL,    DrvJoy0 + 0,    "p1 coin"},
    {"P1 Start",    BIT_DIGITAL,    DrvJoy0 + 4,    "p1 start"},
	A("P1 Steering", BIT_ANALOG_REL, &DrvWheel,     "p1 x-axis"),
	A("P1 Accelerate", BIT_ANALOG_REL, &DrvAccel,   "p1 z-axis"),

	{"Reset",		BIT_DIGITAL,	&DrvReset,	    "reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy0 + 3,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	    "dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	    "dip"},
};

STDINPUTINFO(Hangonjr)
#undef A


static struct BurnDIPInfo HangonjrDIPList[]=
{
	{0x06, 0xff, 0xff, 0xff, NULL		},
	{0x07, 0xff, 0xff, 0x04, NULL		},

	{0   , 0xfe, 0   ,    4, "Enemies"		},
	{0x06, 0x01, 0x06, 0x06, "Easy"		},
	{0x06, 0x01, 0x06, 0x04, "Medium"		},
	{0x06, 0x01, 0x06, 0x02, "Hard"		},
	{0x06, 0x01, 0x06, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    4, "Time Adj."		},
	{0x06, 0x01, 0x18, 0x18, "Easy"		},
	{0x06, 0x01, 0x18, 0x10, "Medium"		},
	{0x06, 0x01, 0x18, 0x08, "Hard"		},
	{0x06, 0x01, 0x18, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode (No Toggle)"		},
	{0x07, 0x01, 0x04, 0x04, "Off"		},
	{0x07, 0x01, 0x04, 0x00, "On"		},
};

STDDIPINFO(Hangonjr)

static struct BurnInputInfo TetrisseInputList[] = {

	{"P1 Coin",		BIT_DIGITAL,	DrvJoy0 + 0,	"p1 coin"},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy0 + 6,	"p1 start"},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy1 + 4,	"p1 fire 1"},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy1 + 5,	"p1 fire 2"},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"},
	{"Service",		BIT_DIGITAL,	DrvJoy0 + 3,	"service"},
	{"Dip A",		BIT_DIPSWITCH,	DrvDip + 0,	"dip"},
	{"Dip B",		BIT_DIPSWITCH,	DrvDip + 1,	"dip"},
};

STDINPUTINFO(Tetrisse)


static struct BurnDIPInfo TetrisseDIPList[]=
{
	{0x0a, 0xff, 0xff, 0x30, NULL		},
	{0x0b, 0xff, 0xff, 0x04, NULL		},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"		},
	{0x0a, 0x01, 0x02, 0x02, "Off"		},
	{0x0a, 0x01, 0x02, 0x00, "On"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"		},
	{0x0a, 0x01, 0x30, 0x20, "Easy"		},
	{0x0a, 0x01, 0x30, 0x30, "Normal"		},
	{0x0a, 0x01, 0x30, 0x10, "Hard"		},
	{0x0a, 0x01, 0x30, 0x00, "Hardest"		},

	{0   , 0xfe, 0   ,    2, "Service Mode (No Toggle)"		},
	{0x0b, 0x01, 0x04, 0x04, "Off"		},
	{0x0b, 0x01, 0x04, 0x00, "On"		},
};
STDDIPINFO(Tetrisse)

static UINT8 __fastcall systeme_main_read(UINT16 address)
{
//	bprintf(0, _T("systeme_main_read adr %X.\n"), address);

	return 0;
}

static void __fastcall systeme_main_write(UINT16 address, UINT8 data)
{
	if(address >= 0x8000 && address <= 0xbfff)
	{
		segae_vdp_vram [1-segae_8000bank][(address - 0x8000) + (0x4000-(segae_vdp_vrambank[1-segae_8000bank] * 0x4000))] = data;

		return;
	}
}

static UINT32 scalerange(UINT32 x, UINT32 in_min, UINT32 in_max, UINT32 out_min, UINT32 out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


static UINT8 scale_wheel(UINT32 PaddlePortnum) {
	UINT8 Temp;

	Temp = 0x7f + (PaddlePortnum >> 4);
	if (Temp < 0x01) Temp = 0x01;
	if (Temp > 0xfe) Temp = 0xfe;
	Temp = scalerange(Temp, 0x3f, 0xbe, 0x20, 0xe0);
	return Temp;
}

static UINT8 scale_accel(UINT32 PaddlePortnum) {
	UINT8 Temp;

	Temp = PaddlePortnum >> 4;
	if (Temp < 0x08) Temp = 0x00; // sometimes default for digital button -> analog is "1"
	return Temp;
}


static UINT8 __fastcall hangonjr_port_f8_read (UINT8 port)
{
	UINT8 temp = 0;
	//bprintf(0, _T("Wheel %.04X  Accel %.04X\n"), scale_wheel(DrvWheel), scale_accel(DrvAccel));

	if (port_fa_last == 0x08)  /* 0000 1000 */ /* Angle */
		temp = scale_wheel(DrvWheel);

	if (port_fa_last == 0x09)  /* 0000 1001 */ /* Accel */
		temp = scale_accel(DrvAccel);

	return temp;
}

static inline void __fastcall hangonjr_port_fa_write (UINT8 data)
{
	/* Seems to write the same pattern again and again bits ---- xx-x used */
	port_fa_last = data;
}

static void segae_bankswitch (void)
{
	ZetMapMemory(DrvMainROM + 0x10000 + ( rombank * 0x4000), 0x8000, 0xbfff, MAP_ROM);
}

static void __fastcall bank_write(UINT8 data)
{
	segae_vdp_vrambank[0]	= (data & 0x80) >> 7; /* Back  Layer VDP (0) VRAM Bank */
	segae_vdp_vrambank[1]	= (data & 0x40) >> 6; /* Front Layer VDP (1) VRAM Bank */
	segae_8000bank			= (data & 0x20) >> 5; /* 0x8000 Write Select */
	rombank					=  data & 0x0f;		  /* Rom Banking */
	segae_bankswitch();
}

static void segae_vdp_setregister ( UINT8 chip, UINT16 cmd )
{
	UINT8 regnumber;
	UINT8 regdata;

	regnumber = (cmd & 0x0f00) >> 8;
	regdata   = (cmd & 0x00ff);
	if (regnumber < 11) {
		segae_vdp_regs[chip][regnumber] = regdata;
	} else {
		/* Illegal, there aren't this many registers! */
	}
}

static void segae_vdp_processcmd ( UINT8 chip, UINT16 cmd )
{
	if ( (cmd & 0xf000) == 0x8000 ) { /*  1 0 0 0 - - - - - - - - - - - -  VDP Register Set */
		segae_vdp_setregister (chip, cmd);
	} else { /* Anything Else */
		segae_vdp_accessmode[chip] = (cmd & 0xc000) >> 14;
		segae_vdp_accessaddr[chip] = (cmd & 0x3fff);

		if ((segae_vdp_accessmode[chip]==0x03) && (segae_vdp_accessaddr[chip] > 0x1f) ) { /* Check Address is valid for CRAM */
			/* Illegal, CRAM isn't this large! */
			segae_vdp_accessaddr[chip] &= 0x1f;
		}

		if (segae_vdp_accessmode[chip] == 0x00) { /*  0 0 - - - - - - - - - - - - - -  VRAM Acess Mode (Special Read) */
			segae_vdp_readbuffer[chip] = segae_vdp_vram[chip][ segae_vdp_vrambank[chip]*0x4000 + segae_vdp_accessaddr[chip] ];
			segae_vdp_accessaddr[chip] += 1;
			segae_vdp_accessaddr[chip] &= 0x3fff;
		}
	}
}

static UINT8 segae_vdp_counter_r (UINT8 chip, UINT8 offset)
{
	UINT8 temp = 0;
	UINT16 sline;

	switch (offset)
	{
		case 0: /* port 0x7e, Vert Position (in scanlines) */
			sline = currentLine;
//			if (sline > 0xDA) sline -= 6;
//			temp = sline-1 ;
			if (sline > 0xDA) sline -= 5;
			temp = sline ;
			break;
		case 1: /* port 0x7f, Horz Position (in pixel clock cycles)  */
			/* unhandled for now */
			break;
	}
	return temp;
}

static UINT8 segae_vdp_data_r(UINT8 chip)
{
	UINT8 temp;

	segae_vdp_cmdpart[chip] = 0;

	temp = segae_vdp_readbuffer[chip];

	if (segae_vdp_accessmode[chip]==0x03) { /* CRAM Access */
		/* error CRAM can't be read!! */
	} else { /* VRAM */
		segae_vdp_readbuffer[chip] = segae_vdp_vram[chip][ segae_vdp_vrambank[chip]*0x4000 + segae_vdp_accessaddr[chip] ];
		segae_vdp_accessaddr[chip] += 1;
		segae_vdp_accessaddr[chip] &= 0x3fff;
	}
	return temp;
}

static UINT8 segae_vdp_reg_r ( UINT8 chip )
{
	UINT8 temp;

	temp = 0;

	temp |= (vintpending << 7);
	temp |= (hintpending << 6);

	hintpending = vintpending = 0;

	return temp;
}

static inline UINT8 pal2bit(UINT8 bits)
{
	bits &= 3;
	return (bits << 6) | (bits << 4) | (bits << 2) | bits;
}

static void segae_vdp_data_w ( UINT8 chip, UINT8 data )
{
	segae_vdp_cmdpart[chip] = 0;

	if (segae_vdp_accessmode[chip]==0x03) { /* CRAM Access */
		UINT8 r,g,b, temp;

		temp = segae_vdp_cram[chip][segae_vdp_accessaddr[chip]];

		segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] = data;

		if (temp != data) 
		{

			r = (segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] & 0x03) >> 0;
			g = (segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] & 0x0c) >> 2;
			b = (segae_vdp_cram[chip][segae_vdp_accessaddr[chip]] & 0x30) >> 4;

			Palette[segae_vdp_accessaddr[chip] + 32*chip] = pal2bit(r) << 16 | pal2bit(g) << 8 | pal2bit(b);  //BurnHighCol(pal2bit(r), pal2bit(g), pal2bit(b), 0);
			DrvPalette[segae_vdp_accessaddr[chip] + 32*chip] = BurnHighCol(pal2bit(r), pal2bit(g), pal2bit(b), 0);
		}

		segae_vdp_accessaddr[chip] += 1;
		segae_vdp_accessaddr[chip] &= 0x1f;
	} else { /* VRAM Accesses */
		segae_vdp_vram[chip][ segae_vdp_vrambank[chip]*0x4000 + segae_vdp_accessaddr[chip] ] = data;
		segae_vdp_accessaddr[chip] += 1;
		segae_vdp_accessaddr[chip] &= 0x3fff;
	}
}

static void segae_vdp_reg_w ( UINT8 chip, UINT8 data )
{
	if (!segae_vdp_cmdpart[chip]) {
		segae_vdp_cmdpart[chip] = 1;
		segae_vdp_command[chip] = data;
	} else {
		segae_vdp_cmdpart[chip] = 0;
		segae_vdp_command[chip] |= (data << 8);
		segae_vdp_processcmd (chip, segae_vdp_command[chip]);
	}
}

/*static UINT8 input_r(INT32 offset)
{
	//bprintf(0, _T("input_r chip %X.\n"), offset);
	return 0xff;
}*/

static UINT8 __fastcall systeme_main_in(UINT16 port)
{
	port &= 0xff;

	switch (port) {
		case 0x7e: return segae_vdp_counter_r(0, 0);
		case 0x7f: return segae_vdp_counter_r(0, 1);
		case 0xba: return segae_vdp_data_r(0);
		case 0xbb: return segae_vdp_reg_r(0);

		case 0xbe: return segae_vdp_data_r(1);
		case 0xbf: return segae_vdp_reg_r(1);

		case 0xe0: return 0xff - DrvInput[0];
		case 0xe1: return 0xff - DrvInput[1];
//		case 0xe2: return input_r(port); // AM_RANGE(0xe2, 0xe2) AM_READ_PORT( "e2" )
		case 0xf2: return DrvDip[0];//input_r(port); // AM_RANGE(0xf2, 0xf2) AM_READ_PORT( "f2" ) /* DSW0 */
		case 0xf3: return DrvDip[1];//input_r(port); // AM_RANGE(0xf3, 0xf3) AM_READ_PORT( "f3" ) /* DSW1 */
		case 0xf8: return hangonjr_port_f8_read(port); // m_maincpu->space(AS_IO).install_read_handler(0xf8, 0xf8, read8_delegate(FUNC(systeme_state::hangonjr_port_f8_read), this));
	}	
	////bprintf(PRINT_NORMAL, _T("IO Read %x\n"), a);
	return 0;
}

static void __fastcall systeme_main_out(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x7b: SN76496Write(0, data);
		break;

		case 0x7f: SN76496Write(1, data);
		break;

		case 0xba: segae_vdp_data_w(0, data);
		break;

		case 0xbb: segae_vdp_reg_w(0, data);
		break;

		case 0xbe: segae_vdp_data_w(1, data);
		break;

		case 0xbf:	segae_vdp_reg_w(1, data);
		break;

		case 0xf7:	bank_write(data);
		break;

		case 0xfa:	hangonjr_port_fa_write(data);
		break;
	}
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvMainROM	 	    = Next; Next += 0x40000;
	
	AllRam				= Next;
	DrvRAM			    = Next; Next += 0x10000;

	segae_vdp_vram[0]	= Next; Next += 0x8000; /* 32kb (2 banks) */
	segae_vdp_vram[1]	= Next; Next += 0x8000; /* 32kb (2 banks) */

	segae_vdp_cram[0]	= Next; Next += 0x20;
	segae_vdp_regs[0]	= Next; Next += 0x20;

	segae_vdp_cram[1]	= Next; Next += 0x20;
	segae_vdp_regs[1]	= Next; Next += 0x20;

	cache_bitmap		= Next; Next += ( (16+256+16) * 192+17 ) + 0x0f; /* 16 pixels either side to simplify drawing + 0xf for alignment */

	DrvPalette			= (UINT32*)Next; Next += 0x040 * sizeof(UINT32);
	Palette			    = (UINT32*)Next; Next += 0x040 * sizeof(UINT32);

	RamEnd			= Next;
	MemEnd			= Next;

	return 0;
}

//-----------------------
static INT32 DrvExit()
{
	ZetExit();
	GenericTilesExit();
	SN76496Exit();
	BurnFree(AllMem);

	leftcolumnblank = 0;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (DrvRAM, 0, RamEnd - DrvRAM);

	hintcount = 0;
	vintpending = 0;
	hintpending = 0;
	SN76496Reset();
	ZetOpen(0);
	segae_bankswitch();
	ZetReset();
	ZetClose();
	
	return 0;
}

static inline void DrvClearOpposites(UINT8* nJoystickInputs)
{
	if ((*nJoystickInputs & 0x03) == 0x03) {
		*nJoystickInputs &= ~0x03;
	}
	if ((*nJoystickInputs & 0x0c) == 0x0c) {
		*nJoystickInputs &= ~0x0c;
	}
}

static inline void DrvMakeInputs()
{
	// Reset Inputs
	DrvInput[0] = DrvInput[1] = DrvInput[2] = DrvInput[3] = DrvInput[4] = 0x00;

	// Compile Digital Inputs
	for (INT32 i = 0; i < 8; i++) {
		DrvInput[0] |= (DrvJoy0[i] & 1) << i;
		DrvInput[1] |= (DrvJoy1[i] & 1) << i;
		DrvInput[2] |= (DrvJoy2[i] & 1) << i;
		DrvInput[3] |= (DrvJoy3[i] & 1) << i;
		DrvInput[4] |= (DrvJoy4[i] & 1) << i;
	}

	// Clear Opposites
	DrvClearOpposites(&DrvInput[0]);
	DrvClearOpposites(&DrvInput[1]);
}

static void segae_draw8pix_solid16(UINT8 *dest, UINT8 chip, UINT16 tile, UINT8 line, UINT8 flipx, UINT8 col)
{

	UINT32 pix8 = *(UINT32 *)&segae_vdp_vram[chip][(32)*tile + (4)*line + (0x4000) * segae_vdp_vrambank[chip]];
	UINT8  pix, coladd;

	if (!pix8 && !col) return; /*note only the colour 0 of each vdp is transparent NOT colour 16???, fixes sky in HangonJr */

	coladd = 16*col;

	if (flipx)	{
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; pix+= coladd ; if (pix) dest[0] = pix+ 32*chip;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; pix+= coladd ; if (pix) dest[1] = pix+ 32*chip;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; pix+= coladd ; if (pix) dest[2] = pix+ 32*chip;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; pix+= coladd ; if (pix) dest[3] = pix+ 32*chip;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; pix+= coladd ; if (pix) dest[4] = pix+ 32*chip;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; pix+= coladd ; if (pix) dest[5] = pix+ 32*chip;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; pix+= coladd ; if (pix) dest[6] = pix+ 32*chip;
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; pix+= coladd ; if (pix) dest[7] = pix+ 32*chip;
	} else {
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; pix+= coladd ; if (pix) dest[0] = pix+ 32*chip;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; pix+= coladd ; if (pix) dest[1] = pix+ 32*chip;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; pix+= coladd ; if (pix) dest[2] = pix+ 32*chip;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; pix+= coladd ; if (pix) dest[3] = pix+ 32*chip;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; pix+= coladd ; if (pix) dest[4] = pix+ 32*chip;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; pix+= coladd ; if (pix) dest[5] = pix+ 32*chip;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; pix+= coladd ; if (pix) dest[6] = pix+ 32*chip;
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; pix+= coladd ; if (pix) dest[7] = pix+ 32*chip;
	}
}

static void segae_draw8pix(UINT8 *dest, UINT8 chip, UINT16 tile, UINT8 line, UINT8 flipx, UINT8 col)
{

	UINT32 pix8 = *(UINT32 *)&segae_vdp_vram[chip][(32)*tile + (4)*line + (0x4000) * segae_vdp_vrambank[chip]];
	UINT8  pix, coladd;

	if (!pix8) return;

	coladd = 16*col+32*chip;

	if (flipx)	{
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; if (pix) dest[0] = pix+ coladd;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; if (pix) dest[1] = pix+ coladd;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; if (pix) dest[2] = pix+ coladd;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; if (pix) dest[3] = pix+ coladd;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; if (pix) dest[4] = pix+ coladd;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; if (pix) dest[5] = pix+ coladd;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; if (pix) dest[6] = pix+ coladd;
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; if (pix) dest[7] = pix+ coladd;
	} else {
		pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; if (pix) dest[0] = pix+ coladd;
		pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; if (pix) dest[1] = pix+ coladd;
		pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; if (pix) dest[2] = pix+ coladd;
		pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; if (pix) dest[3] = pix+ coladd;
		pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; if (pix) dest[4] = pix+ coladd;
		pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; if (pix) dest[5] = pix+ coladd;
		pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; if (pix) dest[6] = pix+ coladd;
		pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; if (pix) dest[7] = pix+ coladd;
	}
}

static void segae_draw8pixsprite(UINT8 *dest, UINT8 chip, UINT16 tile, UINT8 line)
{

	UINT32 pix8 = *(UINT32 *)&segae_vdp_vram[chip][(32)*tile + (4)*line + (0x4000) * segae_vdp_vrambank[chip]];
	UINT8  pix;

	if (!pix8) return; /*note only the colour 0 of each vdp is transparent NOT colour 16, fixes sky in HangonJr */

	pix = ((pix8 >> 7) & 0x01) | ((pix8 >>14) & 0x02) | ((pix8 >> 21) & 0x04) | ((pix8 >> 28) & 0x08) ; if (pix) dest[0] = pix+16+32*chip;
	pix = ((pix8 >> 6) & 0x01) | ((pix8 >>13) & 0x02) | ((pix8 >> 20) & 0x04) | ((pix8 >> 27) & 0x08) ; if (pix) dest[1] = pix+16+32*chip;
	pix = ((pix8 >> 5) & 0x01) | ((pix8 >>12) & 0x02) | ((pix8 >> 19) & 0x04) | ((pix8 >> 26) & 0x08) ; if (pix) dest[2] = pix+16+32*chip;
	pix = ((pix8 >> 4) & 0x01) | ((pix8 >>11) & 0x02) | ((pix8 >> 18) & 0x04) | ((pix8 >> 25) & 0x08) ; if (pix) dest[3] = pix+16+32*chip;
	pix = ((pix8 >> 3) & 0x01) | ((pix8 >>10) & 0x02) | ((pix8 >> 17) & 0x04) | ((pix8 >> 24) & 0x08) ; if (pix) dest[4] = pix+16+32*chip;
	pix = ((pix8 >> 2) & 0x01) | ((pix8 >> 9) & 0x02) | ((pix8 >> 16) & 0x04) | ((pix8 >> 23) & 0x08) ; if (pix) dest[5] = pix+16+32*chip;
	pix = ((pix8 >> 1) & 0x01) | ((pix8 >> 8) & 0x02) | ((pix8 >> 15) & 0x04) | ((pix8 >> 22) & 0x08) ; if (pix) dest[6] = pix+16+32*chip;
	pix = ((pix8 >> 0) & 0x01) | ((pix8 >> 7) & 0x02) | ((pix8 >> 14) & 0x04) | ((pix8 >> 21) & 0x08) ; if (pix) dest[7] = pix+16+32*chip;

}

static void segae_drawspriteline(UINT8 *dest, UINT8 chip, UINT8 line)
{
	/* todo: figure out what riddle of pythagoras hates about this */

	int nosprites;
	int loopcount;

	UINT16 spritebase;

	nosprites = 0;

	spritebase =  (segae_vdp_regs[chip][5] & 0x7e) << 7;
	spritebase += (segae_vdp_vrambank[chip] * 0x4000);

	/*- find out how many sprites there are -*/

	for (loopcount=0;loopcount<64;loopcount++) {
		UINT8 ypos;

		ypos = segae_vdp_vram[chip][spritebase+loopcount];

		if (ypos==208) {
			nosprites=loopcount;
			break;
		}
	}

//	if (!strcmp(Machine->gamedrv->name,"ridleofp")) nosprites = 63; /* why, there must be a bug elsewhere i guess ?! */

	/*- draw sprites IN REVERSE ORDER -*/

	for (loopcount = nosprites; loopcount >= 0;loopcount--) {
		int ypos;
		UINT8 sheight;

		ypos = segae_vdp_vram[chip][spritebase+loopcount] +1;

		if (segae_vdp_regs[chip][1] & 0x02) sheight=16; else sheight=8;

		if ( (line >= ypos) && (line < ypos+sheight) ) {
			int xpos;
			UINT8 sprnum;
			UINT8 spline;

			spline = line - ypos;

			xpos   = segae_vdp_vram[chip][spritebase+0x80+ (2*loopcount)];
			sprnum = segae_vdp_vram[chip][spritebase+0x81+ (2*loopcount)];

			if (segae_vdp_regs[chip][6] & 0x04)
				sprnum += 0x100;

			segae_draw8pixsprite(dest+xpos, chip, sprnum, spline);

		}
	}
}

static void segae_drawtilesline(UINT8 *dest, int line, UINT8 chip, UINT8 pri)
{
	/* todo: fix vscrolling (or is it something else causing the glitch on the "game over" screen of hangonjr, seems to be ..  */

	UINT8 hscroll;
	UINT8 vscroll;
	UINT16 tmbase;
	UINT8 tilesline, tilesline2;
	UINT8 coloffset, coloffset2;
	UINT8 loopcount;

	hscroll = (256-segae_vdp_regs[chip][8]);
	vscroll = segae_vdp_regs[chip][9];
	if (vscroll > 224) vscroll %= 224;

	tmbase =  (segae_vdp_regs[chip][2] & 0x0e) << 10;
	tmbase += (segae_vdp_vrambank[chip] * 0x4000);

	tilesline = (line + vscroll) >> 3;
	tilesline2= (line + vscroll) % 8;


	coloffset = (hscroll >> 3);
	coloffset2= (hscroll % 8);

	dest -= coloffset2;

	for (loopcount=0;loopcount<33;loopcount++) {

		UINT16 vram_offset, vram_word;
		UINT16 tile_no;
		UINT8  palette, priority, flipx, flipy;

		vram_offset = tmbase
					+ (2 * (32*tilesline + ((coloffset+loopcount)&0x1f) ) );
		vram_word = segae_vdp_vram[chip][vram_offset] | (segae_vdp_vram[chip][vram_offset+1] << 8);

		tile_no = (vram_word & 0x01ff);
		flipx =   (vram_word & 0x0200) >> 9;
		flipy =   (vram_word & 0x0400) >> 10;
		palette = (vram_word & 0x0800) >> 11;
		priority= (vram_word & 0x1000) >> 12;

		tilesline2= (line + vscroll) % 8;
		if (flipy) tilesline2 = 7-tilesline2;

		if (priority == pri) {
			if (chip == 0) segae_draw8pix_solid16(dest, chip, tile_no,tilesline2,flipx,palette);
			else segae_draw8pix(dest, chip, tile_no,tilesline2,flipx,palette);
		}
		dest+=8;
	}
}

static void segae_drawscanline(int line)
{
	UINT8* dest;

	dest = cache_bitmap + (16+256+16) * line;

	/* This should be cleared to bg colour, but which vdp determines that !, neither seems to be right, maybe its always the same col? */
	memset(dest, 0, 16+256+16);

	if (segae_vdp_regs[0][1] & 0x40) {
		segae_drawtilesline (dest+16, line, 0,0);
		segae_drawspriteline(dest+16, 0, line);
		segae_drawtilesline (dest+16, line, 0,1);
	}

	{
		if (segae_vdp_regs[1][1] & 0x40) {
			segae_drawtilesline (dest+16, line, 1,0);
			segae_drawspriteline(dest+16, 1, line);
			segae_drawtilesline (dest+16, line, 1,1);
		}
	}

	if (leftcolumnblank) memset(dest+16, 32+16, 8); /* Clear Leftmost column, there should be a register for this like on the SMS i imagine    */
							   			  /* on the SMS this is bit 5 of register 0 (according to CMD's SMS docs) for system E this  */
							   			  /* appears to be incorrect, most games need it blanked 99% of the time so we blank it      */

}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 i = 0; i < 0x40; i++) {
			DrvPalette[i] = BurnHighCol((Palette[i] >> 16) & 0xff, (Palette[i] >> 8) & 0xff, Palette[i] & 0xff, 0);
		}
		DrvRecalc = 0;
	}

	BurnTransferCopy(DrvPalette);

	UINT16 *pDst = pTransDraw;
	UINT8 *pSrc = &cache_bitmap[16];

	for (INT32 y = 0; y < nScreenHeight; y++)
	{
		for (INT32 x = 0; x < nScreenWidth; x++)
		{
			pDst[x] = pSrc[x];
		}
		pDst += nScreenWidth;
		pSrc += 288;
	}

	return 0;
}

static void segae_interrupt ()
{
	if (currentLine == 0) {
		hintcount = segae_vdp_regs[1][10];
	}

	if (currentLine <= 192) {

		if (currentLine != 192) segae_drawscanline(currentLine);

		if (currentLine == 192)
			vintpending = 1;

		if (hintcount == 0) {
			hintcount = segae_vdp_regs[1][10];
			hintpending = 1;

			if  ((segae_vdp_regs[1][0] & 0x10)) {
				ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
				return;
			}

		} else {
			hintcount--;
		}
	}

	if (currentLine > 192) {
		hintcount = segae_vdp_regs[1][10];

		if ( (currentLine<0xe0) && (vintpending) ) {
			ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
		}
	}
}


static INT32 DrvFrame()
{
	INT32 nInterleave = 262;

	if (DrvReset) DrvDoReset();

	DrvMakeInputs();

	nCyclesTotal = 10738635 / 2 / 60;
	nCyclesDone = 0;
	currentLine = 0;

	ZetNewFrame();

	for (INT32 i = 0; i < nInterleave; i++) {
		INT32 nNext;

		// Run Z80 #1
		ZetOpen(0);
		nNext = (i + 1) * nCyclesTotal / nInterleave;
		nCyclesSegment = nNext - nCyclesDone;
		nCyclesDone += ZetRun(nCyclesSegment);
		currentLine = (i - 4) & 0xff;

		segae_interrupt();
		ZetClose();
	}

	if (pBurnSoundOut)
	{
		SN76496Update(0, pBurnSoundOut, nBurnSoundLen);
		SN76496Update(1, pBurnSoundOut, nBurnSoundLen);
	}	

	if (pBurnDraw) DrvDraw();

	return 0;
}

static INT32 DrvInit(UINT8 game)
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	if (BurnLoadRom(DrvMainROM + 0x00000,  0, 1)) return 1;	// ( "rom5.ic7",   0x00000, 0x08000, CRC(d63925a7) SHA1(699f222d9712fa42651c753fe75d7b60e016d3ad) ) /* Fixed Code */
	if (BurnLoadRom(DrvMainROM + 0x10000,  1, 1)) return 1;	// ( "rom4.ic5",   0x10000, 0x08000, CRC(ee3caab3) SHA1(f583cf92c579d1ca235e8b300e256ba58a04dc90) )
	if (BurnLoadRom(DrvMainROM + 0x18000,  2, 1)) return 1;	// ( "rom3.ic4",   0x18000, 0x08000, CRC(d2ba9bc9) SHA1(85cf2a801883bf69f78134fc4d5075134f47dc03) )

	if(game)
	{
		if (BurnLoadRom(DrvMainROM + 0x20000,  3, 1)) return 1;	// ( "rom2.ic3",   0x20000, 0x08000, CRC(e14da070) SHA1(f8781f65be5246a23c1f492905409775bbf82ea8) )
		if (BurnLoadRom(DrvMainROM + 0x28000,  4, 1)) return 1; // ( "rom1.ic2",   0x28000, 0x08000, CRC(3810cbf5) SHA1(c8d5032522c0c903ab3d138f62406a66e14a5c69) )
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvMainROM, 0x0000, 0x7fff, MAP_ROM);
	ZetMapMemory(DrvRAM, 0xc000, 0xffff, MAP_RAM);

	ZetSetWriteHandler(systeme_main_write);
	ZetSetReadHandler(systeme_main_read);
	ZetSetInHandler(systeme_main_in);
	ZetSetOutHandler(systeme_main_out);

	ZetClose();

	SN76489Init(0, 10738635 / 3, 0);
	SN76489Init(1, 10738635 / 3, 1);

	SN76496SetRoute(0, 0.50, BURN_SND_ROUTE_BOTH);
	SN76496SetRoute(1, 0.50, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	DrvDoReset();

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

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_vram[0];
		ba.nLen	  = 0x8000;
		ba.szName = "vram0";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_vram[1];
		ba.nLen	  = 0x8000;
		ba.szName = "vram1";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_cram[0];
		ba.nLen	  = 0x20;
		ba.szName = "cram0";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_cram[1];
		ba.nLen	  = 0x20;
		ba.szName = "cram1";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_regs[0];
		ba.nLen	  = 0x20;
		ba.szName = "regs0";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
		ba.Data	  = segae_vdp_regs[1];
		ba.nLen	  = 0x20;
		ba.szName = "regs1";
		BurnAcb(&ba);

	}

	if (nAction & ACB_DRIVER_DATA) {

		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(segae_8000bank);
		SCAN_VAR(port_fa_last);
		SCAN_VAR(rombank);

		SCAN_VAR(hintcount);
		SCAN_VAR(vintpending);
		SCAN_VAR(hintpending);
		SCAN_VAR(segae_vdp_cmdpart);
		SCAN_VAR(segae_vdp_command);

		SCAN_VAR(segae_vdp_accessmode);
		SCAN_VAR(segae_vdp_accessaddr);
		SCAN_VAR(segae_vdp_readbuffer);

		SCAN_VAR(segae_vdp_vrambank);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			segae_bankswitch();
			ZetClose();
		}
	}

	return 0;
}


static INT32 DrvTransfrmInit()
{
	leftcolumnblank = 1;

	return DrvInit(2);
}

static INT32 DrvHangonJrInit()
{
	leftcolumnblank = 1;

	return DrvInit(1);
}

static INT32 DrvTetrisInit()
{
	return DrvInit(0);
}
//-----------------------

// Hang-On Jr.

static struct BurnRomInfo hangonjrRomDesc[] = {
	{ "rom5.ic7",	0x8000, 0xd63925a7, 1 }, //  0 maincpu
	{ "rom4.ic5",	0x8000, 0xee3caab3, 1 }, //  1
	{ "rom3.ic4",	0x8000, 0xd2ba9bc9, 1 }, //  2
	{ "rom2.ic3",	0x8000, 0xe14da070, 1 }, //  3
	{ "rom1.ic2",	0x8000, 0x3810cbf5, 1 }, //  4
};

STD_ROM_PICK(hangonjr)
STD_ROM_FN(hangonjr)

//  Tetris (Japan, System E)

static struct BurnRomInfo TetrisseRomDesc[] = {

	{ "epr12213.7", 0x8000, 0xef3c7a38, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "epr12212.5", 0x8000, 0x28b550bf, BRF_ESS | BRF_PRG }, // 1
	{ "epr12211.4", 0x8000, 0x5aa114e9, BRF_ESS | BRF_PRG }, // 2
};

STD_ROM_PICK(Tetrisse)
STD_ROM_FN(Tetrisse)

//  Transformer

static struct BurnRomInfo TransfrmRomDesc[] = {

	{ "ic7.top", 0x8000, 0xccf1d123, BRF_ESS | BRF_PRG }, // 0 maincpu
	{ "epr-7347.ic5", 0x8000, 0xdf0f639f, BRF_ESS | BRF_PRG }, // 1
	{ "epr-7348.ic4", 0x8000, 0x0f38ea96, BRF_ESS | BRF_PRG }, // 2
	{ "ic3.top", 0x8000, 0x9d485df6, BRF_ESS | BRF_PRG }, // 3
	{ "epr-7350.ic2", 0x8000, 0x0052165d, BRF_ESS | BRF_PRG }, // 4
};

STD_ROM_PICK(Transfrm)
STD_ROM_FN(Transfrm)

struct BurnDriver BurnDrvHangonjr = {
	"hangonjr", NULL, NULL, NULL, "1985",
	"Hang-On Jr.\0", "Graphics issues on the Game Over screen", "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, hangonjrRomInfo, hangonjrRomName, NULL, NULL, HangonjrInputInfo, HangonjrDIPInfo,
	DrvHangonJrInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};

struct BurnDriver BurnDrvTetrisse = {
	"tetrisse", NULL, NULL, NULL, "1988",
	"Tetris (Japan, System E)\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, TetrisseRomInfo, TetrisseRomName, NULL, NULL, TetrisseInputInfo, TetrisseDIPInfo,
	DrvTetrisInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};

struct BurnDriver BurnDrvTransfrm = {
	"transfrm", NULL, NULL, NULL, "1986",
	"Transformer\0", NULL, "Sega", "System E",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MISC, GBF_MISC, 0,
	NULL, TransfrmRomInfo, TransfrmRomName, NULL, NULL, TransfrmInputInfo, TransfrmDIPInfo,
	DrvTransfrmInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 64,
	256, 192, 4, 3
};
