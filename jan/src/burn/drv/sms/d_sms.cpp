// Sega Mastersystem driver for FBA, interface for SMS Plus by Charles MacDonald

#include "smsshared.h"
#include "z80_intf.h"
#include "sn76496.h"
#include "burn_ym2413.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvRAM;
static UINT32 *DrvPalette;
static UINT8 DrvReset;

UINT8 SMSPaletteRecalc;
UINT8 SMSJoy1[12];
UINT8 SMSJoy2[12];
UINT8 SMSDips[3];

static struct BurnDIPInfo SMSDIPList[] = {
	{0x0e, 0xff, 0xff, 0x00, NULL				},

	{0   , 0xfe, 0   ,    2, "FM Unit Emulation"},
	{0x0e, 0x01, 0x04, 0x04, "On (Change needs restart!)"},
	{0x0e, 0x01, 0x04, 0x00, "Off"			},
};

STDDIPINFO(SMS)

static struct BurnDIPInfo GGDIPList[]=
{
	{0x0e, 0xff, 0xff, 0x00, NULL			},

	{0   , 0xfe, 0   ,    2, "Display full screen (overscan mode)"},
	{0x0e, 0x01, 0x08, 0x08, "On"			},
	{0x0e, 0x01, 0x08, 0x00, "Off"			},
};

STDDIPINFO(GG)

static struct BurnInputInfo SMSInputList[] = {
	{"P1 Start(GG)/Pause(SMS)",		BIT_DIGITAL,	SMSJoy1 + 1,	"p1 start"	},
	{"P1 Up",		    BIT_DIGITAL,	SMSJoy1 + 3,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	SMSJoy1 + 4,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	SMSJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	SMSJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	SMSJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	SMSJoy1 + 8,	"p1 fire 2"	},

	{"P2 Up",		    BIT_DIGITAL,	SMSJoy2 + 3,	"p2 up"		},
	{"P2 Down",		    BIT_DIGITAL,	SMSJoy2 + 4,	"p2 down"	},
	{"P2 Left",		    BIT_DIGITAL,	SMSJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	SMSJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	SMSJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	SMSJoy2 + 8,	"p2 fire 2"	},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
	{"Dip A",		    BIT_DIPSWITCH,	SMSDips + 0,	"dip"       },
};

STDINPUTINFO(SMS)

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvPalette	= (UINT32*)Next; Next += 0x1000 * sizeof(UINT32);

	AllRam		= Next;
	DrvRAM      = Next; Next += 0x2000; // RAM (not used..yet!)
	RamEnd		= Next;
	MemEnd		= Next;

	return 0;
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	vdp_init();     // gets rid of crap on the screen w/GG
	render_init();  // ""
	system_reset();

	return 0;
}

INT32 SMSExit()
{
	ZetExit();
	GenericTilesExit();

	BurnFree (AllMem);

	if (cart.rom) {
		BurnFree (cart.rom);
		cart.rom = NULL;
	}

	system_shutdown();

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

void DrvCalcPalette()
{
	for (INT32 i = 0; i < PALETTE_SIZE; i++)
	{
		int r, g, b;
		r = bitmap.pal.color[i][0];
		g = bitmap.pal.color[i][1];
		b = bitmap.pal.color[i][2];
		DrvPalette[i] = BurnHighCol(r, g, b, 0);
	}
}

INT32 SMSDraw()
{	
	DrvCalcPalette();
	BurnTransferCopy(DrvPalette);
	return 0;
}

INT32 SMSFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		// Inputs!
		/* Reset input states */
		input.system    = 0;
		input.pad[0]    = 0;
		input.pad[1]    = 0;
		input.analog[0] = 0x7F;
		input.analog[1] = 0x7F;

		/* Parse player 1 joystick state */
		if(SMSJoy1[3]) input.pad[0] |= INPUT_UP;
		if(SMSJoy1[4]) input.pad[0] |= INPUT_DOWN;
		if(SMSJoy1[5]) input.pad[0] |= INPUT_LEFT;
		if(SMSJoy1[6]) input.pad[0] |= INPUT_RIGHT;
		if(SMSJoy1[7]) input.pad[0] |= INPUT_BUTTON2;
		if(SMSJoy1[8]) input.pad[0] |= INPUT_BUTTON1;
		DrvClearOpposites(&input.pad[0]);
		// Player 2
		if(SMSJoy2[3]) input.pad[1] |= INPUT_UP;
		if(SMSJoy2[4]) input.pad[1] |= INPUT_DOWN;
		if(SMSJoy2[5]) input.pad[1] |= INPUT_LEFT;
		if(SMSJoy2[6]) input.pad[1] |= INPUT_RIGHT;
		if(SMSJoy2[7]) input.pad[1] |= INPUT_BUTTON2;
		if(SMSJoy2[8]) input.pad[1] |= INPUT_BUTTON1;
		DrvClearOpposites(&input.pad[1]);
		if(SMSJoy1[1]) input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;

		gg_overscanmode = (SMSDips[0] & 0x08);

	}

	BurnTransferClear();

	ZetOpen(0);
	system_frame(0);
	ZetClose();

	if (pBurnDraw)
		SMSDraw();

	return 0;
}

void system_manage_sram(UINT8 */*sram*/, INT32 /*slot*/, INT32 /*mode*/)
{

}

// Notes:
// Back to the Future II - bottom of the screen is corrupt, playable
// Street Fighter II - reboots @ game start

// GG:
// Surf ninjas - weird graphics at boot, playable
// T2 - Judgement Day - glitchy sprites in-game, noted to play the SMS version instead.

static INT32 load_rom()
{
    INT32 size;
	struct BurnRomInfo ri;

	BurnDrvGetRomInfo(&ri, 0);
	size = ri.nLen;

	/* Don't load games smaller than 16K */
    if(size < 0x2000) return 0;

	cart.rom = (UINT8 *)BurnMalloc(0x100000);
	if (BurnLoadRom(cart.rom + 0x0000, 0, 1)) return 0;

    /* Take care of image header, if present */
    if((size / 512) & 1)
    {   bprintf(0, _T("Removed SMS Cart header.\n"));
        size -= 512;
        memmove(cart.rom, cart.rom + 512, size);
    }

    cart.pages = (size / 0x4000);
    cart.pages8k = (size / 0x2000);

    /* Assign default settings (US NTSC machine) */
    cart.mapper     = MAPPER_SEGA;
    sms.display     = DISPLAY_NTSC;
    sms.territory   = TERRITORY_EXPORT;

	/* Figure out game image type */
	if ((BurnDrvGetHardwareCode() & HARDWARE_PUBLIC_MASK) == HARDWARE_SEGA_GAME_GEAR) {
		sms.console = CONSOLE_GG;
	} else {
		sms.console = CONSOLE_SMS;
	}

	// Override mapper from hardware code
	switch (BurnDrvGetHardwareCode() & 0xff) {
		case HARDWARE_SMS_MAPPER_NONE:
			cart.mapper = MAPPER_NONE;
			break;

		case HARDWARE_SMS_MAPPER_CODIES: {
			cart.mapper = MAPPER_CODIES;
			break;
		}
		
		case HARDWARE_SMS_MAPPER_MSX: {
			cart.mapper = MAPPER_MSX;
			break;
		}
		
		case HARDWARE_SMS_MAPPER_MSX_NEMESIS: {
			cart.mapper = MAPPER_MSX_NEMESIS;
			break;
		}
		
		case HARDWARE_SMS_MAPPER_KOREA: {
			cart.mapper = MAPPER_KOREA;
			break;
		}

		case HARDWARE_SMS_MAPPER_KOREA8K: {
			cart.mapper = MAPPER_KOREA8K;
			break;
		}

		case HARDWARE_SMS_MAPPER_4PAK: {
			cart.mapper = MAPPER_4PAK;
			break;
		}

		case HARDWARE_SMS_MAPPER_XIN1: {
			cart.mapper = MAPPER_XIN1;
			break;
		}

		default: {
			cart.mapper = MAPPER_SEGA;
			break;
		}
	}
	
	// Override GG SMS mode from hardware code
	if ((BurnDrvGetHardwareCode() & HARDWARE_SMS_GG_SMS_MODE) == HARDWARE_SMS_GG_SMS_MODE) {
		sms.console = CONSOLE_SMS;
	}
	
	// Override PAL mode from hardware code
	if ((BurnDrvGetHardwareCode() & HARDWARE_SMS_DISPLAY_PAL) == HARDWARE_SMS_DISPLAY_PAL) {
		sms.display = DISPLAY_PAL;
	}
	
	// Override Japanese territory from hardware code
	if ((BurnDrvGetHardwareCode() & HARDWARE_SMS_JAPANESE) == HARDWARE_SMS_JAPANESE) {
		sms.territory = TERRITORY_DOMESTIC;
	}

    system_assign_device(PORT_A, DEVICE_PAD2B);
    system_assign_device(PORT_B, DEVICE_PAD2B);

    return 1;
}

INT32 SMSInit()
{
	cart.rom = NULL;

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	GenericTilesInit();

    if(load_rom() == 0)
	{
		bprintf(0, _T("Error loading SMS/GG rom!\n"));
		return 1;
	} else {
		bprintf(0, _T("SMS/GG rom loaded ok!\n"));
	}

	/* Set up bitmap structure */
    memset(&bitmap, 0, sizeof(bitmap_t));
    bitmap.width  = 256;
    bitmap.height = 192;
    bitmap.depth  = 16;
    bitmap.granularity = 2;
    bitmap.pitch  = bitmap.width * bitmap.granularity;
    bitmap.data   = (UINT8 *)pTransDraw;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 192;

    snd.fm_clock = (sms.display == DISPLAY_PAL) ? CLOCK_PAL : CLOCK_NTSC;
    snd.psg_clock = (sms.display == DISPLAY_PAL) ? CLOCK_PAL : CLOCK_NTSC;
	sms.use_fm = (SMSDips[0] & 0x04);

    system_init();

	return 0;
}

static void system_load_state()
{
	if(cart.mapper == MAPPER_MSX || cart.mapper == MAPPER_MSX_NEMESIS) {
		if (cart.fcr[3]) sms_mapper8k_w(3, cart.fcr[3]);
		if (cart.fcr[2]) sms_mapper8k_w(2, cart.fcr[2]);
		if (cart.fcr[1]) sms_mapper8k_w(1, cart.fcr[1]);
		if (cart.fcr[0]) sms_mapper8k_w(0, cart.fcr[0]);
	} else {
		if(cart.mapper == MAPPER_KOREA8K) {
			if (cart.fcr[3]) sms_mapper8kvirt_w(3, cart.fcr[3]);
			if (cart.fcr[2]) sms_mapper8kvirt_w(2, cart.fcr[2]);
			if (cart.fcr[1]) sms_mapper8kvirt_w(1, cart.fcr[1]);
			if (cart.fcr[0]) sms_mapper8kvirt_w(0, cart.fcr[0]);
		} else if (cart.mapper != MAPPER_XIN1 && cart.mapper != MAPPER_NONE) {
			sms_mapper_w(3, cart.fcr[3]);
			sms_mapper_w(2, cart.fcr[2]);
			sms_mapper_w(1, cart.fcr[1]);
			sms_mapper_w(0, cart.fcr[0]);
		}

		if (!smsvdp_tmsmode) {
			/* Force full pattern cache update when not in a TMS9918 mode */
			bg_list_index = 0x200;
			for(INT32 i = 0; i < 0x200; i++) {
				bg_name_list[i] = i;
				bg_name_dirty[i] = (UINT8)-1;
			}
			/* Restore palette */
			for(INT32 i = 0; i < PALETTE_SIZE; i++)
				palette_sync(i, 1);
		}
		viewport_check(); // fixes 4pak all action sstate screen corruption
	}
}

INT32 SMSScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029708;
	}

	if (nAction & ACB_VOLATILE) {
		ZetScan(nAction);
		SN76496Scan(nAction, pnMin);

		SCAN_VAR(vdp);
		SCAN_VAR(sms);
		SCAN_VAR(cart.fcr);
		if (sms.use_fm) // put it down here so we keep compatibility with non-fm states.
			BurnYM2413Scan(nAction);

		if (nAction & ACB_WRITE) {
			ZetOpen(0);
			system_load_state();
			ZetClose();
		}
	}

	return 0;
}

INT32 SMSGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}
   // remove sms_
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j+4];
	}

	*pszName = szFilename;

	return 0;
}

INT32 GGGetZipName(char** pszName, UINT32 i)
{
	static char szFilename[MAX_PATH];
	char* pszGameName = NULL;

	if (pszName == NULL) {
		return 1;
	}

	if (i == 0) {
		pszGameName = BurnDrvGetTextA(DRV_NAME);
	} else {
		pszGameName = BurnDrvGetTextA(DRV_PARENT);
	}

	if (pszGameName == NULL) {
		*pszName = NULL;
		return 1;
	}
   // remove sms_
	for (UINT32 j = 0; j < strlen(pszGameName); j++) {
		szFilename[j] = pszGameName[j+3];
	}

	*pszName = szFilename;

	return 0;
}

// 4 PAK All Action (Aus)

static struct BurnRomInfo sms_4pakRomDesc[] = {
	{ "4 pak all action (aus).bin",	0x100000, 0xa67f2a5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_4pak)
STD_ROM_FN(sms_4pak)

struct BurnDriver BurnDrvsms_4pak = {
	"sms_4pak", NULL, NULL, NULL, "1995",
	"4 PAK All Action (Aus)\0", NULL, "HES", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_4PAK, GBF_MISC, 0,
	SMSGetZipName, sms_4pakRomInfo, sms_4pakRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// 20 em 1 (Bra)

static struct BurnRomInfo sms_20em1RomDesc[] = {
	{ "20 em 1 (brazil).bin",	0x40000, 0xf0f35c22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_20em1)
STD_ROM_FN(sms_20em1)

struct BurnDriverD BurnDrvsms_20em1 = {
	"sms_20em1", NULL, NULL, NULL, "1995",
	"20 em 1 (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_20em1RomInfo, sms_20em1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Three Dragon Story (Kor)

static struct BurnRomInfo sms_3dragonRomDesc[] = {
	{ "three dragon story, the (k).bin",	0x08000, 0x8640e7e8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_3dragon)
STD_ROM_FN(sms_3dragon)

struct BurnDriver BurnDrvsms_3dragon = {
	"sms_3dragon", NULL, NULL, NULL, "19??",
	"The Three Dragon Story (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_3dragonRomInfo, sms_3dragonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// 94 Super World Cup Soccer (Kor)

static struct BurnRomInfo sms_94swcRomDesc[] = {
	{ "94 super world cup soccer (kr).bin",	0x40000, 0x060d6a7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_94swc)
STD_ROM_FN(sms_94swc)

struct BurnDriver BurnDrvsms_94swc = {
	"sms_94swc", NULL, NULL, NULL, "1994",
	"94 Super World Cup Soccer (Kor)\0", NULL, "Daou Infosys", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_94swcRomInfo, sms_94swcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ace of Aces (Euro)

static struct BurnRomInfo sms_aceofaceRomDesc[] = {
	{ "ace of aces (europe).bin",	0x40000, 0x887d9f6b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aceoface)
STD_ROM_FN(sms_aceoface)

struct BurnDriver BurnDrvsms_aceoface = {
	"sms_aceoface", NULL, NULL, NULL, "1991",
	"Ace of Aces (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM /*| HARDWARE_SMS_DISPLAY_PAL */, GBF_MISC, 0,
	SMSGetZipName, sms_aceofaceRomInfo, sms_aceofaceRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Action Fighter (Euro, USA, Bra, v2)

static struct BurnRomInfo sms_actionfgRomDesc[] = {
	{ "mpr-11043.ic1",	0x20000, 0x3658f3e0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_actionfg)
STD_ROM_FN(sms_actionfg)

struct BurnDriver BurnDrvsms_actionfg = {
	"sms_actionfg", NULL, NULL, NULL, "1986",
	"Action Fighter (Euro, USA, Bra, v2)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_actionfgRomInfo, sms_actionfgRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Action Fighter (Euro, Jpn, v1)

static struct BurnRomInfo sms_actionfg1RomDesc[] = {
	{ "action fighter (japan, europe) (v1.1).bin",	0x20000, 0xd91b340d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_actionfg1)
STD_ROM_FN(sms_actionfg1)

struct BurnDriver BurnDrvsms_actionfg1 = {
	"sms_actionfg1", "sms_actionfg", NULL, NULL, "1986",
	"Action Fighter (Euro, Jpn, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_actionfg1RomInfo, sms_actionfg1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Addams Family (Euro)

static struct BurnRomInfo sms_addfamRomDesc[] = {
	{ "addams family, the (europe).bin",	0x40000, 0x72420f38, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_addfam)
STD_ROM_FN(sms_addfam)

struct BurnDriver BurnDrvsms_addfam = {
	"sms_addfam", NULL, NULL, NULL, "1993",
	"The Addams Family (Euro)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_addfamRomInfo, sms_addfamRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aerial Assault (Euro, Bra)

static struct BurnRomInfo sms_aerialasRomDesc[] = {
	{ "aerial assault (europe).bin",	0x40000, 0xecf491cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aerialas)
STD_ROM_FN(sms_aerialas)

struct BurnDriver BurnDrvsms_aerialas = {
	"sms_aerialas", NULL, NULL, NULL, "1990",
	"Aerial Assault (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aerialasRomInfo, sms_aerialasRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aerial Assault (USA)

static struct BurnRomInfo sms_aerialasuRomDesc[] = {
	{ "mpr-13299-f.ic1",	0x40000, 0x15576613, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aerialasu)
STD_ROM_FN(sms_aerialasu)

struct BurnDriver BurnDrvsms_aerialasu = {
	"sms_aerialasu", "sms_aerialas", NULL, NULL, "1990",
	"Aerial Assault (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aerialasuRomInfo, sms_aerialasuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// After Burner (World)

static struct BurnRomInfo sms_aburnerRomDesc[] = {
	{ "mpr-11271 w01.ic2",	0x80000, 0x1c951f8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aburner)
STD_ROM_FN(sms_aburner)

struct BurnDriver BurnDrvsms_aburner = {
	"sms_aburner", NULL, NULL, NULL, "1987",
	"After Burner (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aburnerRomInfo, sms_aburnerRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Agigongnyong Dooly (Kor)

static struct BurnRomInfo sms_agidoolyRomDesc[] = {
	{ "agigongnyong dooly (kr).bin",	0x20000, 0x4f1be9fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_agidooly)
STD_ROM_FN(sms_agidooly)

struct BurnDriver BurnDrvsms_agidooly = {
	"sms_agidooly", "sms_dinodool", NULL, NULL, "1991",
	"Agigongnyong Dooly (Kor)\0", NULL, "Daou Infosys", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_agidoolyRomInfo, sms_agidoolyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Air Rescue (Euro, Bra, Kor)

static struct BurnRomInfo sms_airrescRomDesc[] = {
	{ "air rescue (europe).bin",	0x40000, 0x8b43d21d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_airresc)
STD_ROM_FN(sms_airresc)

struct BurnDriver BurnDrvsms_airresc = {
	"sms_airresc", NULL, NULL, NULL, "1992",
	"Air Rescue (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_airrescRomInfo, sms_airrescRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Aladdin (Euro, Bra, Kor)

static struct BurnRomInfo sms_aladdinRomDesc[] = {
	{ "aladdin (europe).bin",	0x80000, 0xc8718d40, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aladdin)
STD_ROM_FN(sms_aladdin)

struct BurnDriver BurnDrvsms_aladdin = {
	"sms_aladdin", NULL, NULL, NULL, "1994",
	"Disney's Aladdin (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aladdinRomInfo, sms_aladdinRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aleste (Jpn)

static struct BurnRomInfo sms_alesteRomDesc[] = {
	{ "aleste (japan).bin",	0x20000, 0xd8c4165b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aleste)
STD_ROM_FN(sms_aleste)

struct BurnDriver BurnDrvsms_aleste = {
	"sms_aleste", "sms_pstrike", NULL, NULL, "1988",
	"Aleste (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alesteRomInfo, sms_alesteRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aleste II gg2sms

static struct BurnRomInfo sms_aleste2gg2smsRomDesc[] = {
	{ "Aleste IIgg2sms.sms",	0x40000, 0x516d899d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aleste2gg2sms)
STD_ROM_FN(sms_aleste2gg2sms)

struct BurnDriver BurnDrvsms_aleste2gg2sms = {
	"sms_alesteiigg2sms", NULL, NULL, NULL, "1993",
	"Aleste II GG (SMS Conversion)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aleste2gg2smsRomInfo, sms_aleste2gg2smsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd BMX Trial (Jpn)

static struct BurnRomInfo sms_alexbmxRomDesc[] = {
	{ "alex kidd bmx trial (japan).bin",	0x20000, 0xf9dbb533, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexbmx)
STD_ROM_FN(sms_alexbmx)

struct BurnDriver BurnDrvsms_alexbmx = {
	"sms_alexbmx", NULL, NULL, NULL, "1987",
	"Alex Kidd BMX Trial (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexbmxRomInfo, sms_alexbmxRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd - High-Tech World (Euro, USA)

static struct BurnRomInfo sms_alexhitwRomDesc[] = {
	{ "mpr-12408.ic1",	0x20000, 0x013c0a94, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexhitw)
STD_ROM_FN(sms_alexhitw)

struct BurnDriver BurnDrvsms_alexhitw = {
	"sms_alexhitw", NULL, NULL, NULL, "1989",
	"Alex Kidd - High-Tech World (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexhitwRomInfo, sms_alexhitwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd in Miracle World (Euro, USA, v1)

static struct BurnRomInfo sms_alexkiddRomDesc[] = {
	{ "alex kidd in miracle world (usa, europe) (v1.1).bin",	0x20000, 0xaed9aac4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexkidd)
STD_ROM_FN(sms_alexkidd)

struct BurnDriver BurnDrvsms_alexkidd = {
	"sms_alexkidd", NULL, NULL, NULL, "1986",
	"Alex Kidd in Miracle World (Euro, USA, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexkiddRomInfo, sms_alexkiddRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd in Miracle World (Euro, USA, v0)

static struct BurnRomInfo sms_alexkidd1RomDesc[] = {
	{ "alex kidd in miracle world (usa, europe).bin",	0x20000, 0x17a40e29, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexkidd1)
STD_ROM_FN(sms_alexkidd1)

struct BurnDriver BurnDrvsms_alexkidd1 = {
	"sms_alexkidd1", "sms_alexkidd", NULL, NULL, "1986",
	"Alex Kidd in Miracle World (Euro, USA, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexkidd1RomInfo, sms_alexkidd1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd in Miracle World (Bra, v1, Pirate)

static struct BurnRomInfo sms_alexkiddbRomDesc[] = {
	{ "alex kidd in miracle world (b) [hack].bin",	0x20000, 0x7545d7c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexkiddb)
STD_ROM_FN(sms_alexkiddb)

struct BurnDriver BurnDrvsms_alexkiddb = {
	"sms_alexkiddb", "sms_alexkidd", NULL, NULL, "1986",
	"Alex Kidd in Miracle World (Bra, v1, Pirate)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexkiddbRomInfo, sms_alexkiddbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd no Miracle World (Jpn)

static struct BurnRomInfo sms_alexkiddjRomDesc[] = {
	{ "mpr-10379.ic1",	0x20000, 0x08c9ec91, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexkiddj)
STD_ROM_FN(sms_alexkiddj)

struct BurnDriver BurnDrvsms_alexkiddj = {
	"sms_alexkiddj", "sms_alexkidd", NULL, NULL, "1986",
	"Alex Kidd no Miracle World (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexkiddjRomInfo, sms_alexkiddjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd in Radaxian Rumble 1.02e

static struct BurnRomInfo sms_alexkiddrrRomDesc[] = {
	{ "akrr102e.sms",	0x80000, 0x368b64b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexkiddrr)
STD_ROM_FN(sms_alexkiddrr)

struct BurnDriver BurnDrvsms_alexkiddrr = {
	"sms_akrr102e", "sms_alexkidd", NULL, NULL, "2015",
	"Alex Kidd in Radaxian Rumble (ver. 1.02e)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexkiddrrRomInfo, sms_alexkiddrrRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd - The Lost Stars (World)

static struct BurnRomInfo sms_alexlostRomDesc[] = {
	{ "mpr-11402 w09.ic1",	0x40000, 0xc13896d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexlost)
STD_ROM_FN(sms_alexlost)

struct BurnDriver BurnDrvsms_alexlost = {
	"sms_alexlost", NULL, NULL, NULL, "1988",
	"Alex Kidd - The Lost Stars (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexlostRomInfo, sms_alexlostRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd in Shinobi World (Euro, USA, Bra)

static struct BurnRomInfo sms_alexshinRomDesc[] = {
	{ "alex kidd in shinobi world (usa, europe).bin",	0x40000, 0xd2417ed7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alexshin)
STD_ROM_FN(sms_alexshin)

struct BurnDriver BurnDrvsms_alexshin = {
	"sms_alexshin", NULL, NULL, NULL, "1990",
	"Alex Kidd in Shinobi World (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexshinRomInfo, sms_alexshinRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alf (USA, Bra)

static struct BurnRomInfo sms_alfRomDesc[] = {
	{ "alf (usa).bin",	0x20000, 0x82038ad4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alf)
STD_ROM_FN(sms_alf)

struct BurnDriver BurnDrvsms_alf = {
	"sms_alf", NULL, NULL, NULL, "1989",
	"Alf (USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alfRomInfo, sms_alfRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alibaba and 40 Thieves (Kor)

static struct BurnRomInfo sms_alibabaRomDesc[] = {
	{ "alibaba and 40 thieves (kr).sms",	0x08000, 0x08bf3de3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alibaba)
STD_ROM_FN(sms_alibaba)

struct BurnDriver BurnDrvsms_alibaba = {
	"sms_alibaba", NULL, NULL, NULL, "1989",
	"Alibaba and 40 Thieves (Kor)\0", NULL, "HiCom", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_alibabaRomInfo, sms_alibabaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien³ (Euro, Bra)

static struct BurnRomInfo sms_alien3RomDesc[] = {
	{ "alien 3 (europe).bin",	0x40000, 0xb618b144, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alien3)
STD_ROM_FN(sms_alien3)

struct BurnDriver BurnDrvsms_alien3 = {
	"sms_alien3", NULL, NULL, NULL, "1992",
	"Alien 3 (Euro, Bra)\0", NULL, "Arena", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alien3RomInfo, sms_alien3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Altered Beast (Euro, USA, Bra)

static struct BurnRomInfo sms_altbeastRomDesc[] = {
	{ "mpr-12534 t35.ic2",	0x40000, 0xbba2fe98, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_altbeast)
STD_ROM_FN(sms_altbeast)

struct BurnDriver BurnDrvsms_altbeast = {
	"sms_altbeast", NULL, NULL, NULL, "1989",
	"Altered Beast (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_altbeastRomInfo, sms_altbeastRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// American Baseball (Euro, Bra)

static struct BurnRomInfo sms_ameribbRomDesc[] = {
	{ "mpr-12555f.ic1",	0x40000, 0x7b27202f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ameribb)
STD_ROM_FN(sms_ameribb)

struct BurnDriver BurnDrvsms_ameribb = {
	"sms_ameribb", NULL, NULL, NULL, "1989",
	"American Baseball (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ameribbRomInfo, sms_ameribbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// American Pro Football (Euro)

static struct BurnRomInfo sms_ameripfRomDesc[] = {
	{ "american pro football (europe).bin",	0x40000, 0x3727d8b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ameripf)
STD_ROM_FN(sms_ameripf)

struct BurnDriver BurnDrvsms_ameripf = {
	"sms_ameripf", NULL, NULL, NULL, "1989",
	"American Pro Football (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ameripfRomInfo, sms_ameripfRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Andre Agassi Tennis (Euro, Bra)

static struct BurnRomInfo sms_agassiRomDesc[] = {
	{ "andre agassi tennis (europe).bin",	0x40000, 0xf499034d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_agassi)
STD_ROM_FN(sms_agassi)

struct BurnDriver BurnDrvsms_agassi = {
	"sms_agassi", NULL, NULL, NULL, "1993",
	"Andre Agassi Tennis (Euro, Bra)\0", NULL, "TecMagik", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_agassiRomInfo, sms_agassiRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Anmitsu Hime (Jpn)

static struct BurnRomInfo sms_anmitsuRomDesc[] = {
	{ "anmitsu hime (japan).bin",	0x20000, 0xfff63b0b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_anmitsu)
STD_ROM_FN(sms_anmitsu)

struct BurnDriver BurnDrvsms_anmitsu = {
	"sms_anmitsu", "sms_alexhitw", NULL, NULL, "1987",
	"Anmitsu Hime (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_anmitsuRomInfo, sms_anmitsuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Arcade Smash Hits (Euro)

static struct BurnRomInfo sms_arcadeshRomDesc[] = {
	{ "arcade smash hits (europe).bin",	0x40000, 0xe4163163, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_arcadesh)
STD_ROM_FN(sms_arcadesh)

struct BurnDriver BurnDrvsms_arcadesh = {
	"sms_arcadesh", NULL, NULL, NULL, "1992",
	"Arcade Smash Hits (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_arcadeshRomInfo, sms_arcadeshRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Argos no Juujiken (Jpn)

static struct BurnRomInfo sms_argosnjRomDesc[] = {
	{ "argos no juujiken (japan).bin",	0x20000, 0xbae75805, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_argosnj)
STD_ROM_FN(sms_argosnj)

struct BurnDriver BurnDrvsms_argosnj = {
	"sms_argosnj", NULL, NULL, NULL, "1988",
	"Argos no Juujiken (Jpn)\0", NULL, "Tecmo", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_argosnjRomInfo, sms_argosnjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Argos no Juujiken (Jpn, Pirate?)

static struct BurnRomInfo sms_argosnj1RomDesc[] = {
	{ "argos no juujiken [hack].bin",	0x20000, 0x32da4b0d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_argosnj1)
STD_ROM_FN(sms_argosnj1)

struct BurnDriver BurnDrvsms_argosnj1 = {
	"sms_argosnj1", "sms_argosnj", NULL, NULL, "1988",
	"Argos no Juujiken (Jpn, Pirate?)\0", NULL, "Tecmo", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_argosnj1RomInfo, sms_argosnj1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Ariel The Little Mermaid (Bra)

static struct BurnRomInfo sms_arielRomDesc[] = {
	{ "ariel - the little mermaid (brazil).bin",	0x40000, 0xf4b3a7bd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ariel)
STD_ROM_FN(sms_ariel)

struct BurnDriver BurnDrvsms_ariel = {
	"sms_ariel", NULL, NULL, NULL, "1997",
	"Disney's Ariel The Little Mermaid (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_arielRomInfo, sms_arielRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ashura (Jpn)

static struct BurnRomInfo sms_ashuraRomDesc[] = {
	{ "mpr-10386.ic1",	0x20000, 0xae705699, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ashura)
STD_ROM_FN(sms_ashura)

struct BurnDriver BurnDrvsms_ashura = {
	"sms_ashura", "sms_secret", NULL, NULL, "1986",
	"Ashura (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ashuraRomInfo, sms_ashuraRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Assault City (Euro, Bra, Light Phaser version)

static struct BurnRomInfo sms_assaultcRomDesc[] = {
	{ "assault city (europe) (light phaser).bin",	0x40000, 0x861b6e79, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_assaultc)
STD_ROM_FN(sms_assaultc)

struct BurnDriver BurnDrvsms_assaultc = {
	"sms_assaultc", NULL, NULL, NULL, "1990",
	"Assault City (Euro, Bra, Light Phaser version)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_assaultcRomInfo, sms_assaultcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Assault City (Euro)

static struct BurnRomInfo sms_assaultc1RomDesc[] = {
	{ "assault city (europe).bin",	0x40000, 0x0bd8da96, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_assaultc1)
STD_ROM_FN(sms_assaultc1)

struct BurnDriver BurnDrvsms_assaultc1 = {
	"sms_assaultc1", "sms_assaultc", NULL, NULL, "1990",
	"Assault City (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_assaultc1RomInfo, sms_assaultc1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (Euro, Bra)

static struct BurnRomInfo sms_astergreRomDesc[] = {
	{ "asterix and the great rescue (europe) (en,fr,de,es,it).bin",	0x80000, 0xf9b7d26b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_astergre)
STD_ROM_FN(sms_astergre)

struct BurnDriver BurnDrvsms_astergre = {
	"sms_astergre", NULL, NULL, NULL, "1993",
	"Asterix and the Great Rescue (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_astergreRomInfo, sms_astergreRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Asterix (Euro, v1)

static struct BurnRomInfo sms_asterixRomDesc[] = {
	{ "asterix (europe) (en,fr) (v1.1).bin",	0x80000, 0x8c9d5be8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_asterix)
STD_ROM_FN(sms_asterix)

struct BurnDriver BurnDrvsms_asterix = {
	"sms_asterix", NULL, NULL, NULL, "1991",
	"Asterix (Euro, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_asterixRomInfo, sms_asterixRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Asterix (Euro, Bra, v0)

static struct BurnRomInfo sms_asterix1RomDesc[] = {
	{ "mpr-14520-f.ic1",	0x80000, 0x147e02fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_asterix1)
STD_ROM_FN(sms_asterix1)

struct BurnDriver BurnDrvsms_asterix1 = {
	"sms_asterix1", "sms_asterix", NULL, NULL, "1991",
	"Asterix (Euro, Bra, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_asterix1RomInfo, sms_asterix1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Asterix and the Secret Mission (Euro)

static struct BurnRomInfo sms_astermisRomDesc[] = {
	{ "asterix and the secret mission (europe) (en,fr,de).bin",	0x80000, 0xdef9b14e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_astermis)
STD_ROM_FN(sms_astermis)

struct BurnDriver BurnDrvsms_astermis = {
	"sms_astermis", NULL, NULL, NULL, "1993",
	"Asterix and the Secret Mission (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_astermisRomInfo, sms_astermisRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien Storm (Euro, Bra)

static struct BurnRomInfo sms_astormRomDesc[] = {
	{ "mpr-14254.ic1",	0x40000, 0x7f30f793, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_astorm)
STD_ROM_FN(sms_astorm)

struct BurnDriver BurnDrvsms_astorm = {
	"sms_astorm", NULL, NULL, NULL, "1991",
	"Alien Storm (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_astormRomInfo, sms_astormRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astro Flash (Jpn, Pirate)

static struct BurnRomInfo sms_astrofl1RomDesc[] = {
	{ "astro flash [hack].bin",	0x08000, 0x0e21e6cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_astrofl1)
STD_ROM_FN(sms_astrofl1)

struct BurnDriver BurnDrvsms_astrofl1 = {
	"sms_astrofl1", "sms_transbot", NULL, NULL, "1985",
	"Astro Flash (Jpn, Pirate)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_astrofl1RomInfo, sms_astrofl1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astro Warrior &amp; Pit Pot (Euro)

static struct BurnRomInfo sms_astropitRomDesc[] = {
	{ "mpr-11070.ic1",	0x20000, 0x69efd483, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_astropit)
STD_ROM_FN(sms_astropit)

struct BurnDriver BurnDrvsms_astropit = {
	"sms_astropit", NULL, NULL, NULL, "1987",
	"Astro Warrior and Pit Pot (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_astropitRomInfo, sms_astropitRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astro Warrior (Jpn, USA, Bra)

static struct BurnRomInfo sms_astrowRomDesc[] = {
	{ "mpr-10387.ic1",	0x20000, 0x299cbb74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_astrow)
STD_ROM_FN(sms_astrow)

struct BurnDriver BurnDrvsms_astrow = {
	"sms_astrow", NULL, NULL, NULL, "1986",
	"Astro Warrior (Jpn, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_astrowRomInfo, sms_astrowRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien Syndrome (Euro, USA, Bra)

static struct BurnRomInfo sms_aliensynRomDesc[] = {
	{ "mpr-11384 w10.ic1",	0x40000, 0x5cbfe997, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aliensyn)
STD_ROM_FN(sms_aliensyn)

struct BurnDriver BurnDrvsms_aliensyn = {
	"sms_aliensyn", NULL, NULL, NULL, "1987",
	"Alien Syndrome (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aliensynRomInfo, sms_aliensynRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien Syndrome (Jpn)

static struct BurnRomInfo sms_aliensynjRomDesc[] = {
	{ "alien syndrome (japan).bin",	0x40000, 0x4cc11df9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aliensynj)
STD_ROM_FN(sms_aliensynj)

struct BurnDriver BurnDrvsms_aliensynj = {
	"sms_aliensynj", "sms_aliensyn", NULL, NULL, "1987",
	"Alien Syndrome (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aliensynjRomInfo, sms_aliensynjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien Syndrome (Prototype)

static struct BurnRomInfo sms_aliensynpRomDesc[] = {
	{ "alien syndrome (proto).bin",	0x40000, 0xc148868c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aliensynp)
STD_ROM_FN(sms_aliensynp)

struct BurnDriver BurnDrvsms_aliensynp = {
	"sms_aliensynp", "sms_aliensyn", NULL, NULL, "1987",
	"Alien Syndrome (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aliensynpRomInfo, sms_aliensynpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ayrton Senna's Super Monaco GP II (Euro, Bra, Kor)

static struct BurnRomInfo sms_smgp2RomDesc[] = {
	{ "ayrton senna's super monaco gp ii (europe).bin",	0x80000, 0xe890331d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smgp2)
STD_ROM_FN(sms_smgp2)

struct BurnDriver BurnDrvsms_smgp2 = {
	"sms_smgp2", NULL, NULL, NULL, "1992",
	"Ayrton Senna's Super Monaco GP II (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smgp2RomInfo, sms_smgp2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aztec Adventure - The Golden Road to Paradise (Euro, USA) ~ Nazca '88 - The Golden Road to Paradise (Jpn)

static struct BurnRomInfo sms_aztecadvRomDesc[] = {
	{ "mpr-11121.ic1",	0x20000, 0xff614eb3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_aztecadv)
STD_ROM_FN(sms_aztecadv)

struct BurnDriver BurnDrvsms_aztecadv = {
	"sms_aztecadv", NULL, NULL, NULL, "1987",
	"Aztec Adventure - The Golden Road to Paradise (Euro, USA) ~ Nazca '88 - The Golden Road to Paradise (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aztecadvRomInfo, sms_aztecadvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Back to the Future Part II (Euro, Bra)

static struct BurnRomInfo sms_backtof2RomDesc[] = {
	{ "back to the future part ii (europe).bin",	0x40000, 0xe5ff50d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_backtof2)
STD_ROM_FN(sms_backtof2)

struct BurnDriver BurnDrvsms_backtof2 = {
	"sms_backtof2", NULL, NULL, NULL, "1990",
	"Back to the Future Part II (Euro, Bra)\0", NULL, "Image Works", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_backtof2RomInfo, sms_backtof2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Back to the Future Part III (Euro)

static struct BurnRomInfo sms_backtof3RomDesc[] = {
	{ "back to the future part iii (europe).bin",	0x40000, 0x2d48c1d3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_backtof3)
STD_ROM_FN(sms_backtof3)

struct BurnDriver BurnDrvsms_backtof3 = {
	"sms_backtof3", NULL, NULL, NULL, "1992",
	"Back to the Future Part III (Euro)\0", NULL, "Image Works", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_backtof3RomInfo, sms_backtof3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Baku Baku Animal (Bra)

static struct BurnRomInfo sms_bakubakuRomDesc[] = {
	{ "baku baku animal (brazil).bin",	0x40000, 0x35d84dc2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bakubaku)
STD_ROM_FN(sms_bakubaku)

struct BurnDriver BurnDrvsms_bakubaku = {
	"sms_bakubaku", NULL, NULL, NULL, "1996",
	"Baku Baku Animal (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bakubakuRomInfo, sms_bakubakuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bank Panic (Euro)

static struct BurnRomInfo sms_bankpRomDesc[] = {
	{ "mpr-12554.ic1",	0x08000, 0x655fb1f4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bankp)
STD_ROM_FN(sms_bankp)

struct BurnDriver BurnDrvsms_bankp = {
	"sms_bankp", NULL, NULL, NULL, "1987",
	"Bank Panic (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bankpRomInfo, sms_bankpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Simpsons - Bart vs. The Space Mutants (Euro, Bra)

static struct BurnRomInfo sms_bartvssmRomDesc[] = {
	{ "simpsons, the - bart vs. the space mutants (europe).bin",	0x40000, 0xd1cc08ee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bartvssm)
STD_ROM_FN(sms_bartvssm)

struct BurnDriver BurnDrvsms_bartvssm = {
	"sms_bartvssm", NULL, NULL, NULL, "1992",
	"The Simpsons - Bart vs. The Space Mutants (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bartvssmRomInfo, sms_bartvssmRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Simpsons - Bart vs. The World (Euro, Bra)

static struct BurnRomInfo sms_bartvswRomDesc[] = {
	{ "mpr-15988.ic1",	0x40000, 0xf6b2370a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bartvsw)
STD_ROM_FN(sms_bartvsw)

struct BurnDriver BurnDrvsms_bartvsw = {
	"sms_bartvsw", NULL, NULL, NULL, "1993",
	"The Simpsons - Bart vs. The World (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bartvswRomInfo, sms_bartvswRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Basket Ball Nightmare (Euro, Bra)

static struct BurnRomInfo sms_basketnRomDesc[] = {
	{ "mpr-12728.ic1",	0x40000, 0x0df8597f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_basketn)
STD_ROM_FN(sms_basketn)

struct BurnDriver BurnDrvsms_basketn = {
	"sms_basketn", NULL, NULL, NULL, "1989",
	"Basket Ball Nightmare (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_basketnRomInfo, sms_basketnRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Batman Returns (Euro, Bra)

static struct BurnRomInfo sms_batmanrnRomDesc[] = {
	{ "batman returns (europe).bin",	0x40000, 0xb154ec38, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_batmanrn)
STD_ROM_FN(sms_batmanrn)

struct BurnDriver BurnDrvsms_batmanrn = {
	"sms_batmanrn", NULL, NULL, NULL, "1993",
	"Batman Returns (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_batmanrnRomInfo, sms_batmanrnRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Battlemaniacs (Bra)

static struct BurnRomInfo sms_bmaniacsRomDesc[] = {
	{ "battlemaniacs (brazil).bin",	0x40000, 0x1cbb7bf1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bmaniacs)
STD_ROM_FN(sms_bmaniacs)

struct BurnDriver BurnDrvsms_bmaniacs = {
	"sms_bmaniacs", NULL, NULL, NULL, "1994",
	"Battlemaniacs (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bmaniacsRomInfo, sms_bmaniacsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Battle Out Run (Euro, Bra)

static struct BurnRomInfo sms_battleorRomDesc[] = {
	{ "battle out run (europe).bin",	0x40000, 0xc19430ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_battleor)
STD_ROM_FN(sms_battleor)

struct BurnDriver BurnDrvsms_battleor = {
	"sms_battleor", NULL, NULL, NULL, "1989",
	"Battle Out Run (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_battleorRomInfo, sms_battleorRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shadow of the Beast (Euro, Bra)

static struct BurnRomInfo sms_beastRomDesc[] = {
	{ "shadow of the beast (europe).bin",	0x40000, 0x1575581d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_beast)
STD_ROM_FN(sms_beast)

struct BurnDriver BurnDrvsms_beast = {
	"sms_beast", NULL, NULL, NULL, "1991",
	"Shadow of the Beast (Euro, Bra)\0", NULL, "TecMagik", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_beastRomInfo, sms_beastRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection - Hang On + Pit Pot + Spy vs Spy (Kor)

static struct BurnRomInfo sms_hicom3aRomDesc[] = {
	{ "hi-com 3-in-1 the best game collection a (kr).sms",	0x20000, 0x98af0236, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom3a)
STD_ROM_FN(sms_hicom3a)

struct BurnDriver BurnDrvsms_hicom3a = {
	"sms_hicom3a", NULL, NULL, NULL, "1990",
	"The Best Game Collection - Hang On + Pit Pot + Spy vs Spy (Kor)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom3aRomInfo, sms_hicom3aRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection - Great Baseball + Great Soccer + Super Tennis (Kor)

static struct BurnRomInfo sms_hicom3bRomDesc[] = {
	{ "hi-com 3-in-1 the best game collection b (kr).sms",	0x20000, 0x6ebfe1c3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom3b)
STD_ROM_FN(sms_hicom3b)

struct BurnDriver BurnDrvsms_hicom3b = {
	"sms_hicom3b", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection - Great Baseball + Great Soccer + Super Tennis (Kor)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom3bRomInfo, sms_hicom3bRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection - Teddy Boy Blues + Pit-Pot + Astro Flash (Kor)

static struct BurnRomInfo sms_hicom3cRomDesc[] = {
	{ "hi-com 3-in-1 the best game collection c (kr).sms",	0x20000, 0x81a36a4f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom3c)
STD_ROM_FN(sms_hicom3c)

struct BurnDriver BurnDrvsms_hicom3c = {
	"sms_hicom3c", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection - Teddy Boy Blues + Pit-Pot + Astro Flash (Kor)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom3cRomInfo, sms_hicom3cRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection - Teddy Boy Blues + Great Soccer + Comical Machine Gun Joe (Kor)

static struct BurnRomInfo sms_hicom3dRomDesc[] = {
	{ "hi-com 3-in-1 the best game collection d (kr).sms",	0x20000, 0x8d2d695d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom3d)
STD_ROM_FN(sms_hicom3d)

struct BurnDriver BurnDrvsms_hicom3d = {
	"sms_hicom3d", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection - Teddy Boy Blues + Great Soccer + Comical Machine Gun Joe (Kor)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom3dRomInfo, sms_hicom3dRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection - Ghost House + Teddy Boy Blues + Seishun Scandal (Kor)

static struct BurnRomInfo sms_hicom3eRomDesc[] = {
	{ "hi-com 3-in-1 the best game collection e (kr).sms",	0x20000, 0x82c09b57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom3e)
STD_ROM_FN(sms_hicom3e)

struct BurnDriver BurnDrvsms_hicom3e = {
	"sms_hicom3e", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection - Ghost House + Teddy Boy Blues + Seishun Scandal (Kor)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom3eRomInfo, sms_hicom3eRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection - Satellite-7 + Great Baseball + Seishun Scandal (Kor)

static struct BurnRomInfo sms_hicom3fRomDesc[] = {
	{ "hi-com 3-in-1 the best game collection f (kr).sms",	0x20000, 0x4088eeb4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom3f)
STD_ROM_FN(sms_hicom3f)

struct BurnDriver BurnDrvsms_hicom3f = {
	"sms_hicom3f", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection - Satellite-7 + Great Baseball + Seishun Scandal (Kor)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom3fRomInfo, sms_hicom3fRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection (Kor, 8 in 1 Ver. A)

static struct BurnRomInfo sms_hicom8aRomDesc[] = {
	{ "hi-com 8-in-1 the best game collection a (kr).sms",	0x40000, 0xfba94148, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom8a)
STD_ROM_FN(sms_hicom8a)

struct BurnDriver BurnDrvsms_hicom8a = {
	"sms_hicom8a", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection (Kor, 8 in 1 Ver. A)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom8aRomInfo, sms_hicom8aRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection (Kor, 8 in 1 Ver. B)

static struct BurnRomInfo sms_hicom8bRomDesc[] = {
	{ "hi-com 8-in-1 the best game collection b (kr).sms",	0x40000, 0x8333c86e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom8b)
STD_ROM_FN(sms_hicom8b)

struct BurnDriver BurnDrvsms_hicom8b = {
	"sms_hicom8b", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection (Kor, 8 in 1 Ver. B)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom8bRomInfo, sms_hicom8bRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Best Game Collection (Kor, 8 in 1 Ver. C)

static struct BurnRomInfo sms_hicom8cRomDesc[] = {
	{ "hi-com 8-in-1 the best game collection c (kr).sms",	0x40000, 0x00e9809f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hicom8c)
STD_ROM_FN(sms_hicom8c)

struct BurnDriver BurnDrvsms_hicom8c = {
	"sms_hicom8c", "sms_hicom3a", NULL, NULL, "1990",
	"The Best Game Collection (Kor, 8 in 1 Ver. C)\0", NULL, "Hi-Com", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_XIN1, GBF_MISC, 0,
	SMSGetZipName, sms_hicom8cRomInfo, sms_hicom8cRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Black Belt (Euro, USA)

static struct BurnRomInfo sms_blackbltRomDesc[] = {
	{ "black belt (usa, europe).bin",	0x20000, 0xda3a2f57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_blackblt)
STD_ROM_FN(sms_blackblt)

struct BurnDriver BurnDrvsms_blackblt = {
	"sms_blackblt", NULL, NULL, NULL, "1986",
	"Black Belt (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_blackbltRomInfo, sms_blackbltRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Blade Eagle (World)

static struct BurnRomInfo sms_bladeagRomDesc[] = {
	{ "mpr-11421.ic1",	0x40000, 0x8ecd201c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bladeag)
STD_ROM_FN(sms_bladeag)

struct BurnDriver BurnDrvsms_bladeag = {
	"sms_bladeag", NULL, NULL, NULL, "1988",
	"Blade Eagle (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bladeagRomInfo, sms_bladeagRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Blade Eagle (USA, Prototype)

static struct BurnRomInfo sms_bladeag1RomDesc[] = {
	{ "blade eagle (world) (beta).bin",	0x40000, 0x58d5fc48, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bladeag1)
STD_ROM_FN(sms_bladeag1)

struct BurnDriver BurnDrvsms_bladeag1 = {
	"sms_bladeag1", "sms_bladeag", NULL, NULL, "1988",
	"Blade Eagle (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bladeag1RomInfo, sms_bladeag1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Block Hole (Kor)

static struct BurnRomInfo sms_blockholRomDesc[] = {
	{ "block hole (kr).sms",	0x08000, 0x643b6b76, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_blockhol)
STD_ROM_FN(sms_blockhol)

struct BurnDriver BurnDrvsms_blockhol = {
	"sms_blockhol", NULL, NULL, NULL, "1990",
	"Block Hole (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_blockholRomInfo, sms_blockholRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bomber Raid (World)

static struct BurnRomInfo sms_bombraidRomDesc[] = {
	{ "mpr-12043f.ic1",	0x40000, 0x3084cf11, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bombraid)
STD_ROM_FN(sms_bombraid)

struct BurnDriver BurnDrvsms_bombraid = {
	"sms_bombraid", NULL, NULL, NULL, "1989",
	"Bomber Raid (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bombraidRomInfo, sms_bombraidRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bonanza Bros. (Euro, Bra)

static struct BurnRomInfo sms_bnzabrosRomDesc[] = {
	{ "bonanza bros. (europe).bin",	0x40000, 0xcaea8002, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bnzabros)
STD_ROM_FN(sms_bnzabros)

struct BurnDriver BurnDrvsms_bnzabros = {
	"sms_bnzabros", NULL, NULL, NULL, "1991",
	"Bonanza Bros. (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bnzabrosRomInfo, sms_bnzabrosRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Bonkers Wax Up! (Bra)

static struct BurnRomInfo sms_bonkersRomDesc[] = {
	{ "bonkers wax up! (brazil).bin",	0x80000, 0xb3768a7a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bonkers)
STD_ROM_FN(sms_bonkers)

struct BurnDriver BurnDrvsms_bonkers = {
	"sms_bonkers", NULL, NULL, NULL, "1994",
	"Disney's Bonkers Wax Up! (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_bonkersRomInfo, sms_bonkersRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chouon Senshi Borgman (Jpn)

static struct BurnRomInfo sms_borgmanRomDesc[] = {
	{ "chouon senshi borgman (japan).bin",	0x20000, 0xe421e466, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_borgman)
STD_ROM_FN(sms_borgman)

struct BurnDriver BurnDrvsms_borgman = {
	"sms_borgman", "sms_cyborgh", NULL, NULL, "1988",
	"Chouon Senshi Borgman (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_borgmanRomInfo, sms_borgmanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chouon Senshi Borgman (Jpn, Prototype)

static struct BurnRomInfo sms_borgmanpRomDesc[] = {
	{ "chouon senshi borgman [proto] (jp).bin",	0x20000, 0x86f49ae9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_borgmanp)
STD_ROM_FN(sms_borgmanp)

struct BurnDriver BurnDrvsms_borgmanp = {
	"sms_borgmanp", "sms_cyborgh", NULL, NULL, "1988",
	"Chouon Senshi Borgman (Jpn, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_borgmanpRomInfo, sms_borgmanpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bram Stoker's Dracula (Euro)

static struct BurnRomInfo sms_draculaRomDesc[] = {
	{ "bram stoker's dracula (europe).bin",	0x40000, 0x1b10a951, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dracula)
STD_ROM_FN(sms_dracula)

struct BurnDriver BurnDrvsms_dracula = {
	"sms_dracula", NULL, NULL, NULL, "1993",
	"Bram Stoker's Dracula (Euro)\0", NULL, "Sony Imagesoft", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_draculaRomInfo, sms_draculaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bubble Bobble (Kor, Clover)

static struct BurnRomInfo sms_bublbokcRomDesc[] = {
	{ "bobble bobble [clover] (kr).bin",	0x08000, 0x25eb9f80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bublbokc)
STD_ROM_FN(sms_bublbokc)

struct BurnDriver BurnDrvsms_bublbokc = {
	"sms_bublbokc", "sms_bublbobl", NULL, NULL, "1990",
	"Bubble Bobble (Kor, Clover)\0", NULL, "Clover", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bublbokcRomInfo, sms_bublbokcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bubble Bobble (Kor, YM Soft)

static struct BurnRomInfo sms_bublbokyRomDesc[] = {
	{ "power boggle boggle (kr).bin",	0x08000, 0x6d309ac5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bublboky)
STD_ROM_FN(sms_bublboky)

struct BurnDriver BurnDrvsms_bublboky = {
	"sms_bublboky", "sms_bublbobl", NULL, NULL, "1990",
	"Bubble Bobble (Kor, YM Soft)\0", NULL, "YM Soft", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_bublbokyRomInfo, sms_bublbokyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Bubble Bobble (Kor)

static struct BurnRomInfo sms_suprbublRomDesc[] = {
	{ "super bubble bobble (kr).bin",	0x08000, 0x22c09cfd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_suprbubl)
STD_ROM_FN(sms_suprbubl)

struct BurnDriver BurnDrvsms_suprbubl = {
	"sms_suprbubl", "sms_bublbobl", NULL, NULL, "1989",
	"Super Bubble Bobble (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_suprbublRomInfo, sms_suprbublRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// New Boggle Boggle 2 (Kor)

static struct BurnRomInfo sms_newbogl2RomDesc[] = {
	{ "new boggle boggle (kr).bin",	0x08000, 0xdbbf4dd1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_newbogl2)
STD_ROM_FN(sms_newbogl2)

struct BurnDriver BurnDrvsms_newbogl2 = {
	"sms_newbogl2", "sms_bublbobl", NULL, NULL, "1989",
	"New Boggle Boggle 2 (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_newbogl2RomInfo, sms_newbogl2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bubble Bobble (Euro, Bra)

static struct BurnRomInfo sms_bublboblRomDesc[] = {
	{ "mpr-14177-f.ic1",	0x40000, 0xe843ba7e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bublbobl)
STD_ROM_FN(sms_bublbobl)

struct BurnDriver BurnDrvsms_bublbobl = {
	"sms_bublbobl", NULL, NULL, NULL, "1988",
	"Bubble Bobble (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bublboblRomInfo, sms_bublboblRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Buggy Run (Euro, Bra)

static struct BurnRomInfo sms_buggyrunRomDesc[] = {
	{ "mpr-15984 w06.ic1",	0x80000, 0xb0fc4577, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_buggyrun)
STD_ROM_FN(sms_buggyrun)

struct BurnDriver BurnDrvsms_buggyrun = {
	"sms_buggyrun", NULL, NULL, NULL, "1993",
	"Buggy Run (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_buggyrunRomInfo, sms_buggyrunRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// C_So! (Kor)

static struct BurnRomInfo sms_csoRomDesc[] = {
	{ "c_so! [msx] (kr).bin",	0x08000, 0x0918fba0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cso)
STD_ROM_FN(sms_cso)

struct BurnDriver BurnDrvsms_cso = {
	"sms_cso", NULL, NULL, NULL, "198?",
	"C_So! (Kor)\0", NULL, "Joy Soft", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_csoRomInfo, sms_csoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// California Games II (Euro)

static struct BurnRomInfo sms_calgame2RomDesc[] = {
	{ "mpr-15643-f.ic1",	0x40000, 0xc0e25d62, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_calgame2)
STD_ROM_FN(sms_calgame2)

struct BurnDriver BurnDrvsms_calgame2 = {
	"sms_calgame2", NULL, NULL, NULL, "1993",
	"California Games II (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_calgame2RomInfo, sms_calgame2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// California Games II (Bra, Kor)

static struct BurnRomInfo sms_calgame2bRomDesc[] = {
	{ "california games ii (brazil, korea).bin",	0x40000, 0x45c50294, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_calgame2b)
STD_ROM_FN(sms_calgame2b)

struct BurnDriver BurnDrvsms_calgame2b = {
	"sms_calgame2b", "sms_calgame2", NULL, NULL, "1993",
	"California Games II (Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_calgame2bRomInfo, sms_calgame2bRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// California Games (Euro, USA, Bra)

static struct BurnRomInfo sms_calgamesRomDesc[] = {
	{ "mpr-12342f.ic1",	0x40000, 0xac6009a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_calgames)
STD_ROM_FN(sms_calgames)

struct BurnDriver BurnDrvsms_calgames = {
	"sms_calgames", NULL, NULL, NULL, "1989",
	"California Games (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_calgamesRomInfo, sms_calgamesRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Captain Silver (Euro, Jpn)

static struct BurnRomInfo sms_captsilvRomDesc[] = {
	{ "mpr-11457f.ic1",	0x40000, 0xa4852757, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_captsilv)
STD_ROM_FN(sms_captsilv)

struct BurnDriver BurnDrvsms_captsilv = {
	"sms_captsilv", NULL, NULL, NULL, "1988",
	"Captain Silver (Euro, Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_captsilvRomInfo, sms_captsilvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Captain Silver (USA)

static struct BurnRomInfo sms_captsilvuRomDesc[] = {
	{ "captain silver (usa).bin",	0x20000, 0xb81f6fa5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_captsilvu)
STD_ROM_FN(sms_captsilvu)

struct BurnDriver BurnDrvsms_captsilvu = {
	"sms_captsilvu", "sms_captsilv", NULL, NULL, "1988",
	"Captain Silver (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_captsilvuRomInfo, sms_captsilvuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Casino Games (Euro, USA)

static struct BurnRomInfo sms_casinoRomDesc[] = {
	{ "mpr-13167.ic1",	0x40000, 0x3cff6e80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_casino)
STD_ROM_FN(sms_casino)

struct BurnDriver BurnDrvsms_casino = {
	"sms_casino", NULL, NULL, NULL, "1989",
	"Casino Games (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_casinoRomInfo, sms_casinoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Castelo Rá-Tim-Bum (Bra)

static struct BurnRomInfo sms_casteloRomDesc[] = {
	{ "castelo ra-tim-bum (brazil).bin",	0x80000, 0x31ffd7c3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_castelo)
STD_ROM_FN(sms_castelo)

struct BurnDriver BurnDrvsms_castelo = {
	"sms_castelo", NULL, NULL, NULL, "1997",
	"Castelo Ra-Tim-Bum (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_casteloRomInfo, sms_casteloRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Castle of Illusion Starring Mickey Mouse (Euro, Bra)

static struct BurnRomInfo sms_castlillRomDesc[] = {
	{ "mpr-13584a.ic1",	0x40000, 0x953f42e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_castlill)
STD_ROM_FN(sms_castlill)

struct BurnDriver BurnDrvsms_castlill = {
	"sms_castlill", NULL, NULL, NULL, "1990",
	"Castle of Illusion Starring Mickey Mouse (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_castlillRomInfo, sms_castlillRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Castle of Illusion Starring Mickey Mouse (USA, Display Unit Sample)

static struct BurnRomInfo sms_castlillsRomDesc[] = {
	{ "castle of illusion - starring mickey mouse (unknown) (sample).bin",	0x08000, 0xbd610939, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_castlills)
STD_ROM_FN(sms_castlills)

struct BurnDriver BurnDrvsms_castlills = {
	"sms_castlills", "sms_castlill", NULL, NULL, "1990",
	"Castle of Illusion Starring Mickey Mouse (USA, Display Unit Sample)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_castlillsRomInfo, sms_castlillsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Castle of Illusion Starring Mickey Mouse (USA)

static struct BurnRomInfo sms_castlilluRomDesc[] = {
	{ "castle of illusion starring mickey mouse (usa).bin",	0x40000, 0xb9db4282, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_castlillu)
STD_ROM_FN(sms_castlillu)

struct BurnDriver BurnDrvsms_castlillu = {
	"sms_castlillu", "sms_castlill", NULL, NULL, "1990",
	"Castle of Illusion Starring Mickey Mouse (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_castlilluRomInfo, sms_castlilluRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Champions of Europe (Euro, Bra)

static struct BurnRomInfo sms_champeurRomDesc[] = {
	{ "mpr-14689.ic1",	0x40000, 0x23163a12, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_champeur)
STD_ROM_FN(sms_champeur)

struct BurnDriver BurnDrvsms_champeur = {
	"sms_champeur", NULL, NULL, NULL, "1992",
	"Champions of Europe (Euro, Bra)\0", NULL, "TecMagik", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_champeurRomInfo, sms_champeurRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chapolim x Dracula - Um Duelo Assustador (Bra)

static struct BurnRomInfo sms_chapolimRomDesc[] = {
	{ "chapolim x dracula - um duelo assustador (brazil).bin",	0x20000, 0x492c7c6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_chapolim)
STD_ROM_FN(sms_chapolim)

struct BurnDriver BurnDrvsms_chapolim = {
	"sms_chapolim", "sms_ghosth", NULL, NULL, "1990",
	"Chapolim x Dracula - Um Duelo Assustador (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chapolimRomInfo, sms_chapolimRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taito Chase H.Q. (Euro)

static struct BurnRomInfo sms_chasehqRomDesc[] = {
	{ "taito chase h.q. (europe).bin",	0x40000, 0x85cfc9c9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_chasehq)
STD_ROM_FN(sms_chasehq)

struct BurnDriver BurnDrvsms_chasehq = {
	"sms_chasehq", NULL, NULL, NULL, "1990",
	"Taito Chase H.Q. (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chasehqRomInfo, sms_chasehqRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cheese Cat-astrophe Starring Speedy Gonzales (Euro, Bra)

static struct BurnRomInfo sms_cheeseRomDesc[] = {
	{ "cheese cat-astrophe starring speedy gonzales (europe) (en,fr,de,es).bin",	0x80000, 0x46340c41, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cheese)
STD_ROM_FN(sms_cheese)

struct BurnDriver BurnDrvsms_cheese = {
	"sms_cheese", NULL, NULL, NULL, "1995",
	"Cheese Cat-astrophe Starring Speedy Gonzales (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_cheeseRomInfo, sms_cheeseRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Championship Hockey (Euro)

static struct BurnRomInfo sms_champhckRomDesc[] = {
	{ "championship hockey (europe).bin",	0x40000, 0x7e5839a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_champhck)
STD_ROM_FN(sms_champhck)

struct BurnDriver BurnDrvsms_champhck = {
	"sms_champhck", NULL, NULL, NULL, "1994",
	"Championship Hockey (Euro)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_champhckRomInfo, sms_champhckRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Choplifter (Euro, USA, Bra)

static struct BurnRomInfo sms_chopliftRomDesc[] = {
	{ "mpr-10149 w04.ic1",	0x20000, 0x4a21c15f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_choplift)
STD_ROM_FN(sms_choplift)

struct BurnDriver BurnDrvsms_choplift = {
	"sms_choplift", NULL, NULL, NULL, "1986",
	"Choplifter (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chopliftRomInfo, sms_chopliftRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Choplifter (Jpn, Prototype)

static struct BurnRomInfo sms_chopliftjRomDesc[] = {
	{ "choplifter (japan) (proto).bin",	0x20000, 0x16ec3575, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_chopliftj)
STD_ROM_FN(sms_chopliftj)

struct BurnDriver BurnDrvsms_chopliftj = {
	"sms_chopliftj", "sms_choplift", NULL, NULL, "1986",
	"Choplifter (Jpn, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chopliftjRomInfo, sms_chopliftjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Choplifter (USA, Prototype)

static struct BurnRomInfo sms_chopliftpRomDesc[] = {
	{ "choplifter [proto].bin",	0x20000, 0xfd981232, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_chopliftp)
STD_ROM_FN(sms_chopliftp)

struct BurnDriver BurnDrvsms_chopliftp = {
	"sms_chopliftp", "sms_choplift", NULL, NULL, "1986",
	"Choplifter (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chopliftpRomInfo, sms_chopliftpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chuck Rock (Euro, Bra)

static struct BurnRomInfo sms_chuckrckRomDesc[] = {
	{ "chuck rock (europe).bin",	0x80000, 0xdd0e2927, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_chuckrck)
STD_ROM_FN(sms_chuckrck)

struct BurnDriver BurnDrvsms_chuckrck = {
	"sms_chuckrck", NULL, NULL, NULL, "1992",
	"Chuck Rock (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chuckrckRomInfo, sms_chuckrckRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chuck Rock II - Son of Chuck (Euro)

static struct BurnRomInfo sms_chukrck2RomDesc[] = {
	{ "chuck rock ii - son of chuck (europe).bin",	0x80000, 0xc30e690a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_chukrck2)
STD_ROM_FN(sms_chukrck2)

struct BurnDriver BurnDrvsms_chukrck2 = {
	"sms_chukrck2", NULL, NULL, NULL, "1993",
	"Chuck Rock II - Son of Chuck (Euro)\0", NULL, "Core Design", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chukrck2RomInfo, sms_chukrck2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chuck Rock II - Son of Chuck (Bra)

static struct BurnRomInfo sms_chukrck2bRomDesc[] = {
	{ "chuck rock ii - son of chuck (brazil).bin",	0x80000, 0x87783c04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_chukrck2b)
STD_ROM_FN(sms_chukrck2b)

struct BurnDriver BurnDrvsms_chukrck2b = {
	"sms_chukrck2b", "sms_chukrck2", NULL, NULL, "1993",
	"Chuck Rock II - Son of Chuck (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_chukrck2bRomInfo, sms_chukrck2bRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Circuit (Jpn)

static struct BurnRomInfo sms_circuitRomDesc[] = {
	{ "circuit, the (japan).bin",	0x20000, 0x8fb75994, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_circuit)
STD_ROM_FN(sms_circuit)

struct BurnDriver BurnDrvsms_circuit = {
	"sms_circuit", "sms_worldgp", NULL, NULL, "1986",
	"The Circuit (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_circuitRomInfo, sms_circuitRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cloud Master (Euro, USA)

static struct BurnRomInfo sms_cloudmstRomDesc[] = {
	{ "cloud master (usa, europe).bin",	0x40000, 0xe7f62e6d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cloudmst)
STD_ROM_FN(sms_cloudmst)

struct BurnDriver BurnDrvsms_cloudmst = {
	"sms_cloudmst", NULL, NULL, NULL, "1989",
	"Cloud Master (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_cloudmstRomInfo, sms_cloudmstRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Color and Switch Test (v1.3)

static struct BurnRomInfo sms_colorsRomDesc[] = {
	{ "color and switch test (unknown) (v1.3).bin",	0x08000, 0x7253c3ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_colors)
STD_ROM_FN(sms_colors)

struct BurnDriver BurnDrvsms_colors = {
	"sms_colors", NULL, NULL, NULL, "19??",
	"Color and Switch Test (v1.3)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_colorsRomInfo, sms_colorsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Columns (Euro, USA, Bra, Kor)

static struct BurnRomInfo sms_columnsRomDesc[] = {
	{ "mpr-13293.ic1",	0x20000, 0x665fda92, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_columns)
STD_ROM_FN(sms_columns)

struct BurnDriver BurnDrvsms_columns = {
	"sms_columns", NULL, NULL, NULL, "1990",
	"Columns (Euro, USA, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_columnsRomInfo, sms_columnsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Columns (Prototype)

static struct BurnRomInfo sms_columnspRomDesc[] = {
	{ "columns [proto].bin",	0x20000, 0x3fb40043, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_columnsp)
STD_ROM_FN(sms_columnsp)

struct BurnDriver BurnDrvsms_columnsp = {
	"sms_columnsp", "sms_columns", NULL, NULL, "1990",
	"Columns (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_columnspRomInfo, sms_columnspRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Comic Bakery (Kor)

static struct BurnRomInfo sms_comicbakRomDesc[] = {
	{ "comic bakery (kor).bin",	0x08000, 0x7778e256, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_comicbak)
STD_ROM_FN(sms_comicbak)

struct BurnDriver BurnDrvsms_comicbak = {
	"sms_comicbak", NULL, NULL, NULL, "19??",
	"Comic Bakery (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_comicbakRomInfo, sms_comicbakRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Comical Machine Gun Joe (Tw)

static struct BurnRomInfo sms_comicaltwRomDesc[] = {
	{ "comical machine gun joe (tw).bin",	0x08000, 0x84ad5ae4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_comicaltw)
STD_ROM_FN(sms_comicaltw)

struct BurnDriver BurnDrvsms_comicaltw = {
	"sms_comicaltw", "sms_comical", NULL, NULL, "1986?",
	"Comical Machine Gun Joe (Tw)\0", NULL, "Aaronix", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_comicaltwRomInfo, sms_comicaltwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Comical Machine Gun Joe (Kor)

static struct BurnRomInfo sms_comicalkRomDesc[] = {
	{ "comical machine gun joe (kr).bin",	0x08000, 0x643f6bfc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_comicalk)
STD_ROM_FN(sms_comicalk)

struct BurnDriver BurnDrvsms_comicalk = {
	"sms_comicalk", "sms_comical", NULL, NULL, "198?",
	"Comical Machine Gun Joe (Kor)\0", NULL, "Samsung", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_comicalkRomInfo, sms_comicalkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cool Spot (Euro)

static struct BurnRomInfo sms_coolspotRomDesc[] = {
	{ "cool spot (europe).bin",	0x40000, 0x13ac9023, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_coolspot)
STD_ROM_FN(sms_coolspot)

struct BurnDriver BurnDrvsms_coolspot = {
	"sms_coolspot", NULL, NULL, NULL, "1993",
	"Cool Spot (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_coolspotRomInfo, sms_coolspotRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cosmic Spacehead (Euro)

static struct BurnRomInfo sms_cosmicRomDesc[] = {
	{ "cosmic spacehead (europe) (en,fr,de,es).bin",	0x40000, 0x29822980, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cosmic)
STD_ROM_FN(sms_cosmic)

struct BurnDriver BurnDrvsms_cosmic = {
	"sms_cosmic", NULL, NULL, NULL, "1993",
	"Cosmic Spacehead (Euro)\0", NULL, "Codemasters", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_cosmicRomInfo, sms_cosmicRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Cyber Shinobi (Euro, Bra)

static struct BurnRomInfo sms_cybersRomDesc[] = {
	{ "cyber shinobi, the (europe).bin",	0x40000, 0x1350e4f8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cybers)
STD_ROM_FN(sms_cybers)

struct BurnDriver BurnDrvsms_cybers = {
	"sms_cybers", NULL, NULL, NULL, "1990",
	"The Cyber Shinobi (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_cybersRomInfo, sms_cybersRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Cyber Shinobi (Prototype)

static struct BurnRomInfo sms_cyberspRomDesc[] = {
	{ "cyber shinobi, the [proto].bin",	0x20000, 0xdac00a3a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cybersp)
STD_ROM_FN(sms_cybersp)

struct BurnDriver BurnDrvsms_cybersp = {
	"sms_cybersp", "sms_cybers", NULL, NULL, "1990",
	"The Cyber Shinobi (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_cyberspRomInfo, sms_cyberspRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cyborg Hunter (Euro, USA, Bra)

static struct BurnRomInfo sms_cyborghRomDesc[] = {
	{ "cyborg hunter (usa, europe).bin",	0x20000, 0x908e7524, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cyborgh)
STD_ROM_FN(sms_cyborgh)

struct BurnDriver BurnDrvsms_cyborgh = {
	"sms_cyborgh", NULL, NULL, NULL, "1988",
	"Cyborg Hunter (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_cyborghRomInfo, sms_cyborghRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cyborg Z (Kor)

static struct BurnRomInfo sms_cyborgzRomDesc[] = {
	{ "cyborg z (kr).bin",	0x20000, 0x77efe84a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_cyborgz)
STD_ROM_FN(sms_cyborgz)

struct BurnDriver BurnDrvsms_cyborgz = {
	"sms_cyborgz", NULL, NULL, NULL, "1991",
	"Cyborg Z (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_cyborgzRomInfo, sms_cyborgzRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Daffy Duck in Hollywood (Euro, Bra)

static struct BurnRomInfo sms_daffyRomDesc[] = {
	{ "daffy duck in hollywood (europe) (en,fr,de,es,it).bin",	0x80000, 0x71abef27, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_daffy)
STD_ROM_FN(sms_daffy)

struct BurnDriver BurnDrvsms_daffy = {
	"sms_daffy", NULL, NULL, NULL, "1994",
	"Daffy Duck in Hollywood (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_daffyRomInfo, sms_daffyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dallyeora Pigu-Wang (Kor)

static struct BurnRomInfo sms_dallyeRomDesc[] = {
	{ "dallyeora pigu-wang (korea) (unl).bin",	0x6c000, 0x89b79e77, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dallye)
STD_ROM_FN(sms_dallye)

struct BurnDriver BurnDrvsms_dallye = {
	"sms_dallye", NULL, NULL, NULL, "1995",
	"Dallyeora Pigu-Wang (Kor)\0", NULL, "Game Line", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_KOREA, GBF_MISC, 0,
	SMSGetZipName, sms_dallyeRomInfo, sms_dallyeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Danan: The Jungle Fighter (Euro, Bra)

static struct BurnRomInfo sms_dananRomDesc[] = {
	{ "danan - the jungle fighter (europe).bin",	0x40000, 0xae4a28d7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_danan)
STD_ROM_FN(sms_danan)

struct BurnDriver BurnDrvsms_danan = {
	"sms_danan", NULL, NULL, NULL, "1990",
	"Danan: The Jungle Fighter (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dananRomInfo, sms_dananRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Double Dragon (World)

static struct BurnRomInfo sms_ddragonRomDesc[] = {
	{ "mpr-11831 t03.ic1",	0x40000, 0xa55d89f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ddragon)
STD_ROM_FN(sms_ddragon)

struct BurnDriver BurnDrvsms_ddragon = {
	"sms_ddragon", NULL, NULL, NULL, "1988",
	"Double Dragon (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ddragonRomInfo, sms_ddragonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Double Dragon (Kor)

static struct BurnRomInfo sms_ddragonkRomDesc[] = {
	{ "double dragon (kr).bin",	0x40000, 0xaeacfca7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ddragonk)
STD_ROM_FN(sms_ddragonk)

struct BurnDriver BurnDrvsms_ddragonk = {
	"sms_ddragonk", "sms_ddragon", NULL, NULL, "198?",
	"Double Dragon (Kor)\0", NULL, "Samsung", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ddragonkRomInfo, sms_ddragonkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dead Angle (Euro, USA)

static struct BurnRomInfo sms_deadangRomDesc[] = {
	{ "mpr-12654f.ic1",	0x40000, 0xe2f7193e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_deadang)
STD_ROM_FN(sms_deadang)

struct BurnDriver BurnDrvsms_deadang = {
	"sms_deadang", NULL, NULL, NULL, "1989",
	"Dead Angle (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_deadangRomInfo, sms_deadangRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Deep Duck Trouble Starring Donald Duck (Euro, Bra)

static struct BurnRomInfo sms_deepduckRomDesc[] = {
	{ "deep duck trouble starring donald duck (europe).bin",	0x80000, 0x42fc3a6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_deepduck)
STD_ROM_FN(sms_deepduck)

struct BurnDriver BurnDrvsms_deepduck = {
	"sms_deepduck", NULL, NULL, NULL, "1993",
	"Deep Duck Trouble Starring Donald Duck (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_deepduckRomInfo, sms_deepduckRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Desert Speedtrap Starring Road Runner and Wile E. Coyote (Euro, Bra)

static struct BurnRomInfo sms_desertRomDesc[] = {
	{ "desert speedtrap starring road runner and wile e. coyote (europe) (en,fr,de,es,it).bin",	0x40000, 0xb137007a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_desert)
STD_ROM_FN(sms_desert)

struct BurnDriver BurnDrvsms_desert = {
	"sms_desert", NULL, NULL, NULL, "1993",
	"Desert Speedtrap Starring Road Runner and Wile E. Coyote (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_desertRomInfo, sms_desertRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dick Tracy (Euro, USA, Bra)

static struct BurnRomInfo sms_dicktrRomDesc[] = {
	{ "mpr-13644.ic1",	0x40000, 0xf6fab48d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dicktr)
STD_ROM_FN(sms_dicktr)

struct BurnDriver BurnDrvsms_dicktr = {
	"sms_dicktr", NULL, NULL, NULL, "1990",
	"Dick Tracy (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dicktrRomInfo, sms_dicktrRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dinobasher Starring Bignose the Caveman (Euro, Prototype)

static struct BurnRomInfo sms_dinobashRomDesc[] = {
	{ "dinobasher starring bignose the caveman (europe) (proto).bin",	0x40000, 0xea5c3a6f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dinobash)
STD_ROM_FN(sms_dinobash)

struct BurnDriver BurnDrvsms_dinobash = {
	"sms_dinobash", NULL, NULL, NULL, "19??",
	"Dinobasher Starring Bignose the Caveman (Euro, Prototype)\0", NULL, "Codemasters", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_dinobashRomInfo, sms_dinobashRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Dinosaur Dooley (Kor)

static struct BurnRomInfo sms_dinodoolRomDesc[] = {
	{ "dinosaur dooley, the (korea) (unl).bin",	0x20000, 0x32f4b791, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dinodool)
STD_ROM_FN(sms_dinodool)

struct BurnDriver BurnDrvsms_dinodool = {
	"sms_dinodool", NULL, NULL, NULL, "19??",
	"The Dinosaur Dooley (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dinodoolRomInfo, sms_dinodoolRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Doki Doki Penguin Land - Uchuu Daibouken (Jpn)

static struct BurnRomInfo sms_dokidokiRomDesc[] = {
	{ "doki doki penguin land - uchuu daibouken (japan).bin",	0x20000, 0x2bcdb8fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dokidoki)
STD_ROM_FN(sms_dokidoki)

struct BurnDriver BurnDrvsms_dokidoki = {
	"sms_dokidoki", "sms_pengland", NULL, NULL, "1987",
	"Doki Doki Penguin Land - Uchuu Daibouken (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dokidokiRomInfo, sms_dokidokiRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Doki Doki Penguin Land - Uchuu Daibouken (Jpn, Prototype)

static struct BurnRomInfo sms_dokidokipRomDesc[] = {
	{ "doki doki penguin land - uchuu daibouken (japan, proto).bin",	0x20000, 0x56bd2455, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dokidokip)
STD_ROM_FN(sms_dokidokip)

struct BurnDriver BurnDrvsms_dokidokip = {
	"sms_dokidokip", "sms_pengland", NULL, NULL, "1987",
	"Doki Doki Penguin Land - Uchuu Daibouken (Jpn, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dokidokipRomInfo, sms_dokidokipRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Double Hawk (Euro)

static struct BurnRomInfo sms_doublhwkRomDesc[] = {
	{ "double hawk (europe).bin",	0x40000, 0x8370f6cd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_doublhwk)
STD_ROM_FN(sms_doublhwk)

struct BurnDriver BurnDrvsms_doublhwk = {
	"sms_doublhwk", NULL, NULL, NULL, "1990",
	"Double Hawk (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_doublhwkRomInfo, sms_doublhwkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Double Hawk (Euro, Prototype)

static struct BurnRomInfo sms_doublhwkpRomDesc[] = {
	{ "double hawk (europe) (beta).bin",	0x40000, 0xf76d5cee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_doublhwkp)
STD_ROM_FN(sms_doublhwkp)

struct BurnDriver BurnDrvsms_doublhwkp = {
	"sms_doublhwkp", "sms_doublhwk", NULL, NULL, "1990",
	"Double Hawk (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_doublhwkpRomInfo, sms_doublhwkpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Double Target - Cynthia no Nemuri (Jpn)

static struct BurnRomInfo sms_doubltgtRomDesc[] = {
	{ "double target - cynthia no nemuri (japan).bin",	0x20000, 0x52b83072, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_doubltgt)
STD_ROM_FN(sms_doubltgt)

struct BurnDriver BurnDrvsms_doubltgt = {
	"sms_doubltgt", "sms_quartet", NULL, NULL, "1987",
	"Double Target - Cynthia no Nemuri (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_doubltgtRomInfo, sms_doubltgtRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dragon - The Bruce Lee Story (Euro)

static struct BurnRomInfo sms_dragonRomDesc[] = {
	{ "dragon - the bruce lee story (europe).bin",	0x40000, 0xc88a5064, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dragon)
STD_ROM_FN(sms_dragon)

struct BurnDriver BurnDrvsms_dragon = {
	"sms_dragon", NULL, NULL, NULL, "1994",
	"Dragon - The Bruce Lee Story (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dragonRomInfo, sms_dragonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dragon Crystal (Euro, Bra)

static struct BurnRomInfo sms_dcrystalRomDesc[] = {
	{ "dragon crystal (europe).bin",	0x20000, 0x9549fce4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dcrystal)
STD_ROM_FN(sms_dcrystal)

struct BurnDriver BurnDrvsms_dcrystal = {
	"sms_dcrystal", NULL, NULL, NULL, "1991",
	"Dragon Crystal (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dcrystalRomInfo, sms_dcrystalRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dr. HELLO (Kor)

static struct BurnRomInfo sms_drhelloRomDesc[] = {
	{ "dr. hello (k).bin",	0x08000, 0x16537865, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_drhello)
STD_ROM_FN(sms_drhello)

struct BurnDriver BurnDrvsms_drhello = {
	"sms_drhello", NULL, NULL, NULL, "19??",
	"Dr. HELLO (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_drhelloRomInfo, sms_drhelloRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dr. Robotnik's Mean Bean Machine (Euro, Bra)

static struct BurnRomInfo sms_drrobotnRomDesc[] = {
	{ "dr. robotnik's mean bean machine (europe).bin",	0x40000, 0x6c696221, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_drrobotn)
STD_ROM_FN(sms_drrobotn)

struct BurnDriver BurnDrvsms_drrobotn = {
	"sms_drrobotn", NULL, NULL, NULL, "1993",
	"Dr. Robotnik's Mean Bean Machine (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_drrobotnRomInfo, sms_drrobotnRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Desert Strike (Euro)

static struct BurnRomInfo sms_dstrikeRomDesc[] = {
	{ "desert strike (europe) (en,fr,de,es).bin",	0x80000, 0x6c1433f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dstrike)
STD_ROM_FN(sms_dstrike)

struct BurnDriver BurnDrvsms_dstrike = {
	"sms_dstrike", NULL, NULL, NULL, "1992",
	"Desert Strike (Euro)\0", NULL, "Domark", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dstrikeRomInfo, sms_dstrikeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gokuaku Doumei Dump Matsumoto (Jpn)

static struct BurnRomInfo sms_dumpmatsRomDesc[] = {
	{ "gokuaku doumei dump matsumoto (japan).bin",	0x20000, 0xa249fa9d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dumpmats)
STD_ROM_FN(sms_dumpmats)

struct BurnDriver BurnDrvsms_dumpmats = {
	"sms_dumpmats", "sms_prowres", NULL, NULL, "1986",
	"Gokuaku Doumei Dump Matsumoto (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dumpmatsRomInfo, sms_dumpmatsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Duke (Euro, Bra)

static struct BurnRomInfo sms_dyndukeRomDesc[] = {
	{ "dynamite duke (europe).bin",	0x40000, 0x07306947, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dynduke)
STD_ROM_FN(sms_dynduke)

struct BurnDriver BurnDrvsms_dynduke = {
	"sms_dynduke", NULL, NULL, NULL, "1991",
	"Dynamite Duke (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dyndukeRomInfo, sms_dyndukeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Dux (Euro, Bra)

static struct BurnRomInfo sms_dduxRomDesc[] = {
	{ "dynamite dux (europe).bin",	0x40000, 0x0e1cc1e0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ddux)
STD_ROM_FN(sms_ddux)

struct BurnDriver BurnDrvsms_ddux = {
	"sms_ddux", NULL, NULL, NULL, "1989",
	"Dynamite Dux (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dduxRomInfo, sms_dduxRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Headdy (Bra)

static struct BurnRomInfo sms_dheadRomDesc[] = {
	{ "dynamite headdy (brazil).bin",	0x80000, 0x7db5b0fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_dhead)
STD_ROM_FN(sms_dhead)

struct BurnDriver BurnDrvsms_dhead = {
	"sms_dhead", NULL, NULL, NULL, "1994",
	"Dynamite Headdy (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_dheadRomInfo, sms_dheadRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// E.I. - Exa Innova (Kor)

static struct BurnRomInfo sms_exainnovRomDesc[] = {
	{ "e.i. - exa innova (kr).sms",	0x08000, 0xdd74bcf1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_exainnov)
STD_ROM_FN(sms_exainnov)

struct BurnDriver BurnDrvsms_exainnov = {
	"sms_exainnov", NULL, NULL, NULL, "19??",
	"E.I. - Exa Innova (Kor)\0", NULL, "HiCom", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_exainnovRomInfo, sms_exainnovRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Eagles 5 (Kor)

static struct BurnRomInfo sms_eagles5RomDesc[] = {
	{ "eagles 5 (kr).bin",	0x08000, 0xf06f2ccb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_eagles5)
STD_ROM_FN(sms_eagles5)

struct BurnDriver BurnDrvsms_eagles5 = {
	"sms_eagles5", NULL, NULL, NULL, "1990",
	"Eagles 5 (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_eagles5RomInfo, sms_eagles5RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ecco the Dolphin (Euro, Bra)

static struct BurnRomInfo sms_eccoRomDesc[] = {
	{ "ecco the dolphin (europe).bin",	0x80000, 0x6687fab9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ecco)
STD_ROM_FN(sms_ecco)

struct BurnDriver BurnDrvsms_ecco = {
	"sms_ecco", NULL, NULL, NULL, "1993",
	"Ecco the Dolphin (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_eccoRomInfo, sms_eccoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ecco - The Tides of Time (Bra)

static struct BurnRomInfo sms_ecco2RomDesc[] = {
	{ "ecco - the tides of time (brazil).bin",	0x80000, 0x7c28703a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ecco2)
STD_ROM_FN(sms_ecco2)

struct BurnDriver BurnDrvsms_ecco2 = {
	"sms_ecco2", NULL, NULL, NULL, "1996",
	"Ecco - The Tides of Time (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ecco2RomInfo, sms_ecco2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Earthworm Jim (Bra)

static struct BurnRomInfo sms_ejimRomDesc[] = {
	{ "earthworm jim (brazil).bin",	0x80000, 0xc4d5efc5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ejim)
STD_ROM_FN(sms_ejim)

struct BurnDriver BurnDrvsms_ejim = {
	"sms_ejim", NULL, NULL, NULL, "1996",
	"Earthworm Jim (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_ejimRomInfo, sms_ejimRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Enduro Racer (Euro, USA, Bra)

static struct BurnRomInfo sms_enduroRomDesc[] = {
	{ "mpr-10843 w28.ic1",	0x20000, 0x00e73541, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_enduro)
STD_ROM_FN(sms_enduro)

struct BurnDriver BurnDrvsms_enduro = {
	"sms_enduro", NULL, NULL, NULL, "1987",
	"Enduro Racer (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_enduroRomInfo, sms_enduroRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Enduro Racer (Jpn)

static struct BurnRomInfo sms_endurojRomDesc[] = {
	{ "mpr-10745.ic1",	0x40000, 0x5d5c50b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_enduroj)
STD_ROM_FN(sms_enduroj)

struct BurnDriver BurnDrvsms_enduroj = {
	"sms_enduroj", "sms_enduro", NULL, NULL, "1987",
	"Enduro Racer (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_endurojRomInfo, sms_endurojRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// E-SWAT - City Under Siege (Euro, USA, Easy Version)

static struct BurnRomInfo sms_eswatcRomDesc[] = {
	{ "e-swat - city under siege (usa, europe) (easy version).bin",	0x40000, 0xc4bb1676, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_eswatc)
STD_ROM_FN(sms_eswatc)

struct BurnDriver BurnDrvsms_eswatc = {
	"sms_eswatc", NULL, NULL, NULL, "1990",
	"E-SWAT - City Under Siege (Euro, USA, Easy Version)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_eswatcRomInfo, sms_eswatcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// E-SWAT - City Under Siege (Euro, USA, Bra, Hard Version)

static struct BurnRomInfo sms_eswatc1RomDesc[] = {
	{ "e-swat - city under siege (usa, europe) (hard version).bin",	0x40000, 0xc10fce39, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_eswatc1)
STD_ROM_FN(sms_eswatc1)

struct BurnDriver BurnDrvsms_eswatc1 = {
	"sms_eswatc1", "sms_eswatc", NULL, NULL, "1990",
	"E-SWAT - City Under Siege (Euro, USA, Bra, Hard Version)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_eswatc1RomInfo, sms_eswatc1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Excellent Dizzy Collection (Euro, USA, Prototype)

static struct BurnRomInfo sms_excdizzyRomDesc[] = {
	{ "excellent dizzy collection, the (usa, europe) (en,fr,de,es,it) (proto).bin",	0x40000, 0x8813514b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_excdizzy)
STD_ROM_FN(sms_excdizzy)

struct BurnDriver BurnDrvsms_excdizzy = {
	"sms_excdizzy", NULL, NULL, NULL, "19??",
	"The Excellent Dizzy Collection (Euro, USA, Prototype)\0", NULL, "Codemasters", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_excdizzyRomInfo, sms_excdizzyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 224, 4, 3
};


// F1 (Euro, Bra)

static struct BurnRomInfo sms_f1RomDesc[] = {
	{ "mpr-15830-f.ic1",	0x40000, 0xec788661, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f1)
STD_ROM_FN(sms_f1)

struct BurnDriver BurnDrvsms_f1 = {
	"sms_f1", NULL, NULL, NULL, "1993",
	"F1 (Euro, Bra)\0", NULL, "Domark", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_f1RomInfo, sms_f1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F-1 Spirit - The way to Formula-1 (Kor)

static struct BurnRomInfo sms_f1spiritRomDesc[] = {
	{ "f-1 spirit - the way to formula-1 (kr)",	0x20000, 0x06965ed9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f1spirit)
STD_ROM_FN(sms_f1spirit)

struct BurnDriver BurnDrvsms_f1spirit = {
	"sms_f1spirit", NULL, NULL, NULL, "1987",
	"F-1 Spirit - The way to Formula-1 (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_f1spiritRomInfo, sms_f1spiritRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F-16 Fighting Falcon (USA)

static struct BurnRomInfo sms_f16falcRomDesc[] = {
	{ "f-16 fighting falcon (usa).bin",	0x08000, 0x184c23b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f16falc)
STD_ROM_FN(sms_f16falc)

struct BurnDriver BurnDrvsms_f16falc = {
	"sms_f16falc", "sms_f16fight", NULL, NULL, "1985",
	"F-16 Fighting Falcon (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_f16falcRomInfo, sms_f16falcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F-16 Fighting Falcon (Tw)

static struct BurnRomInfo sms_f16falctwRomDesc[] = {
	{ "f-16 fighting falcon (tw).bin",	0x08000, 0xc4c53226, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f16falctw)
STD_ROM_FN(sms_f16falctw)

struct BurnDriver BurnDrvsms_f16falctw = {
	"sms_f16falctw", "sms_f16fight", NULL, NULL, "1985?",
	"F-16 Fighting Falcon (Tw)\0", NULL, "Aaronix", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_f16falctwRomInfo, sms_f16falctwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F-16 Fighter (Euro, USA)

static struct BurnRomInfo sms_f16fightRomDesc[] = {
	{ "mpr-12643.ic1",	0x08000, 0xeaebf323, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f16fight)
STD_ROM_FN(sms_f16fight)

struct BurnDriver BurnDrvsms_f16fight = {
	"sms_f16fight", NULL, NULL, NULL, "1986",
	"F-16 Fighter (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_f16fightRomInfo, sms_f16fightRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// FA Tetris (Kor)

static struct BurnRomInfo sms_fatetrisRomDesc[] = {
	{ "fa tetris (kr).bin",	0x08000, 0x17ab6883, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fatetris)
STD_ROM_FN(sms_fatetris)

struct BurnDriver BurnDrvsms_fatetris = {
	"sms_fatetris", NULL, NULL, NULL, "1989",
	"FA Tetris (Kor)\0", NULL, "FA Soft", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_fatetrisRomInfo, sms_fatetrisRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Family Games (Jpn)

static struct BurnRomInfo sms_familyRomDesc[] = {
	{ "mpr-11276.ic1",	0x20000, 0x7abc70e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_family)
STD_ROM_FN(sms_family)

struct BurnDriver BurnDrvsms_family = {
	"sms_family", "sms_parlour", NULL, NULL, "1987",
	"Family Games (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_familyRomInfo, sms_familyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantastic Dizzy (Euro)

static struct BurnRomInfo sms_fantdizzRomDesc[] = {
	{ "fantastic dizzy (europe) (en,fr,de,es,it).bin",	0x40000, 0xb9664ae1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantdizz)
STD_ROM_FN(sms_fantdizz)

struct BurnDriver BurnDrvsms_fantdizz = {
	"sms_fantdizz", NULL, NULL, NULL, "1993",
	"Fantastic Dizzy (Euro)\0", NULL, "Codemasters", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_fantdizzRomInfo, sms_fantdizzRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone II - The Tears of Opa-Opa (Euro, USA, Bra)

static struct BurnRomInfo sms_fantzon2RomDesc[] = {
	{ "mpr-10848 w05.ic1",	0x40000, 0xb8b141f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantzon2)
STD_ROM_FN(sms_fantzon2)

struct BurnDriver BurnDrvsms_fantzon2 = {
	"sms_fantzon2", NULL, NULL, NULL, "1987",
	"Fantasy Zone II - The Tears of Opa-Opa (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fantzon2RomInfo, sms_fantzon2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone II - Opa Opa no Namida (Jpn)

static struct BurnRomInfo sms_fantzon2jRomDesc[] = {
	{ "mpr-10847.ic1",	0x40000, 0xc722fb42, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantzon2j)
STD_ROM_FN(sms_fantzon2j)

struct BurnDriver BurnDrvsms_fantzon2j = {
	"sms_fantzon2j", "sms_fantzon2", NULL, NULL, "1987",
	"Fantasy Zone II - Opa Opa no Namida (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fantzon2jRomInfo, sms_fantzon2jRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone (World, v2)

static struct BurnRomInfo sms_fantzoneRomDesc[] = {
	{ "mpr-10118 w01.ic1",	0x20000, 0x65d7e4e0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantzone)
STD_ROM_FN(sms_fantzone)

struct BurnDriver BurnDrvsms_fantzone = {
	"sms_fantzone", NULL, NULL, NULL, "1986",
	"Fantasy Zone (World, v2)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fantzoneRomInfo, sms_fantzoneRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone (World, v1, Prototype)

static struct BurnRomInfo sms_fantzone1RomDesc[] = {
	{ "fantasy zone (world) (v1.1) (beta).bin",	0x20000, 0xf46264fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantzone1)
STD_ROM_FN(sms_fantzone1)

struct BurnDriver BurnDrvsms_fantzone1 = {
	"sms_fantzone1", "sms_fantzone", NULL, NULL, "1986",
	"Fantasy Zone (World, v1, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fantzone1RomInfo, sms_fantzone1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone (Jpn, v0)

static struct BurnRomInfo sms_fantzonejRomDesc[] = {
	{ "mpr-7751.ic1",	0x20000, 0x0ffbcaa3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantzonej)
STD_ROM_FN(sms_fantzonej)

struct BurnDriver BurnDrvsms_fantzonej = {
	"sms_fantzonej", "sms_fantzone", NULL, NULL, "1986",
	"Fantasy Zone (Jpn, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fantzonejRomInfo, sms_fantzonejRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone (Tw)

static struct BurnRomInfo sms_fantzonetwRomDesc[] = {
	{ "fantasy zone (tw).bin",	0x20000, 0x5fd48352, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantzonetw)
STD_ROM_FN(sms_fantzonetw)

struct BurnDriver BurnDrvsms_fantzonetw = {
	"sms_fantzonetw", "sms_fantzone", NULL, NULL, "1986?",
	"Fantasy Zone (Tw)\0", NULL, "Aaronix", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fantzonetwRomInfo, sms_fantzonetwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone - The Maze (Euro, USA)

static struct BurnRomInfo sms_fantzonmRomDesc[] = {
	{ "fantasy zone - the maze (usa, europe).bin",	0x20000, 0xd29889ad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fantzonm)
STD_ROM_FN(sms_fantzonm)

struct BurnDriver BurnDrvsms_fantzonm = {
	"sms_fantzonm", NULL, NULL, NULL, "1987",
	"Fantasy Zone - The Maze (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fantzonmRomInfo, sms_fantzonmRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Final Bubble Bobble (Jpn)

static struct BurnRomInfo sms_finalbbRomDesc[] = {
	{ "final bubble bobble (japan).bin",	0x40000, 0x3ebb7457, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_finalbb)
STD_ROM_FN(sms_finalbb)

struct BurnDriver BurnDrvsms_finalbb = {
	"sms_finalbb", "sms_bublbobl", NULL, NULL, "1988",
	"Final Bubble Bobble (Jpn)\0", NULL, "Sega ", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_finalbbRomInfo, sms_finalbbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Felipe em Acao (Bra)

static struct BurnRomInfo sms_felipeRomDesc[] = {
	{ "felipe em acao (b).bin",	0x08000, 0xccb2cab4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_felipe)
STD_ROM_FN(sms_felipe)

struct BurnDriver BurnDrvsms_felipe = {
	"sms_felipe", "sms_teddyboy", NULL, NULL, "19??",
	"Felipe em Acao (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_felipeRomInfo, sms_felipeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Férias Frustradas do Pica-Pau (Bra)

static struct BurnRomInfo sms_feriasRomDesc[] = {
	{ "ferias frustradas do pica pau (brazil).bin",	0x80000, 0xbf6c2e37, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ferias)
STD_ROM_FN(sms_ferias)

struct BurnDriver BurnDrvsms_ferias = {
	"sms_ferias", NULL, NULL, NULL, "1996",
	"Ferias Frustradas do Pica-Pau (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_feriasRomInfo, sms_feriasRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// FIFA International Soccer (Bra)

static struct BurnRomInfo sms_fifaRomDesc[] = {
	{ "fifa international soccer (brazil) (en,es,pt).bin",	0x80000, 0x9bb3b5f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fifa)
STD_ROM_FN(sms_fifa)

struct BurnDriver BurnDrvsms_fifa = {
	"sms_fifa", NULL, NULL, NULL, "1994",
	"FIFA International Soccer (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fifaRomInfo, sms_fifaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fire and Forget II (Euro)

static struct BurnRomInfo sms_fireforgRomDesc[] = {
	{ "fire and forget ii (europe).bin",	0x40000, 0xf6ad7b1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fireforg)
STD_ROM_FN(sms_fireforg)

struct BurnDriver BurnDrvsms_fireforg = {
	"sms_fireforg", NULL, NULL, NULL, "1990",
	"Fire and Forget II (Euro)\0", NULL, "Titus Arcade", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fireforgRomInfo, sms_fireforgRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fire and Ice (Bra)

static struct BurnRomInfo sms_fireiceRomDesc[] = {
	{ "fire and ice (brazil).bin",	0x40000, 0x8b24a640, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fireice)
STD_ROM_FN(sms_fireice)

struct BurnDriver BurnDrvsms_fireice = {
	"sms_fireice", NULL, NULL, NULL, "1993",
	"Fire and Ice (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_fireiceRomInfo, sms_fireiceRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Flash (Euro, Bra)

static struct BurnRomInfo sms_flashRomDesc[] = {
	{ "flash, the (europe).bin",	0x40000, 0xbe31d63f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_flash)
STD_ROM_FN(sms_flash)

struct BurnDriver BurnDrvsms_flash = {
	"sms_flash", NULL, NULL, NULL, "1993",
	"The Flash (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_flashRomInfo, sms_flashRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Flashpoint (Kor)

static struct BurnRomInfo sms_fpointRomDesc[] = {
	{ "flashpoint (kr).bin",	0x08000, 0x61e8806f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_fpoint)
STD_ROM_FN(sms_fpoint)

struct BurnDriver BurnDrvsms_fpoint = {
	"sms_fpoint", NULL, NULL, NULL, "199?",
	"Flashpoint (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_fpointRomInfo, sms_fpointRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Flintstones (Euro, Bra)

static struct BurnRomInfo sms_flintRomDesc[] = {
	{ "mpr-13701-f.ic1",	0x40000, 0xca5c78a5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_flint)
STD_ROM_FN(sms_flint)

struct BurnDriver BurnDrvsms_flint = {
	"sms_flint", NULL, NULL, NULL, "1991",
	"The Flintstones (Euro, Bra)\0", NULL, "Grandslam", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_flintRomInfo, sms_flintRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// George Foreman's KO Boxing (Euro, Bra)

static struct BurnRomInfo sms_georgekoRomDesc[] = {
	{ "george foreman's ko boxing (europe).bin",	0x40000, 0xa64898ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_georgeko)
STD_ROM_FN(sms_georgeko)

struct BurnDriver BurnDrvsms_georgeko = {
	"sms_georgeko", NULL, NULL, NULL, "1992",
	"George Foreman's KO Boxing (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_georgekoRomInfo, sms_georgekoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Forgotten Worlds (Euro, Bra, Aus)

static struct BurnRomInfo sms_forgottnRomDesc[] = {
	{ "mpr-13706-f.ic1",	0x40000, 0x38c53916, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_forgottn)
STD_ROM_FN(sms_forgottn)

struct BurnDriver BurnDrvsms_forgottn = {
	"sms_forgottn", NULL, NULL, NULL, "1991",
	"Forgotten Worlds (Euro, Bra, Aus)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_forgottnRomInfo, sms_forgottnRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fushigi no Oshiro Pit Pot (Jpn, Pirate?)

static struct BurnRomInfo sms_pitpot1RomDesc[] = {
	{ "fushigi no oshiro pit pot [hack].bin",	0x08000, 0x5d08e823, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pitpot1)
STD_ROM_FN(sms_pitpot1)

struct BurnDriver BurnDrvsms_pitpot1 = {
	"sms_pitpot1", "sms_pitpot", NULL, NULL, "1985",
	"Fushigi no Oshiro Pit Pot (Jpn, Pirate?)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pitpot1RomInfo, sms_pitpot1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gaegujangi Ggachi (Kor)

static struct BurnRomInfo sms_gaegujanRomDesc[] = {
	{ "gaegujangi ggachi (korea) (unl).bin",	0x80000, 0x8b40f6bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gaegujan)
STD_ROM_FN(sms_gaegujan)

struct BurnDriver BurnDrvsms_gaegujan = {
	"sms_gaegujan", NULL, NULL, NULL, "19??",
	"Gaegujangi Ggachi (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gaegujanRomInfo, sms_gaegujanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gain Ground (Euro, Bra)

static struct BurnRomInfo sms_ggroundRomDesc[] = {
	{ "gain ground (europe).bin",	0x40000, 0x3ec5e627, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gground)
STD_ROM_FN(sms_gground)

struct BurnDriver BurnDrvsms_gground = {
	"sms_gground", NULL, NULL, NULL, "1990",
	"Gain Ground (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ggroundRomInfo, sms_ggroundRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gain Ground (Euro, Prototype)

static struct BurnRomInfo sms_ggroundpRomDesc[] = {
	{ "gain ground [proto].bin",	0x40000, 0xd40d03c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ggroundp)
STD_ROM_FN(sms_ggroundp)

struct BurnDriver BurnDrvsms_ggroundp = {
	"sms_ggroundp", "sms_gground", NULL, NULL, "1990",
	"Gain Ground (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ggroundpRomInfo, sms_ggroundpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Galactic Protector (Jpn)

static struct BurnRomInfo sms_galactprRomDesc[] = {
	{ "mpr-11385.ic1",	0x20000, 0xa6fa42d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_galactpr)
STD_ROM_FN(sms_galactpr)

struct BurnDriver BurnDrvsms_galactpr = {
	"sms_galactpr", NULL, NULL, NULL, "1988",
	"Galactic Protector (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_galactprRomInfo, sms_galactprRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Galaxian (Kor)

static struct BurnRomInfo sms_galaxianRomDesc[] = {
	{ "galaxian [kr].bin",	0x08000, 0x577ec227, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_galaxian)
STD_ROM_FN(sms_galaxian)

struct BurnDriver BurnDrvsms_galaxian = {
	"sms_galaxian", NULL, NULL, NULL, "199?",
	"Galaxian (Kor)\0", NULL, "HiCom", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_galaxianRomInfo, sms_galaxianRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Galaxy Force (Euro, Bra)

static struct BurnRomInfo sms_gforceRomDesc[] = {
	{ "mpr-12566a.ic2",	0x80000, 0xa4ac35d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gforce)
STD_ROM_FN(sms_gforce)

struct BurnDriver BurnDrvsms_gforce = {
	"sms_gforce", NULL, NULL, NULL, "1989",
	"Galaxy Force (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gforceRomInfo, sms_gforceRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Galaxy Force (USA)

static struct BurnRomInfo sms_gforceuRomDesc[] = {
	{ "galaxy force (usa).bin",	0x80000, 0x6c827520, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gforceu)
STD_ROM_FN(sms_gforceu)

struct BurnDriver BurnDrvsms_gforceu = {
	"sms_gforceu", "sms_gforce", NULL, NULL, "1989",
	"Galaxy Force (USA)\0", NULL, "Activision", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gforceuRomInfo, sms_gforceuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Game Box Serie Esportes Radicais (Bra)

static struct BurnRomInfo sms_gameboxRomDesc[] = {
	{ "mpr-18796.ic1",	0x40000, 0x1890f407, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gamebox)
STD_ROM_FN(sms_gamebox)

struct BurnDriver BurnDrvsms_gamebox = {
	"sms_gamebox", NULL, NULL, NULL, "19??",
	"Game Box Serie Esportes Radicais (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gameboxRomInfo, sms_gameboxRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gangcheol RoboCop (Kor)

static struct BurnRomInfo sms_robocopRomDesc[] = {
	{ "gangcheol robocop (kr).bin",	0x10000, 0xad522efd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_robocop)
STD_ROM_FN(sms_robocop)

struct BurnDriver BurnDrvsms_robocop = {
	"sms_robocop", NULL, NULL, NULL, "1992",
	"Gangcheol RoboCop (Kor)\0", NULL, "Sieco", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_robocopRomInfo, sms_robocopRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gangster Town (Euro, USA, Bra)

static struct BurnRomInfo sms_gangsterRomDesc[] = {
	{ "mpr-11049.ic1",	0x20000, 0x5fc74d2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gangster)
STD_ROM_FN(sms_gangster)

struct BurnDriver BurnDrvsms_gangster = {
	"sms_gangster", NULL, NULL, NULL, "1987",
	"Gangster Town (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gangsterRomInfo, sms_gangsterRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gauntlet (Euro, Bra)

static struct BurnRomInfo sms_gauntletRomDesc[] = {
	{ "gauntlet (europe).bin",	0x20000, 0xd9190956, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gauntlet)
STD_ROM_FN(sms_gauntlet)

struct BurnDriver BurnDrvsms_gauntlet = {
	"sms_gauntlet", NULL, NULL, NULL, "1990",
	"Gauntlet (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gauntletRomInfo, sms_gauntletRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Golden Axe (Euro, USA)

static struct BurnRomInfo sms_goldnaxeRomDesc[] = {
	{ "mpr-13166.ic1",	0x80000, 0xc08132fb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_goldnaxe)
STD_ROM_FN(sms_goldnaxe)

struct BurnDriver BurnDrvsms_goldnaxe = {
	"sms_goldnaxe", NULL, NULL, NULL, "1989",
	"Golden Axe (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_goldnaxeRomInfo, sms_goldnaxeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Golden Axe Warrior (Euro, USA, Bra)

static struct BurnRomInfo sms_gaxewarrRomDesc[] = {
	{ "mpr-13664-f.ic1",	0x40000, 0xc7ded988, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gaxewarr)
STD_ROM_FN(sms_gaxewarr)

struct BurnDriver BurnDrvsms_gaxewarr = {
	"sms_gaxewarr", NULL, NULL, NULL, "1990",
	"Golden Axe Warrior (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gaxewarrRomInfo, sms_gaxewarrRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Geraldinho (Bra)

static struct BurnRomInfo sms_geraldRomDesc[] = {
	{ "geraldinho (brazil).bin",	0x08000, 0x956c416b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gerald)
STD_ROM_FN(sms_gerald)

struct BurnDriver BurnDrvsms_gerald = {
	"sms_gerald", "sms_teddyboy", NULL, NULL, "19??",
	"Geraldinho (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_geraldRomInfo, sms_geraldRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghostbusters (Euro, USA, Kor)

static struct BurnRomInfo sms_ghostbstRomDesc[] = {
	{ "mpr-10516.ic1",	0x20000, 0x1ddc3059, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghostbst)
STD_ROM_FN(sms_ghostbst)

struct BurnDriver BurnDrvsms_ghostbst = {
	"sms_ghostbst", NULL, NULL, NULL, "1989",
	"Ghostbusters (Euro, USA, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghostbstRomInfo, sms_ghostbstRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghost House (Euro, USA, Bra)

static struct BurnRomInfo sms_ghosthRomDesc[] = {
	{ "mpr-12586.ic1",	0x08000, 0xf1f8ff2d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghosth)
STD_ROM_FN(sms_ghosth)

struct BurnDriver BurnDrvsms_ghosth = {
	"sms_ghosth", NULL, NULL, NULL, "1986",
	"Ghost House (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghosthRomInfo, sms_ghosthRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghost House (Jpn, Pirate?)

static struct BurnRomInfo sms_ghosthj1RomDesc[] = {
	{ "ghost house (j) [hack].bin",	0x08000, 0xdabcc054, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghosthj1)
STD_ROM_FN(sms_ghosthj1)

struct BurnDriver BurnDrvsms_ghosthj1 = {
	"sms_ghosthj1", "sms_ghosth", NULL, NULL, "1986",
	"Ghost House (Jpn, Pirate?)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghosthj1RomInfo, sms_ghosthj1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghost House (Kor)

static struct BurnRomInfo sms_ghosthkRomDesc[] = {
	{ "ghost house (kr).bin",	0x08000, 0x1203afc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghosthk)
STD_ROM_FN(sms_ghosthk)

struct BurnDriver BurnDrvsms_ghosthk = {
	"sms_ghosthk", "sms_ghosth", NULL, NULL, "198?",
	"Ghost House (Kor)\0", NULL, "Samsung", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghosthkRomInfo, sms_ghosthkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghouls'n Ghosts (Euro, USA, Bra)

static struct BurnRomInfo sms_ghoulsRomDesc[] = {
	{ "ghouls'n ghosts (usa, europe).bin",	0x40000, 0x7a92eba6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghouls)
STD_ROM_FN(sms_ghouls)

struct BurnDriver BurnDrvsms_ghouls = {
	"sms_ghouls", NULL, NULL, NULL, "1988",
	"Ghouls'n Ghosts (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghoulsRomInfo, sms_ghoulsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghouls'n Ghosts (USA, Demo)

static struct BurnRomInfo sms_ghoulsdRomDesc[] = {
	{ "ghouls'n ghosts [demo].bin",	0x08000, 0x96ca6902, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghoulsd)
STD_ROM_FN(sms_ghoulsd)

struct BurnDriver BurnDrvsms_ghoulsd = {
	"sms_ghoulsd", "sms_ghouls", NULL, NULL, "1988",
	"Ghouls'n Ghosts (USA, Demo)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghoulsdRomInfo, sms_ghoulsdRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Global Defense (Euro, USA)

static struct BurnRomInfo sms_globaldRomDesc[] = {
	{ "mpr-11274.ic1",	0x20000, 0xb746a6f5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_globald)
STD_ROM_FN(sms_globald)

struct BurnDriver BurnDrvsms_globald = {
	"sms_globald", NULL, NULL, NULL, "1987",
	"Global Defense (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_globaldRomInfo, sms_globaldRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Global Defense (Euro, USA, Prototype)

static struct BurnRomInfo sms_globaldpRomDesc[] = {
	{ "global defense (usa, europe) (beta).bin",	0x20000, 0x91a0fc4e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_globaldp)
STD_ROM_FN(sms_globaldp)

struct BurnDriver BurnDrvsms_globaldp = {
	"sms_globaldp", "sms_globald", NULL, NULL, "1987",
	"Global Defense (Euro, USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_globaldpRomInfo, sms_globaldpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// G-LOC Air Battle (Euro, Bra, Kor)

static struct BurnRomInfo sms_glocRomDesc[] = {
	{ "g-loc air battle (europe).bin",	0x40000, 0x05cdc24e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gloc)
STD_ROM_FN(sms_gloc)

struct BurnDriver BurnDrvsms_gloc = {
	"sms_gloc", NULL, NULL, NULL, "1991",
	"G-LOC Air Battle (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_glocRomInfo, sms_glocRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Golfamania (Euro, Bra)

static struct BurnRomInfo sms_golfamanRomDesc[] = {
	{ "mpr-13144.ic1",	0x40000, 0x48651325, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_golfaman)
STD_ROM_FN(sms_golfaman)

struct BurnDriver BurnDrvsms_golfaman = {
	"sms_golfaman", NULL, NULL, NULL, "1990",
	"Golfamania (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_golfamanRomInfo, sms_golfamanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Golfamania (Prototype)

static struct BurnRomInfo sms_golfamanpRomDesc[] = {
	{ "golfamania (europe) (beta).bin",	0x40000, 0x5dabfdc3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_golfamanp)
STD_ROM_FN(sms_golfamanp)

struct BurnDriver BurnDrvsms_golfamanp = {
	"sms_golfamanp", "sms_golfaman", NULL, NULL, "1990",
	"Golfamania (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_golfamanpRomInfo, sms_golfamanpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Golvellius (Euro, USA)

static struct BurnRomInfo sms_golvellRomDesc[] = {
	{ "mpr-11935f.ic1",	0x40000, 0xa51376fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_golvell)
STD_ROM_FN(sms_golvell)

struct BurnDriver BurnDrvsms_golvell = {
	"sms_golvell", NULL, NULL, NULL, "1988",
	"Golvellius (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_golvellRomInfo, sms_golvellRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GP Rider (Euro, Bra)

static struct BurnRomInfo sms_gpriderRomDesc[] = {
	{ "gp rider (europe).bin",	0x80000, 0xec2da554, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gprider)
STD_ROM_FN(sms_gprider)

struct BurnDriver BurnDrvsms_gprider = {
	"sms_gprider", NULL, NULL, NULL, "1993",
	"GP Rider (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_gpriderRomInfo, sms_gpriderRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Baseball (Euro, USA, Bra)

static struct BurnRomInfo sms_greatbasRomDesc[] = {
	{ "mpr-10415 w11.ic1",	0x20000, 0x10ed6b57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatbas)
STD_ROM_FN(sms_greatbas)

struct BurnDriver BurnDrvsms_greatbas = {
	"sms_greatbas", NULL, NULL, NULL, "1985",
	"Great Baseball (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatbasRomInfo, sms_greatbasRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Baseball (Jpn, Pirate?)

static struct BurnRomInfo sms_greatbasj1RomDesc[] = {
	{ "great baseball (j) [hack].bin",	0x08000, 0xc1e699db, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatbasj1)
STD_ROM_FN(sms_greatbasj1)

struct BurnDriver BurnDrvsms_greatbasj1 = {
	"sms_greatbasj1", "sms_greatbas", NULL, NULL, "1985",
	"Great Baseball (Jpn, Pirate?)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatbasj1RomInfo, sms_greatbasj1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Basketball (World)

static struct BurnRomInfo sms_greatbskRomDesc[] = {
	{ "mpr-11073.ic1",	0x20000, 0x2ac001eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatbsk)
STD_ROM_FN(sms_greatbsk)

struct BurnDriver BurnDrvsms_greatbsk = {
	"sms_greatbsk", NULL, NULL, NULL, "1987",
	"Great Basketball (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatbskRomInfo, sms_greatbskRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Football (World)

static struct BurnRomInfo sms_greatftbRomDesc[] = {
	{ "mpr-10576.ic1",	0x20000, 0x2055825f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatftb)
STD_ROM_FN(sms_greatftb)

struct BurnDriver BurnDrvsms_greatftb = {
	"sms_greatftb", NULL, NULL, NULL, "1987",
	"Great Football (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatftbRomInfo, sms_greatftbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Golf (Euro, USA) ~ Masters Golf (Japan)

static struct BurnRomInfo sms_greatglfRomDesc[] = {
	{ "mpr-11192 w36.ic1",	0x20000, 0x98e4ae4a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatglf)
STD_ROM_FN(sms_greatglf)

struct BurnDriver BurnDrvsms_greatglf = {
	"sms_greatglf", NULL, NULL, NULL, "1987",
	"Great Golf (Euro, USA) ~ Masters Golf (Japan)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatglfRomInfo, sms_greatglfRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Golf (Euro, USA, v1.0)

static struct BurnRomInfo sms_greatglf1RomDesc[] = {
	{ "great golf (ue) (v1.0) [!].bin",	0x20000, 0xc6611c84, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatglf1)
STD_ROM_FN(sms_greatglf1)

struct BurnDriver BurnDrvsms_greatglf1 = {
	"sms_greatglf1", "sms_greatglf", NULL, NULL, "1987",
	"Great Golf (Euro, USA, v1.0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatglf1RomInfo, sms_greatglf1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Golf (Prototype)

static struct BurnRomInfo sms_greatglfpRomDesc[] = {
	{ "great golf (world) (beta).bin",	0x20000, 0x4847bc91, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatglfp)
STD_ROM_FN(sms_greatglfp)

struct BurnDriver BurnDrvsms_greatglfp = {
	"sms_greatglfp", "sms_greatglf", NULL, NULL, "1987",
	"Great Golf (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatglfpRomInfo, sms_greatglfpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Golf (Jpn)

static struct BurnRomInfo sms_greatgljRomDesc[] = {
	{ "great golf (japan).bin",	0x20000, 0x6586bd1f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatglj)
STD_ROM_FN(sms_greatglj)

struct BurnDriver BurnDrvsms_greatglj = {
	"sms_greatglj", NULL, NULL, NULL, "1986",
	"Great Golf (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatgljRomInfo, sms_greatgljRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Golf (Kor)

static struct BurnRomInfo sms_greatglkRomDesc[] = {
	{ "great golf [jp] (kr).sms",	0x20000, 0x5def1bf5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatglk)
STD_ROM_FN(sms_greatglk)

struct BurnDriver BurnDrvsms_greatglk = {
	"sms_greatglk", "sms_greatglj", NULL, NULL, "19??",
	"Great Golf (Kor)\0", NULL, "Samsung?", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatglkRomInfo, sms_greatglkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Ice Hockey (Jpn, USA)

static struct BurnRomInfo sms_greaticeRomDesc[] = {
	{ "mpr-10389.ic1",	0x10000, 0x0cb7e21f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatice)
STD_ROM_FN(sms_greatice)

struct BurnDriver BurnDrvsms_greatice = {
	"sms_greatice", NULL, NULL, NULL, "1987",
	"Great Ice Hockey (Jpn, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greaticeRomInfo, sms_greaticeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Soccer (Euro)

static struct BurnRomInfo sms_greatscrRomDesc[] = {
	{ "great soccer (europe).bin",	0x08000, 0x0ed170c9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatscr)
STD_ROM_FN(sms_greatscr)

struct BurnDriver BurnDrvsms_greatscr = {
	"sms_greatscr", NULL, NULL, NULL, "1985",
	"Great Soccer (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatscrRomInfo, sms_greatscrRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Soccer (Tw)

static struct BurnRomInfo sms_greatscrtwRomDesc[] = {
	{ "great soccer (tw).bin",	0x08000, 0x84665648, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatscrtw)
STD_ROM_FN(sms_greatscrtw)

struct BurnDriver BurnDrvsms_greatscrtw = {
	"sms_greatscrtw", "sms_greatscr", NULL, NULL, "1985?",
	"Great Soccer (Tw)\0", NULL, "Aaronix", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatscrtwRomInfo, sms_greatscrtwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Volleyball (Euro, USA, Bra)

static struct BurnRomInfo sms_greatvolRomDesc[] = {
	{ "great volleyball (usa, europe).bin",	0x20000, 0x8d43ea95, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatvol)
STD_ROM_FN(sms_greatvol)

struct BurnDriver BurnDrvsms_greatvol = {
	"sms_greatvol", NULL, NULL, NULL, "1987",
	"Great Volleyball (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatvolRomInfo, sms_greatvolRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Volleyball (Jpn)

static struct BurnRomInfo sms_greatvoljRomDesc[] = {
	{ "great volleyball (japan).bin",	0x20000, 0x6819b0c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatvolj)
STD_ROM_FN(sms_greatvolj)

struct BurnDriver BurnDrvsms_greatvolj = {
	"sms_greatvolj", "sms_greatvol", NULL, NULL, "1987",
	"Great Volleyball (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatvoljRomInfo, sms_greatvoljRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gun.Smoke (Kor)

static struct BurnRomInfo sms_gunsmokeRomDesc[] = {
	{ "gun.smoke (kr).bin",	0x08000, 0x3af7ccad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_gunsmoke)
STD_ROM_FN(sms_gunsmoke)

struct BurnDriver BurnDrvsms_gunsmoke = {
	"sms_gunsmoke", NULL, NULL, NULL, "1990",
	"Gun.Smoke (Kor)\0", NULL, "ProSoft", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_gunsmokeRomInfo, sms_gunsmokeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Haja no Fuuin (Jpn)

static struct BurnRomInfo sms_hajafuinRomDesc[] = {
	{ "haja no fuuin (japan).bin",	0x40000, 0xb9fdf6d9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hajafuin)
STD_ROM_FN(sms_hajafuin)

struct BurnDriver BurnDrvsms_hajafuin = {
	"sms_hajafuin", "sms_miracle", NULL, NULL, "1987",
	"Haja no Fuuin (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hajafuinRomInfo, sms_hajafuinRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hang-On (Euro, Bra, Aus)

static struct BurnRomInfo sms_hangonRomDesc[] = {
	{ "hang-on (europe).bin",	0x08000, 0x071b045e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hangon)
STD_ROM_FN(sms_hangon)

struct BurnDriver BurnDrvsms_hangon = {
	"sms_hangon", NULL, NULL, NULL, "1985",
	"Hang-On (Euro, Bra, Aus)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hangonRomInfo, sms_hangonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hang-On and Astro Warrior (USA)

static struct BurnRomInfo sms_hangonawRomDesc[] = {
	{ "mpr-11125.ic1",	0x20000, 0x1c5059f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hangonaw)
STD_ROM_FN(sms_hangonaw)

struct BurnDriver BurnDrvsms_hangonaw = {
	"sms_hangonaw", NULL, NULL, NULL, "1986",
	"Hang-On and Astro Warrior (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hangonawRomInfo, sms_hangonawRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hang-On and Safari Hunt (USA)

static struct BurnRomInfo sms_hangonshRomDesc[] = {
	{ "mpr-10137 w02.ic1",	0x20000, 0xe167a561, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hangonsh)
STD_ROM_FN(sms_hangonsh)

struct BurnDriver BurnDrvsms_hangonsh = {
	"sms_hangonsh", NULL, NULL, NULL, "1986",
	"Hang-On and Safari Hunt (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hangonshRomInfo, sms_hangonshRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hang-On and Safari Hunt (USA, Prototype)

static struct BurnRomInfo sms_hangonshpRomDesc[] = {
	{ "hang-on and safari hunt [proto].bin",	0x20000, 0xa120b77f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hangonshp)
STD_ROM_FN(sms_hangonshp)

struct BurnDriver BurnDrvsms_hangonshp = {
	"sms_hangonshp", "sms_hangonsh", NULL, NULL, "1986",
	"Hang-On and Safari Hunt (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hangonshpRomInfo, sms_hangonshpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Heavyweight Champ (Euro)

static struct BurnRomInfo sms_heavywRomDesc[] = {
	{ "heavyweight champ (europe).bin",	0x40000, 0xfdab876a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_heavyw)
STD_ROM_FN(sms_heavyw)

struct BurnDriver BurnDrvsms_heavyw = {
	"sms_heavyw", NULL, NULL, NULL, "1991",
	"Heavyweight Champ (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_heavywRomInfo, sms_heavywRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Advanced Dungeons and Dragons - Heroes of the Lance (Euro, Bra)

static struct BurnRomInfo sms_herolancRomDesc[] = {
	{ "heroes of the lance (europe).bin",	0x80000, 0xcde13ffb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_herolanc)
STD_ROM_FN(sms_herolanc)

struct BurnDriver BurnDrvsms_herolanc = {
	"sms_herolanc", NULL, NULL, NULL, "1991",
	"Advanced Dungeons and Dragons - Heroes of the Lance (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_herolancRomInfo, sms_herolancRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// High School! Kimengumi (Jpn)

static struct BurnRomInfo sms_highscRomDesc[] = {
	{ "high school! kimengumi (japan).bin",	0x20000, 0x9eb1aa4f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_highsc)
STD_ROM_FN(sms_highsc)

struct BurnDriver BurnDrvsms_highsc = {
	"sms_highsc", NULL, NULL, NULL, "1986",
	"High School! Kimengumi (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_highscRomInfo, sms_highscRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hokuto no Ken (Jpn)

static struct BurnRomInfo sms_hokutoRomDesc[] = {
	{ "hokuto no ken (japan).bin",	0x20000, 0x24f5fe8c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hokuto)
STD_ROM_FN(sms_hokuto)

struct BurnDriver BurnDrvsms_hokuto = {
	"sms_hokuto", "sms_blackblt", NULL, NULL, "1986",
	"Hokuto no Ken (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hokutoRomInfo, sms_hokutoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hokuto no Ken (Tw)

static struct BurnRomInfo sms_hokutotwRomDesc[] = {
	{ "hokuto no ken (tw).bin",	0x20000, 0xc4ab363d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hokutotw)
STD_ROM_FN(sms_hokutotw)

struct BurnDriver BurnDrvsms_hokutotw = {
	"sms_hokutotw", "sms_blackblt", NULL, NULL, "1986?",
	"Hokuto no Ken (Tw)\0", NULL, "Aaronix", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hokutotwRomInfo, sms_hokutotwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Home Alone (Euro)

static struct BurnRomInfo sms_homeaRomDesc[] = {
	{ "home alone (europe).bin",	0x40000, 0xc9dbf936, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_homea)
STD_ROM_FN(sms_homea)

struct BurnDriver BurnDrvsms_homea = {
	"sms_homea", NULL, NULL, NULL, "1993",
	"Home Alone (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_homeaRomInfo, sms_homeaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hook (Euro, Prototype)

static struct BurnRomInfo sms_hookRomDesc[] = {
	{ "hook (europe) (proto).bin",	0x40000, 0x9ced34a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hook)
STD_ROM_FN(sms_hook)

struct BurnDriver BurnDrvsms_hook = {
	"sms_hook", NULL, NULL, NULL, "19??",
	"Hook (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hookRomInfo, sms_hookRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hoshi wo Sagashite... (Jpn)

static struct BurnRomInfo sms_hoshiwRomDesc[] = {
	{ "mpr-11454.ic1",	0x40000, 0x955a009e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hoshiw)
STD_ROM_FN(sms_hoshiw)

struct BurnDriver BurnDrvsms_hoshiw = {
	"sms_hoshiw", NULL, NULL, NULL, "1988",
	"Hoshi wo Sagashite... (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hoshiwRomInfo, sms_hoshiwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hwarang Ui Geom (Kor)

static struct BurnRomInfo sms_hwaranRomDesc[] = {
	{ "hwarang ui geom (korea).bin",	0x40000, 0xe4b7c56a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hwaran)
STD_ROM_FN(sms_hwaran)

struct BurnDriver BurnDrvsms_hwaran = {
	"sms_hwaran", "sms_kenseid", NULL, NULL, "1988",
	"Hwarang Ui Geom (Kor)\0", NULL, "Samsung", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hwaranRomInfo, sms_hwaranRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Impossible Mission (Euro, Bra)

static struct BurnRomInfo sms_impmissRomDesc[] = {
	{ "impossible mission (europe).bin",	0x20000, 0x64d6af3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_impmiss)
STD_ROM_FN(sms_impmiss)

struct BurnDriver BurnDrvsms_impmiss = {
	"sms_impmiss", NULL, NULL, NULL, "1990",
	"Impossible Mission (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_impmissRomInfo, sms_impmissRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Impossible Mission (Euro, Prototype)

static struct BurnRomInfo sms_impmisspRomDesc[] = {
	{ "impossible mission (europe) (beta).bin",	0x20000, 0x71c4ca8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_impmissp)
STD_ROM_FN(sms_impmissp)

struct BurnDriver BurnDrvsms_impmissp = {
	"sms_impmissp", "sms_impmiss", NULL, NULL, "1990",
	"Impossible Mission (Euro, Prototype)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_impmisspRomInfo, sms_impmisspRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Incredible Crash Dummies (Euro, Bra)

static struct BurnRomInfo sms_crashdumRomDesc[] = {
	{ "incredible crash dummies, the (europe).bin",	0x40000, 0xb4584dde, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_crashdum)
STD_ROM_FN(sms_crashdum)

struct BurnDriver BurnDrvsms_crashdum = {
	"sms_crashdum", NULL, NULL, NULL, "1993",
	"The Incredible Crash Dummies (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_crashdumRomInfo, sms_crashdumRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Incredible Hulk (Euro, Bra)

static struct BurnRomInfo sms_hulkRomDesc[] = {
	{ "incredible hulk, the (europe).bin",	0x80000, 0xbe9a7071, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hulk)
STD_ROM_FN(sms_hulk)

struct BurnDriver BurnDrvsms_hulk = {
	"sms_hulk", NULL, NULL, NULL, "1994",
	"The Incredible Hulk (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hulkRomInfo, sms_hulkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Indiana Jones and the Last Crusade (Euro, Bra)

static struct BurnRomInfo sms_indycrusRomDesc[] = {
	{ "mpr-13309.ic1",	0x40000, 0x8aeb574b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_indycrus)
STD_ROM_FN(sms_indycrus)

struct BurnDriver BurnDrvsms_indycrus = {
	"sms_indycrus", NULL, NULL, NULL, "1990",
	"Indiana Jones and the Last Crusade (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_indycrusRomInfo, sms_indycrusRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Indiana Jones and the Last Crusade (Euro, Prototype)

static struct BurnRomInfo sms_indycruspRomDesc[] = {
	{ "indiana jones and the last crusade (europe) (beta).bin",	0x40000, 0xacec894d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_indycrusp)
STD_ROM_FN(sms_indycrusp)

struct BurnDriver BurnDrvsms_indycrusp = {
	"sms_indycrusp", "sms_indycrus", NULL, NULL, "1990",
	"Indiana Jones and the Last Crusade (Euro, Prototype)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_indycruspRomInfo, sms_indycruspRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James 'Buster' Douglas Knockout Boxing (USA)

static struct BurnRomInfo sms_jbdougkoRomDesc[] = {
	{ "james 'buster' douglas knockout boxing (usa).bin",	0x40000, 0x6a664405, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_jbdougko)
STD_ROM_FN(sms_jbdougko)

struct BurnDriver BurnDrvsms_jbdougko = {
	"sms_jbdougko", "sms_heavyw", NULL, NULL, "1990",
	"James 'Buster' Douglas Knockout Boxing (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_jbdougkoRomInfo, sms_jbdougkoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James 'Buster' Douglas Knockout Boxing (USA, Prototype)

static struct BurnRomInfo sms_jbdougkopRomDesc[] = {
	{ "james buster douglas knockout boxing [proto].bin",	0x40000, 0xcfb4bd7b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_jbdougkop)
STD_ROM_FN(sms_jbdougkop)

struct BurnDriver BurnDrvsms_jbdougkop = {
	"sms_jbdougkop", "sms_heavyw", NULL, NULL, "1990",
	"James 'Buster' Douglas Knockout Boxing (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_jbdougkopRomInfo, sms_jbdougkopRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James Bond 007 - The Duel (Euro)

static struct BurnRomInfo sms_jb007RomDesc[] = {
	{ "mpr-15484.ic1",	0x40000, 0x8d23587f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_jb007)
STD_ROM_FN(sms_jb007)

struct BurnDriver BurnDrvsms_jb007 = {
	"sms_jb007", NULL, NULL, NULL, "1993",
	"James Bond 007 - The Duel (Euro)\0", NULL, "Domark", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_jb007RomInfo, sms_jb007RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James Bond 007 - The Duel (Bra)

static struct BurnRomInfo sms_jb007bRomDesc[] = {
	{ "james bond 007 - the duel (brazil).bin",	0x40000, 0x8feff688, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_jb007b)
STD_ROM_FN(sms_jb007b)

struct BurnDriver BurnDrvsms_jb007b = {
	"sms_jb007b", "sms_jb007", NULL, NULL, "1993",
	"James Bond 007 - The Duel (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_jb007bRomInfo, sms_jb007bRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Janggun ui Adeul (Kor)

static struct BurnRomInfo sms_janggunRomDesc[] = {
	{ "janggun ui adeul (kr).bin",	0x80000, 0x192949d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_janggun)
STD_ROM_FN(sms_janggun)

struct BurnDriver BurnDrvsms_janggun = {
	"sms_janggun", NULL, NULL, NULL, "1992",
	"Janggun ui Adeul (Kor)\0", NULL, "Daou Infosys", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_KOREA8K, GBF_MISC, 0,
	SMSGetZipName, sms_janggunRomInfo, sms_janggunRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jang Pung 3 (Kor)

static struct BurnRomInfo sms_jangpun3RomDesc[] = {
	{ "jang pung 3 (kr).bin",	0x100000, 0x18fb98a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_jangpun3)
STD_ROM_FN(sms_jangpun3)

struct BurnDriver BurnDrvsms_jangpun3 = {
	"sms_jangpun3", NULL, NULL, NULL, "1994",
	"Jang Pung 3 (Kor)\0", NULL, "Sanghun", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_KOREA, GBF_MISC, 0,
	SMSGetZipName, sms_jangpun3RomInfo, sms_jangpun3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Joe Montana Football (Euro, USA, Bra)

static struct BurnRomInfo sms_joemontRomDesc[] = {
	{ "mpr-13605 t35.ic1",	0x40000, 0x0a9089e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_joemont)
STD_ROM_FN(sms_joemont)

struct BurnDriver BurnDrvsms_joemont = {
	"sms_joemont", NULL, NULL, NULL, "1990",
	"Joe Montana Football (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_joemontRomInfo, sms_joemontRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Jungle Book (Euro, Bra)

static struct BurnRomInfo sms_jungleRomDesc[] = {
	{ "mpr-16057.ic1",	0x40000, 0x695a9a15, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_jungle)
STD_ROM_FN(sms_jungle)

struct BurnDriver BurnDrvsms_jungle = {
	"sms_jungle", NULL, NULL, NULL, "1994",
	"The Jungle Book (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_jungleRomInfo, sms_jungleRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jurassic Park (Euro)

static struct BurnRomInfo sms_jparkRomDesc[] = {
	{ "jurassic park (europe).bin",	0x80000, 0x0667ed9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_jpark)
STD_ROM_FN(sms_jpark)

struct BurnDriver BurnDrvsms_jpark = {
	"sms_jpark", NULL, NULL, NULL, "1993",
	"Jurassic Park (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_jparkRomInfo, sms_jparkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kenseiden (Euro, USA, Bra)

static struct BurnRomInfo sms_kenseidRomDesc[] = {
	{ "kenseiden (usa, europe).bin",	0x40000, 0x516ed32e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_kenseid)
STD_ROM_FN(sms_kenseid)

struct BurnDriver BurnDrvsms_kenseid = {
	"sms_kenseid", NULL, NULL, NULL, "1988",
	"Kenseiden (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_kenseidRomInfo, sms_kenseidRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kenseiden (Jpn)

static struct BurnRomInfo sms_kenseidjRomDesc[] = {
	{ "mpr-11488.ic1",	0x40000, 0x05ea5353, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_kenseidj)
STD_ROM_FN(sms_kenseidj)

struct BurnDriver BurnDrvsms_kenseidj = {
	"sms_kenseidj", "sms_kenseid", NULL, NULL, "1988",
	"Kenseiden (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_kenseidjRomInfo, sms_kenseidjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// King's Quest - Quest for the Crown (USA)

static struct BurnRomInfo sms_kingqstRomDesc[] = {
	{ "king's quest - quest for the crown (usa).bin",	0x20000, 0xf8d33bc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_kingqst)
STD_ROM_FN(sms_kingqst)

struct BurnDriver BurnDrvsms_kingqst = {
	"sms_kingqst", NULL, NULL, NULL, "1989",
	"King's Quest - Quest for the Crown (USA)\0", NULL, "Parker Brothers", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_kingqstRomInfo, sms_kingqstRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// King's Quest - Quest for the Crown (USA, Prototype)

static struct BurnRomInfo sms_kingqstpRomDesc[] = {
	{ "king's quest - quest for the crown (usa) (beta).bin",	0x20000, 0xb7fe0a9d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_kingqstp)
STD_ROM_FN(sms_kingqstp)

struct BurnDriver BurnDrvsms_kingqstp = {
	"sms_kingqstp", "sms_kingqst", NULL, NULL, "1989",
	"King's Quest - Quest for the Crown (USA, Prototype)\0", NULL, "Parker Brothers", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_kingqstpRomInfo, sms_kingqstpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// King and Balloon (Kor)

static struct BurnRomInfo sms_kingballRomDesc[] = {
	{ "king and balloon (kr).sms",	0x08000, 0x0ae470e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_kingball)
STD_ROM_FN(sms_kingball)

struct BurnDriver BurnDrvsms_kingball = {
	"sms_kingball", NULL, NULL, NULL, "19??",
	"King and Balloon (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_kingballRomInfo, sms_kingballRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Klax (Euro)

static struct BurnRomInfo sms_klaxRomDesc[] = {
	{ "klax (europe).bin",	0x20000, 0x2b435fd6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_klax)
STD_ROM_FN(sms_klax)

struct BurnDriver BurnDrvsms_klax = {
	"sms_klax", NULL, NULL, NULL, "1991",
	"Klax (Euro)\0", NULL, "Tengen", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_klaxRomInfo, sms_klaxRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Knightmare II - The Maze of Galious (Kor)

static struct BurnRomInfo sms_knightm2RomDesc[] = {
	{ "knightmare ii - the maze of galious [kr].bin",	0x20000, 0xf89af3cc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_knightm2)
STD_ROM_FN(sms_knightm2)

struct BurnDriver BurnDrvsms_knightm2 = {
	"sms_knightm2", NULL, NULL, NULL, "199?",
	"Knightmare II - The Maze of Galious (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_knightm2RomInfo, sms_knightm2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Krusty's Fun House (Euro, Bra)

static struct BurnRomInfo sms_krustyfhRomDesc[] = {
	{ "krusty's fun house (europe).bin",	0x40000, 0x64a585eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_krustyfh)
STD_ROM_FN(sms_krustyfh)

struct BurnDriver BurnDrvsms_krustyfh = {
	"sms_krustyfh", NULL, NULL, NULL, "1992",
	"Krusty's Fun House (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_krustyfhRomInfo, sms_krustyfhRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kujaku Ou (Jpn)

static struct BurnRomInfo sms_kujakuRomDesc[] = {
	{ "kujaku ou (japan).bin",	0x80000, 0xd11d32e4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_kujaku)
STD_ROM_FN(sms_kujaku)

struct BurnDriver BurnDrvsms_kujaku = {
	"sms_kujaku", "sms_spellcst", NULL, NULL, "1988",
	"Kujaku Ou (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_kujakuRomInfo, sms_kujakuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kung Fu Kid (Euro, USA, Bra)

static struct BurnRomInfo sms_kungfukRomDesc[] = {
	{ "mpr-11069.ic1",	0x20000, 0x1e949d1f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_kungfuk)
STD_ROM_FN(sms_kungfuk)

struct BurnDriver BurnDrvsms_kungfuk = {
	"sms_kungfuk", NULL, NULL, NULL, "1987",
	"Kung Fu Kid (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_kungfukRomInfo, sms_kungfukRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Land of Illusion Starring Mickey Mouse (Euro, Bra)

static struct BurnRomInfo sms_landillRomDesc[] = {
	{ "mpr-15163.ic1",	0x80000, 0x24e97200, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_landill)
STD_ROM_FN(sms_landill)

struct BurnDriver BurnDrvsms_landill = {
	"sms_landill", NULL, NULL, NULL, "1992",
	"Land of Illusion Starring Mickey Mouse (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_landillRomInfo, sms_landillRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Laser Ghost (Euro)

static struct BurnRomInfo sms_lghostRomDesc[] = {
	{ "laser ghost (europe).bin",	0x40000, 0x0ca95637, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lghost)
STD_ROM_FN(sms_lghost)

struct BurnDriver BurnDrvsms_lghost = {
	"sms_lghost", NULL, NULL, NULL, "1991",
	"Laser Ghost (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lghostRomInfo, sms_lghostRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Bra)

static struct BurnRomInfo sms_legndillRomDesc[] = {
	{ "mpr-20989-s.ic1",	0x80000, 0x6350e649, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_legndill)
STD_ROM_FN(sms_legndill)

struct BurnDriver BurnDrvsms_legndill = {
	"sms_legndill", NULL, NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_legndillRomInfo, sms_legndillRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lemmings (Euro, Bra, Kor)

static struct BurnRomInfo sms_lemmingsRomDesc[] = {
	{ "mpr-15221-f.ic1",	0x40000, 0xf369b2d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lemmings)
STD_ROM_FN(sms_lemmings)

struct BurnDriver BurnDrvsms_lemmings = {
	"sms_lemmings", NULL, NULL, NULL, "1991",
	"Lemmings (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lemmingsRomInfo, sms_lemmingsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lemmings (Euro, Prototype)

static struct BurnRomInfo sms_lemmingspRomDesc[] = {
	{ "lemmings (europe) (beta).bin",	0x40000, 0x2c61ed88, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lemmingsp)
STD_ROM_FN(sms_lemmingsp)

struct BurnDriver BurnDrvsms_lemmingsp = {
	"sms_lemmingsp", "sms_lemmings", NULL, NULL, "1991",
	"Lemmings (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lemmingspRomInfo, sms_lemmingspRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lemmings 2 - The Tribes (Euro, Prototype)

static struct BurnRomInfo sms_lemming2RomDesc[] = {
	{ "lemmings 2 - the tribes [proto].bin",	0x80000, 0xcf5aecca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lemming2)
STD_ROM_FN(sms_lemming2)

struct BurnDriver BurnDrvsms_lemming2 = {
	"sms_lemming2", NULL, NULL, NULL, "1994",
	"Lemmings 2 - The Tribes (Euro, Prototype)\0", NULL, "Psygnosis", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lemming2RomInfo, sms_lemming2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Line of Fire (Euro, Bra, Kor)

static struct BurnRomInfo sms_loffireRomDesc[] = {
	{ "line of fire (europe).bin",	0x80000, 0xcb09f355, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_loffire)
STD_ROM_FN(sms_loffire)

struct BurnDriver BurnDrvsms_loffire = {
	"sms_loffire", NULL, NULL, NULL, "1991",
	"Line of Fire (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_loffireRomInfo, sms_loffireRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's The Lion King (Euro, Bra)

static struct BurnRomInfo sms_lionkingRomDesc[] = {
	{ "lion king, the (europe).bin",	0x80000, 0xc352c7eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lionking)
STD_ROM_FN(sms_lionking)

struct BurnDriver BurnDrvsms_lionking = {
	"sms_lionking", NULL, NULL, NULL, "1994",
	"Disney's The Lion King (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lionkingRomInfo, sms_lionkingRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lord of the Sword (Euro, USA, Bra)

static struct BurnRomInfo sms_lordswrdRomDesc[] = {
	{ "lord of the sword (usa, europe).bin",	0x40000, 0xe8511b08, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lordswrd)
STD_ROM_FN(sms_lordswrd)

struct BurnDriver BurnDrvsms_lordswrd = {
	"sms_lordswrd", NULL, NULL, NULL, "1988",
	"Lord of the Sword (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lordswrdRomInfo, sms_lordswrdRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lord of Sword (Jpn)

static struct BurnRomInfo sms_lordswrdjRomDesc[] = {
	{ "lord of sword (japan).bin",	0x40000, 0xaa7d6f45, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lordswrdj)
STD_ROM_FN(sms_lordswrdj)

struct BurnDriver BurnDrvsms_lordswrdj = {
	"sms_lordswrdj", "sms_lordswrd", NULL, NULL, "1988",
	"Lord of Sword (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lordswrdjRomInfo, sms_lordswrdjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Loretta no Shouzou (Jpn)

static struct BurnRomInfo sms_lorettaRomDesc[] = {
	{ "mpr-10517.ic2",	0x20000, 0x323f357f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_loretta)
STD_ROM_FN(sms_loretta)

struct BurnDriver BurnDrvsms_loretta = {
	"sms_loretta", NULL, NULL, NULL, "1987",
	"Loretta no Shouzou (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lorettaRomInfo, sms_lorettaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lucky Dime Caper Starring Donald Duck (Euro, Bra)

static struct BurnRomInfo sms_luckydimRomDesc[] = {
	{ "mpr-14358-f.ic1",	0x40000, 0xa1710f13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_luckydim)
STD_ROM_FN(sms_luckydim)

struct BurnDriver BurnDrvsms_luckydim = {
	"sms_luckydim", NULL, NULL, NULL, "1991",
	"The Lucky Dime Caper Starring Donald Duck (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_luckydimRomInfo, sms_luckydimRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lucky Dime Caper Starring Donald Duck (Euro, Prototype)

static struct BurnRomInfo sms_luckydimpRomDesc[] = {
	{ "lucky dime caper starring donald duck, the (europe) (beta).bin",	0x40000, 0x7f6d0df6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_luckydimp)
STD_ROM_FN(sms_luckydimp)

struct BurnDriver BurnDrvsms_luckydimp = {
	"sms_luckydimp", "sms_luckydim", NULL, NULL, "1991",
	"The Lucky Dime Caper Starring Donald Duck (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_luckydimpRomInfo, sms_luckydimpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mahjong Sengoku Jidai (Jpn, HK)

static struct BurnRomInfo sms_mjsengokRomDesc[] = {
	{ "mpr-11193.ic1",	0x20000, 0xbcfbfc67, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mjsengok)
STD_ROM_FN(sms_mjsengok)

struct BurnDriver BurnDrvsms_mjsengok = {
	"sms_mjsengok", NULL, NULL, NULL, "1987",
	"Mahjong Sengoku Jidai (Jpn, HK)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mjsengokRomInfo, sms_mjsengokRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mahjong Sengoku Jidai (Jpn, Prototype)

static struct BurnRomInfo sms_mjsengokpRomDesc[] = {
	{ "mahjong sengoku jidai (japan) (beta).bin",	0x20000, 0x996b2a07, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mjsengokp)
STD_ROM_FN(sms_mjsengokp)

struct BurnDriver BurnDrvsms_mjsengokp = {
	"sms_mjsengokp", "sms_mjsengok", NULL, NULL, "1987",
	"Mahjong Sengoku Jidai (Jpn, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mjsengokpRomInfo, sms_mjsengokpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Makai Retsuden (Jpn)

static struct BurnRomInfo sms_makairetRomDesc[] = {
	{ "mpr-10742.ic1",	0x20000, 0xca860451, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_makairet)
STD_ROM_FN(sms_makairet)

struct BurnDriver BurnDrvsms_makairet = {
	"sms_makairet", "sms_kungfuk", NULL, NULL, "1987",
	"Makai Retsuden (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_makairetRomInfo, sms_makairetRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Maou Golvellius (Jpn)

static struct BurnRomInfo sms_maougolvRomDesc[] = {
	{ "maou golvellius (japan).bin",	0x40000, 0xbf0411ad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_maougolv)
STD_ROM_FN(sms_maougolv)

struct BurnDriver BurnDrvsms_maougolv = {
	"sms_maougolv", "sms_golvell", NULL, NULL, "1988",
	"Maou Golvellius (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_maougolvRomInfo, sms_maougolvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Maou Golvellius (Jpn, Prototype)

static struct BurnRomInfo sms_maougolvpRomDesc[] = {
	{ "maou golvellius [proto] (jp).bin",	0x40000, 0x21a21352, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_maougolvp)
STD_ROM_FN(sms_maougolvp)

struct BurnDriver BurnDrvsms_maougolvp = {
	"sms_maougolvp", "sms_golvell", NULL, NULL, "1988",
	"Maou Golvellius (Jpn, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_maougolvpRomInfo, sms_maougolvpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Marble Madness (Euro)

static struct BurnRomInfo sms_marbleRomDesc[] = {
	{ "marble madness (europe).bin",	0x40000, 0xbf6f3e5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_marble)
STD_ROM_FN(sms_marble)

struct BurnDriver BurnDrvsms_marble = {
	"sms_marble", NULL, NULL, NULL, "1992",
	"Marble Madness (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_marbleRomInfo, sms_marbleRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Marksman Shooting and Trap Shooting and Safari Hunt (Euro, Bra)

static struct BurnRomInfo sms_marksmanRomDesc[] = {
	{ "mpr-10157f.ic2",	0x20000, 0xe8215c2e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_marksman)
STD_ROM_FN(sms_marksman)

struct BurnDriver BurnDrvsms_marksman = {
	"sms_marksman", NULL, NULL, NULL, "1986",
	"Marksman Shooting and Trap Shooting and Safari Hunt (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_marksmanRomInfo, sms_marksmanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Marksman Shooting and Trap Shooting (USA)

static struct BurnRomInfo sms_marksmanuRomDesc[] = {
	{ "marksman shooting and trap shooting (usa).bin",	0x20000, 0xe8ea842c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_marksmanu)
STD_ROM_FN(sms_marksmanu)

struct BurnDriver BurnDrvsms_marksmanu = {
	"sms_marksmanu", "sms_marksman", NULL, NULL, "1986",
	"Marksman Shooting and Trap Shooting (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_marksmanuRomInfo, sms_marksmanuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Masters of Combat (Euro, Bra, Aus)

static struct BurnRomInfo sms_mastcombRomDesc[] = {
	{ "masters of combat (europe).bin",	0x40000, 0x93141463, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mastcomb)
STD_ROM_FN(sms_mastcomb)

struct BurnDriver BurnDrvsms_mastcomb = {
	"sms_mastcomb", NULL, NULL, NULL, "1993",
	"Masters of Combat (Euro, Bra, Aus)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mastcombRomInfo, sms_mastcombRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Master of Darkness (Euro, Bra)

static struct BurnRomInfo sms_mastdarkRomDesc[] = {
	{ "mpr-15203-f.ic1",	0x40000, 0x96fb4d4b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mastdark)
STD_ROM_FN(sms_mastdark)

struct BurnDriver BurnDrvsms_mastdark = {
	"sms_mastdark", NULL, NULL, NULL, "1992",
	"Master of Darkness (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mastdarkRomInfo, sms_mastdarkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Maze Hunter 3-D (Euro, USA, Bra)

static struct BurnRomInfo sms_mazehuntRomDesc[] = {
	{ "mpr-11564.ic1",	0x20000, 0x31b8040b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mazehunt)
STD_ROM_FN(sms_mazehunt)

struct BurnDriver BurnDrvsms_mazehunt = {
	"sms_mazehunt", NULL, NULL, NULL, "1988",
	"Maze Hunter 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mazehuntRomInfo, sms_mazehuntRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Maze Walker (Jpn)

static struct BurnRomInfo sms_mazewalkRomDesc[] = {
	{ "mpr-11337.ic1",	0x20000, 0x871562b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mazewalk)
STD_ROM_FN(sms_mazewalk)

struct BurnDriver BurnDrvsms_mazewalk = {
	"sms_mazewalk", "sms_mazehunt", NULL, NULL, "1988",
	"Maze Walker (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mazewalkRomInfo, sms_mazewalkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Megumi Rescue (Jpn)

static struct BurnRomInfo sms_megumiRomDesc[] = {
	{ "megumi rescue (japan).bin",	0x20000, 0x29bc7fad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_megumi)
STD_ROM_FN(sms_megumi)

struct BurnDriver BurnDrvsms_megumi = {
	"sms_megumi", NULL, NULL, NULL, "1988",
	"Megumi Rescue (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_megumiRomInfo, sms_megumiRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mercs (Euro, Bra)

static struct BurnRomInfo sms_mercsRomDesc[] = {
	{ "mpr-14269-f.ic1",	0x80000, 0xd7416b83, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mercs)
STD_ROM_FN(sms_mercs)

struct BurnDriver BurnDrvsms_mercs = {
	"sms_mercs", NULL, NULL, NULL, "1990",
	"Mercs (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mercsRomInfo, sms_mercsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mickey's Ultimate Challenge (Bra)

static struct BurnRomInfo sms_mickeyucRomDesc[] = {
	{ "mickey's ultimate challenge (brazil).bin",	0x40000, 0x25051dd5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mickeyuc)
STD_ROM_FN(sms_mickeyuc)

struct BurnDriver BurnDrvsms_mickeyuc = {
	"sms_mickeyuc", NULL, NULL, NULL, "1998",
	"Mickey's Ultimate Challenge (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mickeyucRomInfo, sms_mickeyucRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mick and Mack as the Global Gladiators (Euro, Bra)

static struct BurnRomInfo sms_mickmackRomDesc[] = {
	{ "mpr-15510-f.ic1",	0x40000, 0xb67ceb76, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mickmack)
STD_ROM_FN(sms_mickmack)

struct BurnDriver BurnDrvsms_mickmack = {
	"sms_mickmack", NULL, NULL, NULL, "1993",
	"Mick and Mack as the Global Gladiators (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mickmackRomInfo, sms_mickmackRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Micro Machines (Euro)

static struct BurnRomInfo sms_micromacRomDesc[] = {
	{ "micro machines (europe).bin",	0x40000, 0xa577ce46, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_micromac)
STD_ROM_FN(sms_micromac)

struct BurnDriver BurnDrvsms_micromac = {
	"sms_micromac", NULL, NULL, NULL, "1994",
	"Micro Machines (Euro)\0", NULL, "Codemasters", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_micromacRomInfo, sms_micromacRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Show do Milhão (Bra, Prototype)

static struct BurnRomInfo sms_sdmilhaoRomDesc[] = {
	{ "milhao.bin",	0x20000, 0x58423688, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sdmilhao)
STD_ROM_FN(sms_sdmilhao)

struct BurnDriver BurnDrvsms_sdmilhao = {
	"sms_sdmilhao", NULL, NULL, NULL, "2003",
	"Show do Milhao (Bra, Prototype)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sdmilhaoRomInfo, sms_sdmilhaoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Miracle Warriors - Seal of the Dark Lord (Euro, USA, Bra)

static struct BurnRomInfo sms_miracleRomDesc[] = {
	{ "miracle warriors - seal of the dark lord (usa, europe).bin",	0x40000, 0x0e333b6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_miracle)
STD_ROM_FN(sms_miracle)

struct BurnDriver BurnDrvsms_miracle = {
	"sms_miracle", NULL, NULL, NULL, "1987",
	"Miracle Warriors - Seal of the Dark Lord (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_miracleRomInfo, sms_miracleRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Miracle Warriors - Seal of the Dark Lord (Prototype)

static struct BurnRomInfo sms_miraclepRomDesc[] = {
	{ "miracle warriors - seal of the dark lord (usa, europe) (beta).bin",	0x40000, 0x301a59aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_miraclep)
STD_ROM_FN(sms_miraclep)

struct BurnDriver BurnDrvsms_miraclep = {
	"sms_miraclep", "sms_miracle", NULL, NULL, "1987",
	"Miracle Warriors - Seal of the Dark Lord (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_miraclepRomInfo, sms_miraclepRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Missile Defense 3-D (Euro, USA, Bra)

static struct BurnRomInfo sms_missil3dRomDesc[] = {
	{ "mpr-10844 w29.ic1",	0x20000, 0xfbe5cfbb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_missil3d)
STD_ROM_FN(sms_missil3d)

struct BurnDriver BurnDrvsms_missil3d = {
	"sms_missil3d", NULL, NULL, NULL, "19??",
	"Missile Defense 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_missil3dRomInfo, sms_missil3dRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mortal Kombat (Euro, Bra)

static struct BurnRomInfo sms_mkRomDesc[] = {
	{ "mpr-15757.ic1",	0x80000, 0x302dc686, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mk)
STD_ROM_FN(sms_mk)

struct BurnDriver BurnDrvsms_mk = {
	"sms_mk", NULL, NULL, NULL, "1993",
	"Mortal Kombat (Euro, Bra)\0", NULL, "Arena", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mkRomInfo, sms_mkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mortal Kombat II (Euro, Bra)

static struct BurnRomInfo sms_mk2RomDesc[] = {
	{ "mortal kombat ii (europe).bin",	0x80000, 0x2663bf18, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mk2)
STD_ROM_FN(sms_mk2)

struct BurnDriver BurnDrvsms_mk2 = {
	"sms_mk2", NULL, NULL, NULL, "1994",
	"Mortal Kombat II (Euro, Bra)\0", NULL, "Acclaim Entertainment", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mk2RomInfo, sms_mk2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mortal Kombat 3 (Bra)

static struct BurnRomInfo sms_mk3RomDesc[] = {
	{ "mortal kombat 3 (brazil).bin",	0x80000, 0x395ae757, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mk3)
STD_ROM_FN(sms_mk3)

struct BurnDriver BurnDrvsms_mk3 = {
	"sms_mk3", NULL, NULL, NULL, "1996",
	"Mortal Kombat 3 (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mk3RomInfo, sms_mk3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mônica no Castelo do Dragao (Bra)

static struct BurnRomInfo sms_monicaRomDesc[] = {
	{ "monica no castelo do dragao (brazil).bin",	0x40000, 0x01d67c0b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_monica)
STD_ROM_FN(sms_monica)

struct BurnDriver BurnDrvsms_monica = {
	"sms_monica", "sms_wboymlnd", NULL, NULL, "1991",
	"Monica no Castelo do Dragao (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_monicaRomInfo, sms_monicaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Monopoly (Euro)

static struct BurnRomInfo sms_monopolyRomDesc[] = {
	{ "monopoly (usa, europe).bin",	0x20000, 0x026d94a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_monopoly)
STD_ROM_FN(sms_monopoly)

struct BurnDriver BurnDrvsms_monopoly = {
	"sms_monopoly", NULL, NULL, NULL, "1988",
	"Monopoly (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_monopolyRomInfo, sms_monopolyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Monopoly (USA)

static struct BurnRomInfo sms_monopolyuRomDesc[] = {
	{ "monopoly (usa).bin",	0x20000, 0x69538469, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_monopolyu)
STD_ROM_FN(sms_monopolyu)

struct BurnDriver BurnDrvsms_monopolyu = {
	"sms_monopolyu", "sms_monopoly", NULL, NULL, "1988",
	"Monopoly (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_monopolyuRomInfo, sms_monopolyuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Monopoly (USA, Prototype)

static struct BurnRomInfo sms_monopolypRomDesc[] = {
	{ "monopoly [proto].bin",	0x20000, 0x7e9d87fc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_monopolyp)
STD_ROM_FN(sms_monopolyp)

struct BurnDriver BurnDrvsms_monopolyp = {
	"sms_monopolyp", "sms_monopoly", NULL, NULL, "1988",
	"Monopoly (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_monopolypRomInfo, sms_monopolypRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Montezuma's Revenge Featuring Panama Joe (USA)

static struct BurnRomInfo sms_montezumRomDesc[] = {
	{ "mpr-12402.ic1",	0x20000, 0x82fda895, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_montezum)
STD_ROM_FN(sms_montezum)

struct BurnDriver BurnDrvsms_montezum = {
	"sms_montezum", NULL, NULL, NULL, "1989",
	"Montezuma's Revenge Featuring Panama Joe (USA)\0", NULL, "Parker Brothers", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_montezumRomInfo, sms_montezumRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Montezuma's Revenge Featuring Panama Joe (USA, Prototype)

static struct BurnRomInfo sms_montezumpRomDesc[] = {
	{ "montezuma's revenge - featuring panama joe [proto].bin",	0x20000, 0x575d0fcf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_montezump)
STD_ROM_FN(sms_montezump)

struct BurnDriver BurnDrvsms_montezump = {
	"sms_montezump", "sms_montezum", NULL, NULL, "1989",
	"Montezuma's Revenge Featuring Panama Joe (USA, Prototype)\0", NULL, "Parker Brothers", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_montezumpRomInfo, sms_montezumpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mopiranger (Kor)

static struct BurnRomInfo sms_mopirangRomDesc[] = {
	{ "mopiranger (kr).bin",	0x08000, 0xb49aa6fc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mopirang)
STD_ROM_FN(sms_mopirang)

struct BurnDriver BurnDrvsms_mopirang = {
	"sms_mopirang", NULL, NULL, NULL, "19??",
	"Mopiranger (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_mopirangRomInfo, sms_mopirangRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ms. Pac-Man (Euro, Bra)

static struct BurnRomInfo sms_mspacmanRomDesc[] = {
	{ "ms. pac-man (europe).bin",	0x20000, 0x3cd816c6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mspacman)
STD_ROM_FN(sms_mspacman)

struct BurnDriver BurnDrvsms_mspacman = {
	"sms_mspacman", NULL, NULL, NULL, "1991",
	"Ms. Pac-Man (Euro, Bra)\0", NULL, "Tengen", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mspacmanRomInfo, sms_mspacmanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Michael Jackson's Moonwalker (Euro, USA, Bra, Kor)

static struct BurnRomInfo sms_mwalkRomDesc[] = {
	{ "michael jackson's moonwalker (usa, europe).bin",	0x40000, 0x56cc906b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mwalk)
STD_ROM_FN(sms_mwalk)

struct BurnDriver BurnDrvsms_mwalk = {
	"sms_mwalk", NULL, NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (Euro, USA, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mwalkRomInfo, sms_mwalkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Michael Jackson's Moonwalker (USA, Prototype)

static struct BurnRomInfo sms_mwalkpRomDesc[] = {
	{ "michael jackson's moonwalker (usa, europe) (beta).bin",	0x40000, 0x54123936, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_mwalkp)
STD_ROM_FN(sms_mwalkp)

struct BurnDriver BurnDrvsms_mwalkp = {
	"sms_mwalkp", "sms_mwalk", NULL, NULL, "1990",
	"Michael Jackson's Moonwalker (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_mwalkpRomInfo, sms_mwalkpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Micro Xevious (Kor)

static struct BurnRomInfo sms_xeviousRomDesc[] = {
	{ "micro xevious, the (kr).bin",	0x08000, 0x41cc2ade, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_xevious)
STD_ROM_FN(sms_xevious)

struct BurnDriver BurnDrvsms_xevious = {
	"sms_xevious", NULL, NULL, NULL, "1990",
	"The Micro Xevious (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_xeviousRomInfo, sms_xeviousRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// My Hero (Euro, USA, Bra)

static struct BurnRomInfo sms_myheroRomDesc[] = {
	{ "my hero (usa, europe).bin",	0x08000, 0x62f0c23d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_myhero)
STD_ROM_FN(sms_myhero)

struct BurnDriver BurnDrvsms_myhero = {
	"sms_myhero", NULL, NULL, NULL, "1986",
	"My Hero (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_myheroRomInfo, sms_myheroRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Jam (Euro, Prototype)

static struct BurnRomInfo sms_nbajamRomDesc[] = {
	{ "nba jam [proto].bin",	0x80000, 0x332a847d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_nbajam)
STD_ROM_FN(sms_nbajam)

struct BurnDriver BurnDrvsms_nbajam = {
	"sms_nbajam", NULL, NULL, NULL, "1994",
	"NBA Jam (Euro, Prototype)\0", NULL, "Acclaim", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_nbajamRomInfo, sms_nbajamRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Golf (Prototype)

static struct BurnRomInfo sms_supgolfRomDesc[] = {
	{ "super golf [proto].bin",	0x40000, 0x7f7b568d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_supgolf)
STD_ROM_FN(sms_supgolf)

struct BurnDriver BurnDrvsms_supgolf = {
	"sms_supgolf", "sms_golfaman", NULL, NULL, "1989",
	"Super Golf (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_supgolfRomInfo, sms_supgolfRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nekkyuu Koushien (Jpn)

static struct BurnRomInfo sms_nekkyuRomDesc[] = {
	{ "mpr-11886.ic1",	0x40000, 0x5b5f9106, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_nekkyu)
STD_ROM_FN(sms_nekkyu)

struct BurnDriver BurnDrvsms_nekkyu = {
	"sms_nekkyu", NULL, NULL, NULL, "1988",
	"Nekkyuu Koushien (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_nekkyuRomInfo, sms_nekkyuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nemesis (Kor)

static struct BurnRomInfo sms_nemesisRomDesc[] = {
	{ "nemesis (kr).bin",	0x20000, 0xe316c06d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_nemesis)
STD_ROM_FN(sms_nemesis)

struct BurnDriver BurnDrvsms_nemesis = {
	"sms_nemesis", NULL, NULL, NULL, "1987",
	"Nemesis (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX_NEMESIS, GBF_MISC, 0,
	SMSGetZipName, sms_nemesisRomInfo, sms_nemesisRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nemesis 2 (Kor)

static struct BurnRomInfo sms_nemesis2RomDesc[] = {
	{ "nemesis 2 (kr).bin",	0x20000, 0x0a77fa5e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_nemesis2)
STD_ROM_FN(sms_nemesis2)

struct BurnDriver BurnDrvsms_nemesis2 = {
	"sms_nemesis2", NULL, NULL, NULL, "1987",
	"Nemesis 2 (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_nemesis2RomInfo, sms_nemesis2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Ninja (Euro, USA)

static struct BurnRomInfo sms_ninjaRomDesc[] = {
	{ "mpr-11046.ic1",	0x20000, 0x66a15bd9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ninja)
STD_ROM_FN(sms_ninja)

struct BurnDriver BurnDrvsms_ninja = {
	"sms_ninja", NULL, NULL, NULL, "1986",
	"The Ninja (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ninjaRomInfo, sms_ninjaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Ninja (Jpn)

static struct BurnRomInfo sms_ninjajRomDesc[] = {
	{ "mpr-10409.ic1",	0x20000, 0x320313ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ninjaj)
STD_ROM_FN(sms_ninjaj)

struct BurnDriver BurnDrvsms_ninjaj = {
	"sms_ninjaj", "sms_ninja", NULL, NULL, "1986",
	"The Ninja (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ninjajRomInfo, sms_ninjajRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ninja Gaiden (Euro, Bra)

static struct BurnRomInfo sms_ngaidenRomDesc[] = {
	{ "mpr-14677.ic1",	0x40000, 0x1b1d8cc2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ngaiden)
STD_ROM_FN(sms_ngaiden)

struct BurnDriver BurnDrvsms_ngaiden = {
	"sms_ngaiden", NULL, NULL, NULL, "1992",
	"Ninja Gaiden (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ngaidenRomInfo, sms_ngaidenRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ninja Gaiden (Euro, Prototype)

static struct BurnRomInfo sms_ngaidenpRomDesc[] = {
	{ "ninja gaiden (europe) (beta).bin",	0x40000, 0x761e9396, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ngaidenp)
STD_ROM_FN(sms_ngaidenp)

struct BurnDriver BurnDrvsms_ngaidenp = {
	"sms_ngaidenp", "sms_ngaiden", NULL, NULL, "1992",
	"Ninja Gaiden (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ngaidenpRomInfo, sms_ngaidenpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Olympic Gold (Euro)

static struct BurnRomInfo sms_olympgldRomDesc[] = {
	{ "mpr-14754-f.ic1",	0x40000, 0x6a5a1e39, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_olympgld)
STD_ROM_FN(sms_olympgld)

struct BurnDriver BurnDrvsms_olympgld = {
	"sms_olympgld", NULL, NULL, NULL, "1992",
	"Olympic Gold (Euro)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_olympgldRomInfo, sms_olympgldRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Olympic Gold (Kor)

static struct BurnRomInfo sms_olympgldkRomDesc[] = {
	{ "olympic gold (korea) (en,fr,de,es,it,nl,pt,sv).bin",	0x40000, 0xd1a7b1aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_olympgldk)
STD_ROM_FN(sms_olympgldk)

struct BurnDriver BurnDrvsms_olympgldk = {
	"sms_olympgldk", "sms_olympgld", NULL, NULL, "1992",
	"Olympic Gold (Kor)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_olympgldkRomInfo, sms_olympgldkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Opa Opa (Jpn)

static struct BurnRomInfo sms_opaopaRomDesc[] = {
	{ "opa opa (japan).bin",	0x20000, 0xbea27d5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_opaopa)
STD_ROM_FN(sms_opaopa)

struct BurnDriver BurnDrvsms_opaopa = {
	"sms_opaopa", "sms_fantzonm", NULL, NULL, "1987",
	"Opa Opa (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_opaopaRomInfo, sms_opaopaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Operation Wolf (Euro, Bra)

static struct BurnRomInfo sms_opwolfRomDesc[] = {
	{ "operation wolf (europe).bin",	0x40000, 0x205caae8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_opwolf)
STD_ROM_FN(sms_opwolf)

struct BurnDriver BurnDrvsms_opwolf = {
	"sms_opwolf", NULL, NULL, NULL, "1990",
	"Operation Wolf (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_opwolfRomInfo, sms_opwolfRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Ottifants (Euro, Bra)

static struct BurnRomInfo sms_ottifantRomDesc[] = {
	{ "ottifants, the (europe) (en,fr,de,es,it).bin",	0x40000, 0x82ef2a7d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ottifant)
STD_ROM_FN(sms_ottifant)

struct BurnDriver BurnDrvsms_ottifant = {
	"sms_ottifant", NULL, NULL, NULL, "1993",
	"The Ottifants (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ottifantRomInfo, sms_ottifantRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Out Run (World)

static struct BurnRomInfo sms_outrunRomDesc[] = {
	{ "mpr-11078 w04.ic1",	0x40000, 0x5589d8d2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_outrun)
STD_ROM_FN(sms_outrun)

struct BurnDriver BurnDrvsms_outrun = {
	"sms_outrun", NULL, NULL, NULL, "1987",
	"Out Run (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_outrunRomInfo, sms_outrunRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Out Run 3-D (Euro, Bra)

static struct BurnRomInfo sms_outrun3dRomDesc[] = {
	{ "mpr-12130.ic1",	0x40000, 0xd6f43dda, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_outrun3d)
STD_ROM_FN(sms_outrun3d)

struct BurnDriver BurnDrvsms_outrun3d = {
	"sms_outrun3d", NULL, NULL, NULL, "1991",
	"Out Run 3-D (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_outrun3dRomInfo, sms_outrun3dRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Out Run Europa (Euro, Bra)

static struct BurnRomInfo sms_outrneurRomDesc[] = {
	{ "out run europa (europe).bin",	0x40000, 0x3932adbc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_outrneur)
STD_ROM_FN(sms_outrneur)

struct BurnDriver BurnDrvsms_outrneur = {
	"sms_outrneur", NULL, NULL, NULL, "1991",
	"Out Run Europa (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_outrneurRomInfo, sms_outrneurRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pac-Mania (Euro)

static struct BurnRomInfo sms_pacmaniaRomDesc[] = {
	{ "pac-mania (europe).bin",	0x20000, 0xbe57a9a5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pacmania)
STD_ROM_FN(sms_pacmania)

struct BurnDriver BurnDrvsms_pacmania = {
	"sms_pacmania", NULL, NULL, NULL, "1991",
	"Pac-Mania (Euro)\0", NULL, "Tengen", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pacmaniaRomInfo, sms_pacmaniaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Paperboy (Euro, v0)

static struct BurnRomInfo sms_paperboyRomDesc[] = {
	{ "paperboy (europe).bin",	0x20000, 0x294e0759, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_paperboy)
STD_ROM_FN(sms_paperboy)

struct BurnDriver BurnDrvsms_paperboy = {
	"sms_paperboy", NULL, NULL, NULL, "1990",
	"Paperboy (Euro, v0)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_paperboyRomInfo, sms_paperboyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Paperboy (USA, Bra, v1)

static struct BurnRomInfo sms_paperboyuRomDesc[] = {
	{ "mpr-13306a-m.ic1",	0x20000, 0x327a0b4c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_paperboyu)
STD_ROM_FN(sms_paperboyu)

struct BurnDriver BurnDrvsms_paperboyu = {
	"sms_paperboyu", "sms_paperboy", NULL, NULL, "1990",
	"Paperboy (USA, Bra, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_paperboyuRomInfo, sms_paperboyuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Parlour Games (Euro, USA)

static struct BurnRomInfo sms_parlourRomDesc[] = {
	{ "mpr-11817-s.ic2",	0x20000, 0xe030e66c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_parlour)
STD_ROM_FN(sms_parlour)

struct BurnDriver BurnDrvsms_parlour = {
	"sms_parlour", NULL, NULL, NULL, "1987",
	"Parlour Games (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_parlourRomInfo, sms_parlourRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pat Riley Basketball (USA, Prototype)

static struct BurnRomInfo sms_patrileyRomDesc[] = {
	{ "pat riley basketball (usa) (proto).bin",	0x40000, 0x9aefe664, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_patriley)
STD_ROM_FN(sms_patriley)

struct BurnDriver BurnDrvsms_patriley = {
	"sms_patriley", NULL, NULL, NULL, "19??",
	"Pat Riley Basketball (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_patrileyRomInfo, sms_patrileyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Penguin Adventure (Kor)

static struct BurnRomInfo sms_pengadvRomDesc[] = {
	{ "penguin adventure (kr).bin",	0x20000, 0x445525e2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pengadv)
STD_ROM_FN(sms_pengadv)

struct BurnDriver BurnDrvsms_pengadv = {
	"sms_pengadv", NULL, NULL, NULL, "199?",
	"Penguin Adventure (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_pengadvRomInfo, sms_pengadvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Penguin Land (Euro, USA)

static struct BurnRomInfo sms_penglandRomDesc[] = {
	{ "mpr-11190.ic1",	0x20000, 0xf97e9875, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pengland)
STD_ROM_FN(sms_pengland)

struct BurnDriver BurnDrvsms_pengland = {
	"sms_pengland", NULL, NULL, NULL, "1987",
	"Penguin Land (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_penglandRomInfo, sms_penglandRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// PGA Tour Golf (Euro)

static struct BurnRomInfo sms_pgatourRomDesc[] = {
	{ "pga tour golf (europe).bin",	0x40000, 0x95b9ea95, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pgatour)
STD_ROM_FN(sms_pgatour)

struct BurnDriver BurnDrvsms_pgatour = {
	"sms_pgatour", NULL, NULL, NULL, "1991",
	"PGA Tour Golf (Euro)\0", NULL, "Tengen", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_pgatourRomInfo, sms_pgatourRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// PitFighter - The Ultimate Challenge (Euro)

static struct BurnRomInfo sms_pitfightRomDesc[] = {
	{ "pit fighter (europe).bin",	0x80000, 0xb840a446, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pitfight)
STD_ROM_FN(sms_pitfight)

struct BurnDriver BurnDrvsms_pitfight = {
	"sms_pitfight", NULL, NULL, NULL, "1991",
	"PitFighter - The Ultimate Challenge (Euro)\0", NULL, "Domark", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_pitfightRomInfo, sms_pitfightRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// PitFighter - The Ultimate Challenge (Bra)

static struct BurnRomInfo sms_pitfightbRomDesc[] = {
	{ "pit fighter (brazil).bin",	0x80000, 0xaa4d4b5a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pitfightb)
STD_ROM_FN(sms_pitfightb)

struct BurnDriver BurnDrvsms_pitfightb = {
	"sms_pitfightb", "sms_pitfight", NULL, NULL, "1991",
	"PitFighter - The Ultimate Challenge (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pitfightbRomInfo, sms_pitfightbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pooyan (Kor)

static struct BurnRomInfo sms_pooyanRomDesc[] = {
	{ "pooyan (kr).sms",	0x08000, 0xca082218, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pooyan)
STD_ROM_FN(sms_pooyan)

struct BurnDriver BurnDrvsms_pooyan = {
	"sms_pooyan", NULL, NULL, NULL, "19??",
	"Pooyan (Kor)\0", NULL, "HiCom", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_NONE, GBF_MISC, 0,
	SMSGetZipName, sms_pooyanRomInfo, sms_pooyanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Populous (Euro, Bra)

static struct BurnRomInfo sms_populousRomDesc[] = {
	{ "populous (europe).bin",	0x40000, 0xc7a1fdef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_populous)
STD_ROM_FN(sms_populous)

struct BurnDriver BurnDrvsms_populous = {
	"sms_populous", NULL, NULL, NULL, "1989",
	"Populous (Euro, Bra)\0", NULL, "TecMagik", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_populousRomInfo, sms_populousRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Poseidon Wars 3-D (Euro, USA, Bra)

static struct BurnRomInfo sms_poseidonRomDesc[] = {
	{ "mpr-12122f.ic1",	0x40000, 0xabd48ad2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_poseidon)
STD_ROM_FN(sms_poseidon)

struct BurnDriver BurnDrvsms_poseidon = {
	"sms_poseidon", NULL, NULL, NULL, "1988",
	"Poseidon Wars 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_poseidonRomInfo, sms_poseidonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Power Strike (Euro, Bra, Kor)

static struct BurnRomInfo sms_pstrikeRomDesc[] = {
	{ "mpr-11685 w48.ic1",	0x20000, 0x4077efd9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstrike)
STD_ROM_FN(sms_pstrike)

struct BurnDriver BurnDrvsms_pstrike = {
	"sms_pstrike", NULL, NULL, NULL, "1988",
	"Power Strike (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstrikeRomInfo, sms_pstrikeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Power Strike II (Euro, Bra)

static struct BurnRomInfo sms_pstrike2RomDesc[] = {
	{ "mpr-15671.ic1",	0x80000, 0xa109a6fe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstrike2)
STD_ROM_FN(sms_pstrike2)

struct BurnDriver BurnDrvsms_pstrike2 = {
	"sms_pstrike2", NULL, NULL, NULL, "1993",
	"Power Strike II (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstrike2RomInfo, sms_pstrike2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Prince of Persia (Euro, Bra)

static struct BurnRomInfo sms_ppersiaRomDesc[] = {
	{ "prince of persia (europe).bin",	0x40000, 0x7704287d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ppersia)
STD_ROM_FN(sms_ppersia)

struct BurnDriver BurnDrvsms_ppersia = {
	"sms_ppersia", NULL, NULL, NULL, "1992",
	"Prince of Persia (Euro, Bra)\0", NULL, "Domark", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ppersiaRomInfo, sms_ppersiaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Predator 2 (Euro)

static struct BurnRomInfo sms_predatr2RomDesc[] = {
	{ "predator 2 (europe).bin",	0x40000, 0x0047b615, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_predatr2)
STD_ROM_FN(sms_predatr2)

struct BurnDriver BurnDrvsms_predatr2 = {
	"sms_predatr2", NULL, NULL, NULL, "1992",
	"Predator 2 (Euro)\0", NULL, "Arena", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_predatr2RomInfo, sms_predatr2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Predator 2 (Bra)

static struct BurnRomInfo sms_predatr2bRomDesc[] = {
	{ "predator 2 (br).bin",	0x40000, 0x2b54c82b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_predatr2b)
STD_ROM_FN(sms_predatr2b)

struct BurnDriver BurnDrvsms_predatr2b = {
	"sms_predatr2b", "sms_predatr2", NULL, NULL, "1992?",
	"Predator 2 (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_predatr2bRomInfo, sms_predatr2bRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pro Wrestling (Euro, USA, Bra)

static struct BurnRomInfo sms_prowresRomDesc[] = {
	{ "mpr-10154f.ic1",	0x20000, 0xfbde42d3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_prowres)
STD_ROM_FN(sms_prowres)

struct BurnDriver BurnDrvsms_prowres = {
	"sms_prowres", NULL, NULL, NULL, "1986",
	"Pro Wrestling (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_prowresRomInfo, sms_prowresRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Pro Yakyuu Pennant Race (Jpn)

static struct BurnRomInfo sms_proyakyuRomDesc[] = {
	{ "mpr-10749.ic1",	0x20000, 0xda9be8f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_proyakyu)
STD_ROM_FN(sms_proyakyu)

struct BurnDriver BurnDrvsms_proyakyu = {
	"sms_proyakyu", NULL, NULL, NULL, "1987",
	"The Pro Yakyuu Pennant Race (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_proyakyuRomInfo, sms_proyakyuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star (Euro, USA, v3)

static struct BurnRomInfo sms_pstarRomDesc[] = {
	{ "mpr-11711at.ic2",	0x80000, 0x00bef1d7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstar)
STD_ROM_FN(sms_pstar)

struct BurnDriver BurnDrvsms_pstar = {
	"sms_pstar", NULL, NULL, NULL, "1987",
	"Phantasy Star (Euro, USA, v3)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstarRomInfo, sms_pstarRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star (Euro, USA, v2)

static struct BurnRomInfo sms_pstar1RomDesc[] = {
	{ "phantasy star (usa, europe) (v1.2).bin",	0x80000, 0xe4a65e79, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstar1)
STD_ROM_FN(sms_pstar1)

struct BurnDriver BurnDrvsms_pstar1 = {
	"sms_pstar1", "sms_pstar", NULL, NULL, "1987",
	"Phantasy Star (Euro, USA, v2)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstar1RomInfo, sms_pstar1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star (Bra)

static struct BurnRomInfo sms_pstarbRomDesc[] = {
	{ "phantasy star (brazil).bin",	0x80000, 0x75971bef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstarb)
STD_ROM_FN(sms_pstarb)

struct BurnDriver BurnDrvsms_pstarb = {
	"sms_pstarb", "sms_pstar", NULL, NULL, "1987",
	"Phantasy Star (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstarbRomInfo, sms_pstarbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star (Jpn)

static struct BurnRomInfo sms_pstarjRomDesc[] = {
	{ "mpr-11198.ic1",	0x80000, 0x6605d36a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstarj)
STD_ROM_FN(sms_pstarj)

struct BurnDriver BurnDrvsms_pstarj = {
	"sms_pstarj", "sms_pstar", NULL, NULL, "1987",
	"Phantasy Star (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstarjRomInfo, sms_pstarjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star (Jpn, MD

static struct BurnRomInfo sms_pstarjmdRomDesc[] = {
	{ "phantasy star (j) (from saturn collection cd) [!].bin",	0x80000, 0x07301f83, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstarjmd)
STD_ROM_FN(sms_pstarjmd)

struct BurnDriver BurnDrvsms_pstarjmd = {
	"sms_pstarjmd", "sms_pstar", NULL, NULL, "1994",
	"Phantasy Star (Jpn, MD\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstarjmdRomInfo, sms_pstarjmdRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star (Kor)

static struct BurnRomInfo sms_pstarkRomDesc[] = {
	{ "phantasy star (korea).bin",	0x80000, 0x747e83b5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pstark)
STD_ROM_FN(sms_pstark)

struct BurnDriver BurnDrvsms_pstark = {
	"sms_pstark", "sms_pstar", NULL, NULL, "1987",
	"Phantasy Star (Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pstarkRomInfo, sms_pstarkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Psychic World (Euro, Bra)

static struct BurnRomInfo sms_psychicwRomDesc[] = {
	{ "psychic world (europe).bin",	0x40000, 0x5c0b1f0f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_psychicw)
STD_ROM_FN(sms_psychicw)

struct BurnDriver BurnDrvsms_psychicw = {
	"sms_psychicw", NULL, NULL, NULL, "1991",
	"Psychic World (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_psychicwRomInfo, sms_psychicwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Psycho Fox (Euro, USA, Bra)

static struct BurnRomInfo sms_psychofRomDesc[] = {
	{ "psycho fox (usa, europe).bin",	0x40000, 0x97993479, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_psychof)
STD_ROM_FN(sms_psychof)

struct BurnDriver BurnDrvsms_psychof = {
	"sms_psychof", NULL, NULL, NULL, "1989",
	"Psycho Fox (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_psychofRomInfo, sms_psychofRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Putt and Putter (Euro, Bra)

static struct BurnRomInfo sms_puttputtRomDesc[] = {
	{ "putt and putter (europe).bin",	0x20000, 0x357d4f78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_puttputt)
STD_ROM_FN(sms_puttputt)

struct BurnDriver BurnDrvsms_puttputt = {
	"sms_puttputt", NULL, NULL, NULL, "1992",
	"Putt and Putter (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_puttputtRomInfo, sms_puttputtRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Putt and Putter (Euro, Prototype)

static struct BurnRomInfo sms_puttputtpRomDesc[] = {
	{ "putt and putter (europe) (beta).bin",	0x20000, 0x8167ccc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_puttputtp)
STD_ROM_FN(sms_puttputtp)

struct BurnDriver BurnDrvsms_puttputtp = {
	"sms_puttputtp", "sms_puttputt", NULL, NULL, "1992",
	"Putt and Putter (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_puttputtpRomInfo, sms_puttputtpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Puznic (Kor)

static struct BurnRomInfo sms_puznicRomDesc[] = {
	{ "puznic (kr).sms",	0x08000, 0x76e8265f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_puznic)
STD_ROM_FN(sms_puznic)

struct BurnDriver BurnDrvsms_puznic = {
	"sms_puznic", NULL, NULL, NULL, "1990",
	"Puznic (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_puznicRomInfo, sms_puznicRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Quartet (Euro, USA)

static struct BurnRomInfo sms_quartetRomDesc[] = {
	{ "mpr-10418 w15.ic1",	0x20000, 0xe0f34fa6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_quartet)
STD_ROM_FN(sms_quartet)

struct BurnDriver BurnDrvsms_quartet = {
	"sms_quartet", NULL, NULL, NULL, "1987",
	"Quartet (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_quartetRomInfo, sms_quartetRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Quest for the Shaven Yak Starring Ren Hoëk and Stimpy (Bra)

static struct BurnRomInfo sms_shavnyakRomDesc[] = {
	{ "quest for the shaven yak starring ren hoek and stimpy (brazil).bin",	0x80000, 0xf42e145c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_shavnyak)
STD_ROM_FN(sms_shavnyak)

struct BurnDriver BurnDrvsms_shavnyak = {
	"sms_shavnyak", NULL, NULL, NULL, "1993",
	"Quest for the Shaven Yak Starring Ren Hoek and Stimpy (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_shavnyakRomInfo, sms_shavnyakRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rainbow Islands - The Story of Bubble Bobble 2 (Euro)

static struct BurnRomInfo sms_rbislandRomDesc[] = {
	{ "rainbow islands - the story of bubble bobble 2 (europe).bin",	0x40000, 0xc172a22c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rbisland)
STD_ROM_FN(sms_rbisland)

struct BurnDriver BurnDrvsms_rbisland = {
	"sms_rbisland", NULL, NULL, NULL, "1993",
	"Rainbow Islands - The Story of Bubble Bobble 2 (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rbislandRomInfo, sms_rbislandRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rainbow Islands - The Story of Bubble Bobble 2 (Bra)

static struct BurnRomInfo sms_rbislandbRomDesc[] = {
	{ "rainbow islands - the story of bubble bobble 2 (brazil).bin",	0x40000, 0x00ec173a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rbislandb)
STD_ROM_FN(sms_rbislandb)

struct BurnDriver BurnDrvsms_rbislandb = {
	"sms_rbislandb", "sms_rbisland", NULL, NULL, "1993",
	"Rainbow Islands - The Story of Bubble Bobble 2 (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rbislandbRomInfo, sms_rbislandbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rambo - First Blood Part II (USA)

static struct BurnRomInfo sms_rambo2RomDesc[] = {
	{ "mpr-10407 w10.ic1",	0x20000, 0xbbda65f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rambo2)
STD_ROM_FN(sms_rambo2)

struct BurnDriver BurnDrvsms_rambo2 = {
	"sms_rambo2", "sms_secret", NULL, NULL, "1986",
	"Rambo - First Blood Part II (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rambo2RomInfo, sms_rambo2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rambo III (Euro, USA, Bra)

static struct BurnRomInfo sms_rambo3RomDesc[] = {
	{ "mpr-12036f.ic1",	0x40000, 0xda5a7013, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rambo3)
STD_ROM_FN(sms_rambo3)

struct BurnDriver BurnDrvsms_rambo3 = {
	"sms_rambo3", NULL, NULL, NULL, "1988",
	"Rambo III (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rambo3RomInfo, sms_rambo3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rampage (Euro, USA, Bra)

static struct BurnRomInfo sms_rampageRomDesc[] = {
	{ "mpr-12042f.ic1",	0x40000, 0x42fc47ee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rampage)
STD_ROM_FN(sms_rampage)

struct BurnDriver BurnDrvsms_rampage = {
	"sms_rampage", NULL, NULL, NULL, "1988",
	"Rampage (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rampageRomInfo, sms_rampageRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rampart (Euro)

static struct BurnRomInfo sms_rampartRomDesc[] = {
	{ "rampart (europe).bin",	0x40000, 0x426e5c8a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rampart)
STD_ROM_FN(sms_rampart)

struct BurnDriver BurnDrvsms_rampart = {
	"sms_rampart", NULL, NULL, NULL, "1991",
	"Rampart (Euro)\0", NULL, "Tengen", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rampartRomInfo, sms_rampartRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rastan (Euro, USA, Bra)

static struct BurnRomInfo sms_rastanRomDesc[] = {
	{ "rastan (usa, europe).bin",	0x40000, 0xc547eb1b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rastan)
STD_ROM_FN(sms_rastan)

struct BurnDriver BurnDrvsms_rastan = {
	"sms_rastan", NULL, NULL, NULL, "1988",
	"Rastan (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rastanRomInfo, sms_rastanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// R.C. Grand Prix (Euro, USA, Bra)

static struct BurnRomInfo sms_rcgpRomDesc[] = {
	{ "mpr-12903 t24.ic1",	0x40000, 0x54316fea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rcgp)
STD_ROM_FN(sms_rcgp)

struct BurnDriver BurnDrvsms_rcgp = {
	"sms_rcgp", NULL, NULL, NULL, "1989",
	"R.C. Grand Prix (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rcgpRomInfo, sms_rcgpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// R.C. Grand Prix (Prototype)

static struct BurnRomInfo sms_rcgppRomDesc[] = {
	{ "r.c. grand prix [proto].bin",	0x40000, 0x767f11b8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rcgpp)
STD_ROM_FN(sms_rcgpp)

struct BurnDriver BurnDrvsms_rcgpp = {
	"sms_rcgpp", "sms_rcgp", NULL, NULL, "1989",
	"R.C. Grand Prix (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rcgppRomInfo, sms_rcgppRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Reggie Jackson Baseball (USA)

static struct BurnRomInfo sms_regjacksRomDesc[] = {
	{ "reggie jackson baseball (usa).bin",	0x40000, 0x6d94bb0e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_regjacks)
STD_ROM_FN(sms_regjacks)

struct BurnDriver BurnDrvsms_regjacks = {
	"sms_regjacks", "sms_ameribb", NULL, NULL, "1988",
	"Reggie Jackson Baseball (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_regjacksRomInfo, sms_regjacksRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Renegade (Euro, Bra)

static struct BurnRomInfo sms_renegadeRomDesc[] = {
	{ "renegade (europe).bin",	0x40000, 0x3be7f641, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_renegade)
STD_ROM_FN(sms_renegade)

struct BurnDriver BurnDrvsms_renegade = {
	"sms_renegade", NULL, NULL, NULL, "1993",
	"Renegade (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_renegadeRomInfo, sms_renegadeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rescue Mission (Euro, USA, Bra)

static struct BurnRomInfo sms_rescuemsRomDesc[] = {
	{ "mpr-11400.ic1",	0x20000, 0x79ac8e7f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rescuems)
STD_ROM_FN(sms_rescuems)

struct BurnDriver BurnDrvsms_rescuems = {
	"sms_rescuems", NULL, NULL, NULL, "1988",
	"Rescue Mission (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rescuemsRomInfo, sms_rescuemsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Road Fighter (Kor)

static struct BurnRomInfo sms_roadfghtRomDesc[] = {
	{ "road fighter (kr).bin",	0x08000, 0x8034bd27, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_roadfght)
STD_ROM_FN(sms_roadfght)

struct BurnDriver BurnDrvsms_roadfght = {
	"sms_roadfght", NULL, NULL, NULL, "19??",
	"Road Fighter (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_roadfghtRomInfo, sms_roadfghtRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Road Rash (Euro, Bra)

static struct BurnRomInfo sms_roadrashRomDesc[] = {
	{ "road rash (europe).bin",	0x80000, 0xb876fc74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_roadrash)
STD_ROM_FN(sms_roadrash)

struct BurnDriver BurnDrvsms_roadrash = {
	"sms_roadrash", NULL, NULL, NULL, "1993",
	"Road Rash (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_roadrashRomInfo, sms_roadrashRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James Pond 2 - Codename RoboCod (Euro, Bra)

static struct BurnRomInfo sms_robocodRomDesc[] = {
	{ "james pond 2 - codename robocod (europe).bin",	0x80000, 0x102d5fea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_robocod)
STD_ROM_FN(sms_robocod)

struct BurnDriver BurnDrvsms_robocod = {
	"sms_robocod", NULL, NULL, NULL, "1993",
	"James Pond 2 - Codename RoboCod (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_robocodRomInfo, sms_robocodRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// RoboCop 3 (Euro, Bra)

static struct BurnRomInfo sms_robocop3RomDesc[] = {
	{ "robocop 3 (europe).bin",	0x40000, 0x9f951756, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_robocop3)
STD_ROM_FN(sms_robocop3)

struct BurnDriver BurnDrvsms_robocop3 = {
	"sms_robocop3", NULL, NULL, NULL, "1993",
	"RoboCop 3 (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_robocop3RomInfo, sms_robocop3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// RoboCop versus The Terminator (Euro, Bra)

static struct BurnRomInfo sms_robotermRomDesc[] = {
	{ "robocop versus the terminator (europe).bin",	0x80000, 0x8212b754, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_roboterm)
STD_ROM_FN(sms_roboterm)

struct BurnDriver BurnDrvsms_roboterm = {
	"sms_roboterm", NULL, NULL, NULL, "1993",
	"RoboCop versus The Terminator (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_robotermRomInfo, sms_robotermRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rocky (World)

static struct BurnRomInfo sms_rockyRomDesc[] = {
	{ "mpr-11072 w01.ic1",	0x40000, 0x1bcc7be3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rocky)
STD_ROM_FN(sms_rocky)

struct BurnDriver BurnDrvsms_rocky = {
	"sms_rocky", NULL, NULL, NULL, "1987",
	"Rocky (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rockyRomInfo, sms_rockyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// R-Type (World)

static struct BurnRomInfo sms_rtypeRomDesc[] = {
	{ "mpr-12002 t13.ic2",	0x80000, 0xbb54b6b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rtype)
STD_ROM_FN(sms_rtype)

struct BurnDriver BurnDrvsms_rtype = {
	"sms_rtype", NULL, NULL, NULL, "1988",
	"R-Type (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rtypeRomInfo, sms_rtypeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// R-Type (Prototype)

static struct BurnRomInfo sms_rtypepRomDesc[] = {
	{ "r-type (proto).bin",	0x80000, 0x0d0840d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_rtypep)
STD_ROM_FN(sms_rtypep)

struct BurnDriver BurnDrvsms_rtypep = {
	"sms_rtypep", "sms_rtype", NULL, NULL, "1988",
	"R-Type (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_rtypepRomInfo, sms_rtypepRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Running Battle (Euro, Bra)

static struct BurnRomInfo sms_runningRomDesc[] = {
	{ "mpr-14252-f.ic1",	0x40000, 0x1fdae719, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_running)
STD_ROM_FN(sms_running)

struct BurnDriver BurnDrvsms_running = {
	"sms_running", NULL, NULL, NULL, "1991",
	"Running Battle (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_runningRomInfo, sms_runningRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sagaia (Euro, Bra)

static struct BurnRomInfo sms_sagaiaRomDesc[] = {
	{ "sagaia (europe).bin",	0x40000, 0x66388128, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sagaia)
STD_ROM_FN(sms_sagaia)

struct BurnDriver BurnDrvsms_sagaia = {
	"sms_sagaia", NULL, NULL, NULL, "1992",
	"Sagaia (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sagaiaRomInfo, sms_sagaiaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sangokushi 3 (Kor)

static struct BurnRomInfo sms_sangoku3RomDesc[] = {
	{ "sangokushi 3 (korea) (unl).bin",	0x100000, 0x97d03541, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sangoku3)
STD_ROM_FN(sms_sangoku3)

struct BurnDriver BurnDrvsms_sangoku3 = {
	"sms_sangoku3", NULL, NULL, NULL, "1994",
	"Sangokushi 3 (Kor)\0", NULL, "Game Line", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_KOREA, GBF_MISC, 0,
	SMSGetZipName, sms_sangoku3RomInfo, sms_sangoku3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sapo Xulé - O Mestre do Kung Fu (Bra)

static struct BurnRomInfo sms_sapomestrRomDesc[] = {
	{ "sapo xule - o mestre do kung fu (brazil).bin",	0x40000, 0x890e83e4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sapomestr)
STD_ROM_FN(sms_sapomestr)

struct BurnDriver BurnDrvsms_sapomestr = {
	"sms_sapomestr", "sms_kungfuk", NULL, NULL, "1995",
	"Sapo Xule - O Mestre do Kung Fu (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sapomestrRomInfo, sms_sapomestrRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// S.O.S Lagoa Poluida (Bra)

static struct BurnRomInfo sms_sapososRomDesc[] = {
	{ "sapo xule - s.o.s lagoa poluida (brazil).bin",	0x20000, 0x7ab2946a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_saposos)
STD_ROM_FN(sms_saposos)

struct BurnDriver BurnDrvsms_saposos = {
	"sms_saposos", "sms_astrow", NULL, NULL, "1995",
	"S.O.S Lagoa Poluida (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sapososRomInfo, sms_sapososRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sapo Xulé vs. Os Invasores do Brejo (Bra)

static struct BurnRomInfo sms_sapoxuleRomDesc[] = {
	{ "sapo xule vs. os invasores do brejo (brazil).bin",	0x40000, 0x9a608327, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sapoxule)
STD_ROM_FN(sms_sapoxule)

struct BurnDriver BurnDrvsms_sapoxule = {
	"sms_sapoxule", "sms_psychof", NULL, NULL, "1995",
	"Sapo Xule vs. Os Invasores do Brejo (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sapoxuleRomInfo, sms_sapoxuleRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Satellite 7 (Jpn, Pirate?)

static struct BurnRomInfo sms_satell7aRomDesc[] = {
	{ "satellite 7 (j) [hack].bin",	0x08000, 0x87b9ecb8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_satell7a)
STD_ROM_FN(sms_satell7a)

struct BurnDriver BurnDrvsms_satell7a = {
	"sms_satell7a", "sms_satell7", NULL, NULL, "1985",
	"Satellite 7 (Jpn, Pirate?)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_satell7aRomInfo, sms_satell7aRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Smurfs 2 (Euro)

static struct BurnRomInfo sms_smurfs2RomDesc[] = {
	{ "schtroumpfs autour du monde, les (europe) (en,fr,de,es).bin",	0x40000, 0x97e5bb7d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smurfs2)
STD_ROM_FN(sms_smurfs2)

struct BurnDriver BurnDrvsms_smurfs2 = {
	"sms_smurfs2", NULL, NULL, NULL, "1996",
	"The Smurfs 2 (Euro)\0", NULL, "Infogrames", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smurfs2RomInfo, sms_smurfs2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Smurfs 2 (Euro, Prototype)

static struct BurnRomInfo sms_smurfs2pRomDesc[] = {
	{ "schtroumpfs autour du monde, les (europe) (en,fr,de,es) (beta).bin",	0x40000, 0x7982ae67, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smurfs2p)
STD_ROM_FN(sms_smurfs2p)

struct BurnDriver BurnDrvsms_smurfs2p = {
	"sms_smurfs2p", "sms_smurfs2", NULL, NULL, "1996",
	"The Smurfs 2 (Euro, Prototype)\0", NULL, "Infogrames", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smurfs2pRomInfo, sms_smurfs2pRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Special Criminal Investigation (Euro)

static struct BurnRomInfo sms_sciRomDesc[] = {
	{ "special criminal investigation (europe).bin",	0x40000, 0xfa8e4ca0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sci)
STD_ROM_FN(sms_sci)

struct BurnDriver BurnDrvsms_sci = {
	"sms_sci", NULL, NULL, NULL, "1992",
	"Special Criminal Investigation (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sciRomInfo, sms_sciRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Special Criminal Investigation (Euro, Prototype)

static struct BurnRomInfo sms_scipRomDesc[] = {
	{ "special criminal investigation (europe) (beta).bin",	0x40000, 0x1b7d2a20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_scip)
STD_ROM_FN(sms_scip)

struct BurnDriver BurnDrvsms_scip = {
	"sms_scip", "sms_sci", NULL, NULL, "1992",
	"Special Criminal Investigation (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_scipRomInfo, sms_scipRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Scramble Spirits (Euro, Bra)

static struct BurnRomInfo sms_sspiritsRomDesc[] = {
	{ "scramble spirits (europe).bin",	0x40000, 0x9a8b28ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sspirits)
STD_ROM_FN(sms_sspirits)

struct BurnDriver BurnDrvsms_sspirits = {
	"sms_sspirits", NULL, NULL, NULL, "1989",
	"Scramble Spirits (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sspiritsRomInfo, sms_sspiritsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// SDI (Jpn)

static struct BurnRomInfo sms_sdiRomDesc[] = {
	{ "mpr-11191.ic1",	0x20000, 0x1de2c2d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sdi)
STD_ROM_FN(sms_sdi)

struct BurnDriver BurnDrvsms_sdi = {
	"sms_sdi", "sms_globald", NULL, NULL, "1987",
	"SDI (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sdiRomInfo, sms_sdiRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Secret Command (Euro)

static struct BurnRomInfo sms_secretRomDesc[] = {
	{ "secret command (europe).bin",	0x20000, 0x00529114, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_secret)
STD_ROM_FN(sms_secret)

struct BurnDriver BurnDrvsms_secret = {
	"sms_secret", NULL, NULL, NULL, "1986",
	"Secret Command (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_secretRomInfo, sms_secretRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sega Chess (Euro, Bra)

static struct BurnRomInfo sms_segachssRomDesc[] = {
	{ "sega chess (europe).bin",	0x40000, 0xa8061aef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_segachss)
STD_ROM_FN(sms_segachss)

struct BurnDriver BurnDrvsms_segachss = {
	"sms_segachss", NULL, NULL, NULL, "1991",
	"Sega Chess (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_segachssRomInfo, sms_segachssRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sega Graphic Board (Jpn, Prototype v2.0)

static struct BurnRomInfo sms_segagfxRomDesc[] = {
	{ "graphic board v2.0.bin",	0x08000, 0x276aa542, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_segagfx)
STD_ROM_FN(sms_segagfx)

struct BurnDriver BurnDrvsms_segagfx = {
	"sms_segagfx", NULL, NULL, NULL, "1987",
	"Sega Graphic Board (Jpn, Prototype v2.0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_segagfxRomInfo, sms_segagfxRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sega World Tournament Golf (Euro, Bra, Kor)

static struct BurnRomInfo sms_segawtgRomDesc[] = {
	{ "sega world tournament golf (europe).bin",	0x40000, 0x296879dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_segawtg)
STD_ROM_FN(sms_segawtg)

struct BurnDriver BurnDrvsms_segawtg = {
	"sms_segawtg", NULL, NULL, NULL, "1993",
	"Sega World Tournament Golf (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_segawtgRomInfo, sms_segawtgRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Seishun Scandal (Jpn, Pirate?)

static struct BurnRomInfo sms_seishun1RomDesc[] = {
	{ "seishyun scandal (j) [hack].bin",	0x08000, 0xbcd91d78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_seishun1)
STD_ROM_FN(sms_seishun1)

struct BurnDriver BurnDrvsms_seishun1 = {
	"sms_seishun1", "sms_myhero", NULL, NULL, "1986",
	"Seishun Scandal (Jpn, Pirate?)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_seishun1RomInfo, sms_seishun1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sensible Soccer (Euro)

static struct BurnRomInfo sms_sensibleRomDesc[] = {
	{ "sensible soccer (europe).bin",	0x20000, 0xf8176918, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sensible)
STD_ROM_FN(sms_sensible)

struct BurnDriver BurnDrvsms_sensible = {
	"sms_sensible", NULL, NULL, NULL, "1993",
	"Sensible Soccer (Euro)\0", NULL, "Sony Imagesoft", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sensibleRomInfo, sms_sensibleRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sky Jaguar (Kor)

static struct BurnRomInfo sms_skyjagRomDesc[] = {
	{ "sky jaguar (kr).bin",	0x08000, 0x5b8e65e4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_skyjag)
STD_ROM_FN(sms_skyjag)

struct BurnDriver BurnDrvsms_skyjag = {
	"sms_skyjag", NULL, NULL, NULL, "199?",
	"Sky Jaguar (Kor)\0", NULL, "Samsung", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_skyjagRomInfo, sms_skyjagRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Street Fighter II (Bra)

static struct BurnRomInfo sms_sf2RomDesc[] = {
	{ "street fighter ii (brazil).bin",	0xc8000, 0x0f8287ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sf2)
STD_ROM_FN(sms_sf2)

struct BurnDriver BurnDrvsms_sf2 = {
	"sms_sf2", NULL, NULL, NULL, "1997",
	"Street Fighter II (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_sf2RomInfo, sms_sf2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shadow Dancer (Euro, Bra, Kor)

static struct BurnRomInfo sms_shdancerRomDesc[] = {
	{ "shadow dancer (europe).bin",	0x80000, 0x3793c01a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_shdancer)
STD_ROM_FN(sms_shdancer)

struct BurnDriver BurnDrvsms_shdancer = {
	"sms_shdancer", NULL, NULL, NULL, "1991",
	"Shadow Dancer (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM /*| HARDWARE_SMS_DISPLAY_PAL */, GBF_MISC, 0,
	SMSGetZipName, sms_shdancerRomInfo, sms_shdancerRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shanghai (Euro, USA)

static struct BurnRomInfo sms_shanghaiRomDesc[] = {
	{ "shanghai (usa, europe).bin",	0x20000, 0xaab67ec3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_shanghai)
STD_ROM_FN(sms_shanghai)

struct BurnDriver BurnDrvsms_shanghai = {
	"sms_shanghai", NULL, NULL, NULL, "1988",
	"Shanghai (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_shanghaiRomInfo, sms_shanghaiRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shanghai (Prototype)

static struct BurnRomInfo sms_shanghaipRomDesc[] = {
	{ "shanghai (usa, europe) (beta).bin",	0x20000, 0xd5d25156, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_shanghaip)
STD_ROM_FN(sms_shanghaip)

struct BurnDriver BurnDrvsms_shanghaip = {
	"sms_shanghaip", "sms_shanghai", NULL, NULL, "1988",
	"Shanghai (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_shanghaipRomInfo, sms_shanghaipRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Space Harrier (Euro)

static struct BurnRomInfo sms_sharrierRomDesc[] = {
	{ "mpr-11071.ic1",	0x40000, 0xca1d3752, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sharrier)
STD_ROM_FN(sms_sharrier)

struct BurnDriver BurnDrvsms_sharrier = {
	"sms_sharrier", NULL, NULL, NULL, "1986",
	"Space Harrier (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sharrierRomInfo, sms_sharrierRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Space Harrier 3-D (Euro, USA, Bra)

static struct BurnRomInfo sms_sharr3dRomDesc[] = {
	{ "mpr-11564.ic1",	0x40000, 0x6bd5c2bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sharr3d)
STD_ROM_FN(sms_sharr3d)

struct BurnDriver BurnDrvsms_sharr3d = {
	"sms_sharr3d", NULL, NULL, NULL, "1988",
	"Space Harrier 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sharr3dRomInfo, sms_sharr3dRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Space Harrier 3D (Jpn)

static struct BurnRomInfo sms_sharr3djRomDesc[] = {
	{ "space harrier 3d (japan).bin",	0x40000, 0x156948f9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sharr3dj)
STD_ROM_FN(sms_sharr3dj)

struct BurnDriver BurnDrvsms_sharr3dj = {
	"sms_sharr3dj", "sms_sharr3d", NULL, NULL, "1988",
	"Space Harrier 3D (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sharr3djRomInfo, sms_sharr3djRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Space Harrier (Jpn, USA)

static struct BurnRomInfo sms_sharrierjuRomDesc[] = {
	{ "space harrier (japan).bin",	0x40000, 0xbeddf80e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sharrierju)
STD_ROM_FN(sms_sharrierju)

struct BurnDriver BurnDrvsms_sharrierju = {
	"sms_sharrierju", "sms_sharrier", NULL, NULL, "1986",
	"Space Harrier (Jpn, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sharrierjuRomInfo, sms_sharrierjuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shinobi (Euro, USA, Bra, v1)

static struct BurnRomInfo sms_shinobiRomDesc[] = {
	{ "shinobi (usa, europe).bin",	0x40000, 0x0c6fac4e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_shinobi)
STD_ROM_FN(sms_shinobi)

struct BurnDriver BurnDrvsms_shinobi = {
	"sms_shinobi", NULL, NULL, NULL, "1988",
	"Shinobi (Euro, USA, Bra, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_shinobiRomInfo, sms_shinobiRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shinobi (Jpn, Bra, v0)

static struct BurnRomInfo sms_shinobijRomDesc[] = {
	{ "shinobi (japan).bin",	0x40000, 0xe1fff1bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_shinobij)
STD_ROM_FN(sms_shinobij)

struct BurnDriver BurnDrvsms_shinobij = {
	"sms_shinobij", "sms_shinobi", NULL, NULL, "1988",
	"Shinobi (Jpn, Bra, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_shinobijRomInfo, sms_shinobijRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shooting Gallery (Euro, USA, Bra)

static struct BurnRomInfo sms_shootingRomDesc[] = {
	{ "mpr-10515.ic1",	0x20000, 0x4b051022, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_shooting)
STD_ROM_FN(sms_shooting)

struct BurnDriver BurnDrvsms_shooting = {
	"sms_shooting", NULL, NULL, NULL, "1987",
	"Shooting Gallery (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_shootingRomInfo, sms_shootingRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sítio do Picapau Amarelo (Bra)

static struct BurnRomInfo sms_sitioRomDesc[] = {
	{ "sitio do picapau amarelo (brazil).bin",	0x100000, 0xabdf3923, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sitio)
STD_ROM_FN(sms_sitio)

struct BurnDriver BurnDrvsms_sitio = {
	"sms_sitio", NULL, NULL, NULL, "1997",
	"Sitio do Picapau Amarelo (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sitioRomInfo, sms_sitioRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Slap Shot (USA, v2)

static struct BurnRomInfo sms_slapshotRomDesc[] = {
	{ "slap shot (usa) (v1.2).bin",	0x40000, 0x702c3e98, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_slapshot)
STD_ROM_FN(sms_slapshot)

struct BurnDriver BurnDrvsms_slapshot = {
	"sms_slapshot", NULL, NULL, NULL, "1990",
	"Slap Shot (USA, v2)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_slapshotRomInfo, sms_slapshotRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Slap Shot (Euro, v1)

static struct BurnRomInfo sms_slapshotaRomDesc[] = {
	{ "slap shot (europe) (v1.1).bin",	0x40000, 0xd33b296a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_slapshota)
STD_ROM_FN(sms_slapshota)

struct BurnDriver BurnDrvsms_slapshota = {
	"sms_slapshota", "sms_slapshot", NULL, NULL, "1990",
	"Slap Shot (Euro, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_slapshotaRomInfo, sms_slapshotaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Slap Shot (Euro, v0)

static struct BurnRomInfo sms_slapshotbRomDesc[] = {
	{ "mpr-12934.ic1",	0x40000, 0xc93bd0e9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_slapshotb)
STD_ROM_FN(sms_slapshotb)

struct BurnDriver BurnDrvsms_slapshotb = {
	"sms_slapshotb", "sms_slapshot", NULL, NULL, "1989",
	"Slap Shot (Euro, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_slapshotbRomInfo, sms_slapshotbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Slap Shoot (Prototype)

static struct BurnRomInfo sms_slapshotpRomDesc[] = {
	{ "slap shot (proto).bin",	0x40000, 0xfbb0a92a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_slapshotp)
STD_ROM_FN(sms_slapshotp)

struct BurnDriver BurnDrvsms_slapshotp = {
	"sms_slapshotp", "sms_slapshot", NULL, NULL, "1990",
	"Slap Shoot (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_slapshotpRomInfo, sms_slapshotpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Monaco GP (Euro, Bra)

static struct BurnRomInfo sms_smgpRomDesc[] = {
	{ "mpr-13289.ic1",	0x40000, 0x55bf81a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smgp)
STD_ROM_FN(sms_smgp)

struct BurnDriver BurnDrvsms_smgp = {
	"sms_smgp", NULL, NULL, NULL, "1990",
	"Super Monaco GP (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smgpRomInfo, sms_smgpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Monaco GP (USA)

static struct BurnRomInfo sms_smgpuRomDesc[] = {
	{ "mpr-13295 t32.ic1",	0x40000, 0x3ef12baa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smgpu)
STD_ROM_FN(sms_smgpu)

struct BurnDriver BurnDrvsms_smgpu = {
	"sms_smgpu", "sms_smgp", NULL, NULL, "1990",
	"Super Monaco GP (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smgpuRomInfo, sms_smgpuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Monaco GP (Euro, Prototype)

static struct BurnRomInfo sms_smgpp1RomDesc[] = {
	{ "super monaco gp [proto 1].bin",	0x40000, 0xdd7adbcc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smgpp1)
STD_ROM_FN(sms_smgpp1)

struct BurnDriver BurnDrvsms_smgpp1 = {
	"sms_smgpp1", "sms_smgp", NULL, NULL, "1990",
	"Super Monaco GP (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smgpp1RomInfo, sms_smgpp1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Monaco GP (Euro, Older Prototype)

static struct BurnRomInfo sms_smgpp2RomDesc[] = {
	{ "super monaco gp (proto).bin",	0x40000, 0xa1d6d963, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smgpp2)
STD_ROM_FN(sms_smgpp2)

struct BurnDriver BurnDrvsms_smgpp2 = {
	"sms_smgpp2", "sms_smgp", NULL, NULL, "1990",
	"Super Monaco GP (Euro, Older Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smgpp2RomInfo, sms_smgpp2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Promocao Especial M. System III Compact (Bra, Sample)

static struct BurnRomInfo sms_sms3sampRomDesc[] = {
	{ "promocao especial m. system iii compact (brazil) (sample).bin",	0x08000, 0x30af0233, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sms3samp)
STD_ROM_FN(sms_sms3samp)

struct BurnDriver BurnDrvsms_sms3samp = {
	"sms_sms3samp", NULL, NULL, NULL, "19??",
	"Promocao Especial M. System III Compact (Bra, Sample)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sms3sampRomInfo, sms_sms3sampRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Smurfs (Euro, Bra)

static struct BurnRomInfo sms_smurfsRomDesc[] = {
	{ "smurfs, the (europe) (en,fr,de,es).bin",	0x40000, 0x3e63768a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smurfs)
STD_ROM_FN(sms_smurfs)

struct BurnDriver BurnDrvsms_smurfs = {
	"sms_smurfs", NULL, NULL, NULL, "1994",
	"The Smurfs (Euro, Bra)\0", NULL, "Infogrames", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_smurfsRomInfo, sms_smurfsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Solomon no Kagi - Oujo Rihita no Namida (Jpn)

static struct BurnRomInfo sms_solomonRomDesc[] = {
	{ "solomon no kagi - oujo rihita no namida (japan).bin",	0x20000, 0x11645549, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_solomon)
STD_ROM_FN(sms_solomon)

struct BurnDriver BurnDrvsms_solomon = {
	"sms_solomon", NULL, NULL, NULL, "1988",
	"Solomon no Kagi - Oujo Rihita no Namida (Jpn)\0", NULL, "Salio", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_solomonRomInfo, sms_solomonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog (Euro, USA, Bra)

static struct BurnRomInfo sms_sonicRomDesc[] = {
	{ "mpr-14271-f.ic1",	0x40000, 0xb519e833, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sonic)
STD_ROM_FN(sms_sonic)

struct BurnDriver BurnDrvsms_sonic = {
	"sms_sonic", NULL, NULL, NULL, "1991",
	"Sonic The Hedgehog (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sonicRomInfo, sms_sonicRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Sonic The Hedgehog FM MOD 1.02 (Euro, USA, Bra)

static struct BurnRomInfo sms_sonicfm102RomDesc[] = {
	{ "Sonic1SMS_FM_v102.sms",	0x40000, 0x7c077b38, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sonicfm102)
STD_ROM_FN(sms_sonicfm102)

struct BurnDriver BurnDrvsms_sonicfm102 = {
	"sms_sonicfm102", "sms_sonic", NULL, NULL, "1991",
	"Sonic The Hedgehog (FM Mod 1.02)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sonicfm102RomInfo, sms_sonicfm102RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog 2 (Euro, Bra, Kor, v1)

static struct BurnRomInfo sms_sonic2RomDesc[] = {
	{ "sonic the hedgehog 2 (europe) (v1.1).bin",	0x80000, 0xd6f2bfca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sonic2)
STD_ROM_FN(sms_sonic2)

struct BurnDriver BurnDrvsms_sonic2 = {
	"sms_sonic2", NULL, NULL, NULL, "1992",
	"Sonic The Hedgehog 2 (Euro, Bra, Kor, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sonic2RomInfo, sms_sonic2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog 2 (Euro, Bra, v0)

static struct BurnRomInfo sms_sonic2aRomDesc[] = {
	{ "mpr-15159.ic1",	0x80000, 0x5b3b922c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sonic2a)
STD_ROM_FN(sms_sonic2a)

struct BurnDriver BurnDrvsms_sonic2a = {
	"sms_sonic2a", "sms_sonic2", NULL, NULL, "1992",
	"Sonic The Hedgehog 2 (Euro, Bra, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sonic2aRomInfo, sms_sonic2aRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Bra)

static struct BurnRomInfo sms_sonicblsRomDesc[] = {
	{ "mpr-19947-s.u2",	0x100000, 0x96b3f29e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sonicbls)
STD_ROM_FN(sms_sonicbls)

struct BurnDriver BurnDrvsms_sonicbls = {
	"sms_sonicbls", NULL, NULL, NULL, "1997",
	"Sonic Blast (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sonicblsRomInfo, sms_sonicblsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog Chaos (Euro, Bra)

static struct BurnRomInfo sms_soniccRomDesc[] = {
	{ "mpr-15926-f.ic1",	0x80000, 0xaedf3bdf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sonicc)
STD_ROM_FN(sms_sonicc)

struct BurnDriver BurnDrvsms_sonicc = {
	"sms_sonicc", NULL, NULL, NULL, "1993",
	"Sonic The Hedgehog Chaos (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_soniccRomInfo, sms_soniccRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic's Edusoft (Prototype)

static struct BurnRomInfo sms_soniceduRomDesc[] = {
	{ "sonic's edusoft (unknown) (proto).bin",	0x40000, 0xd9096263, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sonicedu)
STD_ROM_FN(sms_sonicedu)

struct BurnDriver BurnDrvsms_sonicedu = {
	"sms_sonicedu", NULL, NULL, NULL, "19??",
	"Sonic's Edusoft (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_soniceduRomInfo, sms_soniceduRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog Spinball (Euro, Bra)

static struct BurnRomInfo sms_sspinRomDesc[] = {
	{ "sonic spinball (europe).bin",	0x80000, 0x11c1bc8a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sspin)
STD_ROM_FN(sms_sspin)

struct BurnDriver BurnDrvsms_sspin = {
	"sms_sspin", NULL, NULL, NULL, "1994",
	"Sonic The Hedgehog Spinball (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sspinRomInfo, sms_sspinRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Streets of Rage (Euro, Bra)

static struct BurnRomInfo sms_sorRomDesc[] = {
	{ "streets of rage (europe).bin",	0x80000, 0x4ab3790f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sor)
STD_ROM_FN(sms_sor)

struct BurnDriver BurnDrvsms_sor = {
	"sms_sor", NULL, NULL, NULL, "1993",
	"Streets of Rage (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sorRomInfo, sms_sorRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Streets of Rage II (Euro, Bra)

static struct BurnRomInfo sms_sor2RomDesc[] = {
	{ "streets of rage ii (europe).bin",	0x80000, 0x04e9c089, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sor2)
STD_ROM_FN(sms_sor2)

struct BurnDriver BurnDrvsms_sor2 = {
	"sms_sor2", NULL, NULL, NULL, "1993",
	"Streets of Rage II (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sor2RomInfo, sms_sor2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Space Gun (Euro)

static struct BurnRomInfo sms_spacegunRomDesc[] = {
	{ "mpr-15063.ic1",	0x80000, 0xa908cff5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spacegun)
STD_ROM_FN(sms_spacegun)

struct BurnDriver BurnDrvsms_spacegun = {
	"sms_spacegun", NULL, NULL, NULL, "1992",
	"Space Gun (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_spacegunRomInfo, sms_spacegunRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Speedball (Image Works) (Euro)

static struct BurnRomInfo sms_speedblRomDesc[] = {
	{ "speedball (europe) (mirrorsoft).bin",	0x20000, 0xa57cad18, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_speedbl)
STD_ROM_FN(sms_speedbl)

struct BurnDriver BurnDrvsms_speedbl = {
	"sms_speedbl", NULL, NULL, NULL, "1990",
	"Speedball (Image Works) (Euro)\0", NULL, "Image Works", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_speedblRomInfo, sms_speedblRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Speedball 2 (Euro)

static struct BurnRomInfo sms_speedbl2RomDesc[] = {
	{ "speedball 2 (europe).bin",	0x40000, 0x0c7366a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_speedbl2)
STD_ROM_FN(sms_speedbl2)

struct BurnDriver BurnDrvsms_speedbl2 = {
	"sms_speedbl2", NULL, NULL, NULL, "1992",
	"Speedball 2 (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_speedbl2RomInfo, sms_speedbl2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Speedball (Virgin) (Euro, USA)

static struct BurnRomInfo sms_speedblvRomDesc[] = {
	{ "speedball (europe) (virgin).bin",	0x20000, 0x5ccc1a65, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_speedblv)
STD_ROM_FN(sms_speedblv)

struct BurnDriver BurnDrvsms_speedblv = {
	"sms_speedblv", NULL, NULL, NULL, "1992",
	"Speedball (Virgin) (Euro, USA)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_speedblvRomInfo, sms_speedblvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// SpellCaster (Euro, USA, Bra)

static struct BurnRomInfo sms_spellcstRomDesc[] = {
	{ "mpr-12532-t.ic2",	0x80000, 0x4752cae7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spellcst)
STD_ROM_FN(sms_spellcst)

struct BurnDriver BurnDrvsms_spellcst = {
	"sms_spellcst", NULL, NULL, NULL, "1988",
	"SpellCaster (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spellcstRomInfo, sms_spellcstRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spider-Man - Return of the Sinister Six (Euro, Bra)

static struct BurnRomInfo sms_spidermnRomDesc[] = {
	{ "spider-man - return of the sinister six (europe).bin",	0x40000, 0xebe45388, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spidermn)
STD_ROM_FN(sms_spidermn)

struct BurnDriver BurnDrvsms_spidermn = {
	"sms_spidermn", NULL, NULL, NULL, "1992",
	"Spider-Man - Return of the Sinister Six (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_spidermnRomInfo, sms_spidermnRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spider-Man vs. The Kingpin (Euro, USA, Bra)

static struct BurnRomInfo sms_spidkingRomDesc[] = {
	{ "mpr-13949 t42.ic1",	0x40000, 0x908ff25c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spidking)
STD_ROM_FN(sms_spidking)

struct BurnDriver BurnDrvsms_spidking = {
	"sms_spidking", NULL, NULL, NULL, "1990",
	"Spider-Man vs. The Kingpin (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spidkingRomInfo, sms_spidkingRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Pad Football (USA)

static struct BurnRomInfo sms_sportsftRomDesc[] = {
	{ "sports pad football (usa).bin",	0x20000, 0xe42e4998, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sportsft)
STD_ROM_FN(sms_sportsft)

struct BurnDriver BurnDrvsms_sportsft = {
	"sms_sportsft", NULL, NULL, NULL, "1987",
	"Sports Pad Football (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sportsftRomInfo, sms_sportsftRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Pad Soccer (Jpn)

static struct BurnRomInfo sms_sportsscRomDesc[] = {
	{ "sports pad soccer (japan).bin",	0x20000, 0x41c948bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sportssc)
STD_ROM_FN(sms_sportssc)

struct BurnDriver BurnDrvsms_sportssc = {
	"sms_sportssc", "sms_worldsoc", NULL, NULL, "1988",
	"Sports Pad Soccer (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sportsscRomInfo, sms_sportsscRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spy vs. Spy (Euro, USA, Bra)

static struct BurnRomInfo sms_spyvsspyRomDesc[] = {
	{ "spy vs. spy (usa, europe).bin",	0x08000, 0x78d7faab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spyvsspy)
STD_ROM_FN(sms_spyvsspy)

struct BurnDriver BurnDrvsms_spyvsspy = {
	"sms_spyvsspy", NULL, NULL, NULL, "1986",
	"Spy vs. Spy (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spyvsspyRomInfo, sms_spyvsspyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spy vs Spy (Kor)

static struct BurnRomInfo sms_spyvsspykRomDesc[] = {
	{ "spy vs spy (japan).bin",	0x08000, 0xd41b9a08, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spyvsspyk)
STD_ROM_FN(sms_spyvsspyk)

struct BurnDriver BurnDrvsms_spyvsspyk = {
	"sms_spyvsspyk", "sms_spyvsspy", NULL, NULL, "1986",
	"Spy vs Spy (Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spyvsspykRomInfo, sms_spyvsspykRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spy vs. Spy (Jpn, Pirate?)

static struct BurnRomInfo sms_spyvsspyj1RomDesc[] = {
	{ "spy vs. spy (j) [hack].bin",	0x08000, 0xa71bc542, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spyvsspyj1)
STD_ROM_FN(sms_spyvsspyj1)

struct BurnDriver BurnDrvsms_spyvsspyj1 = {
	"sms_spyvsspyj1", "sms_spyvsspy", NULL, NULL, "1986",
	"Spy vs. Spy (Jpn, Pirate?)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spyvsspyj1RomInfo, sms_spyvsspyj1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spy vs Spy (Tw)

static struct BurnRomInfo sms_spyvsspytwRomDesc[] = {
	{ "spy vs spy (tw).bin",	0x08000, 0x689f58a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spyvsspytw)
STD_ROM_FN(sms_spyvsspytw)

struct BurnDriver BurnDrvsms_spyvsspytw = {
	"sms_spyvsspytw", "sms_spyvsspy", NULL, NULL, "1986?",
	"Spy vs Spy (Tw)\0", NULL, "Aaronix", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spyvsspytwRomInfo, sms_spyvsspytwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spy vs. Spy (USA, Display Unit Sample)

static struct BurnRomInfo sms_spyvsspysRomDesc[] = {
	{ "spy vs. spy (u) (display-unit cart) [!].bin",	0x40000, 0xb87e1b2b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spyvsspys)
STD_ROM_FN(sms_spyvsspys)

struct BurnDriver BurnDrvsms_spyvsspys = {
	"sms_spyvsspys", "sms_spyvsspy", NULL, NULL, "1986",
	"Spy vs. Spy (USA, Display Unit Sample)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spyvsspysRomInfo, sms_spyvsspysRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Space Invaders (Euro)

static struct BurnRomInfo sms_ssinvRomDesc[] = {
	{ "super space invaders (europe).bin",	0x40000, 0x1d6244ee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ssinv)
STD_ROM_FN(sms_ssinv)

struct BurnDriver BurnDrvsms_ssinv = {
	"sms_ssinv", NULL, NULL, NULL, "1991",
	"Super Space Invaders (Euro)\0", NULL, "Domark", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ssinvRomInfo, sms_ssinvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Smash T.V. (Euro)

static struct BurnRomInfo sms_smashtvRomDesc[] = {
	{ "super smash t.v. (europe).bin",	0x40000, 0xe0b1aff8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_smashtv)
STD_ROM_FN(sms_smashtv)

struct BurnDriver BurnDrvsms_smashtv = {
	"sms_smashtv", NULL, NULL, NULL, "1992",
	"Smash T.V. (Euro)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_smashtvRomInfo, sms_smashtvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Star Wars (Euro, Bra)

static struct BurnRomInfo sms_starwarsRomDesc[] = {
	{ "star wars (europe).bin",	0x80000, 0xd4b8f66d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_starwars)
STD_ROM_FN(sms_starwars)

struct BurnDriver BurnDrvsms_starwars = {
	"sms_starwars", NULL, NULL, NULL, "1993",
	"Star Wars (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_starwarsRomInfo, sms_starwarsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Street Master (Kor)

static struct BurnRomInfo sms_strtmastRomDesc[] = {
	{ "street master (kr).bin",	0x20000, 0x83f0eede, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_strtmast)
STD_ROM_FN(sms_strtmast)

struct BurnDriver BurnDrvsms_strtmast = {
	"sms_strtmast", NULL, NULL, NULL, "1992",
	"Street Master (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_strtmastRomInfo, sms_strtmastRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Strider (Euro, USA, Bra, Kor)

static struct BurnRomInfo sms_striderRomDesc[] = {
	{ "mpr-14093 w08.ic1",	0x80000, 0x9802ed31, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_strider)
STD_ROM_FN(sms_strider)

struct BurnDriver BurnDrvsms_strider = {
	"sms_strider", NULL, NULL, NULL, "1991",
	"Strider (Euro, USA, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_striderRomInfo, sms_striderRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Strider (USA, Display Unit Sample)

static struct BurnRomInfo sms_striderdRomDesc[] = {
	{ "strider (demo).bin",	0x20000, 0xb990269a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_striderd)
STD_ROM_FN(sms_striderd)

struct BurnDriver BurnDrvsms_striderd = {
	"sms_striderd", "sms_strider", NULL, NULL, "1991",
	"Strider (USA, Display Unit Sample)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_striderdRomInfo, sms_striderdRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Strider II (Euro, Bra)

static struct BurnRomInfo sms_strider2RomDesc[] = {
	{ "strider ii (europe).bin",	0x40000, 0xb8f0915a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_strider2)
STD_ROM_FN(sms_strider2)

struct BurnDriver BurnDrvsms_strider2 = {
	"sms_strider2", NULL, NULL, NULL, "1992",
	"Strider II (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_strider2RomInfo, sms_strider2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Submarine Attack (Euro)

static struct BurnRomInfo sms_submarinRomDesc[] = {
	{ "submarine attack (europe).bin",	0x40000, 0xd8f2f1b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_submarin)
STD_ROM_FN(sms_submarin)

struct BurnDriver BurnDrvsms_submarin = {
	"sms_submarin", NULL, NULL, NULL, "1990",
	"Submarine Attack (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_submarinRomInfo, sms_submarinRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Suho Jeonsa (Kor)

static struct BurnRomInfo sms_suhocheoRomDesc[] = {
	{ "suho cheonsa (kr).bin",	0x40000, 0x01686d67, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_suhocheo)
STD_ROM_FN(sms_suhocheo)

struct BurnDriver BurnDrvsms_suhocheo = {
	"sms_suhocheo", NULL, NULL, NULL, "19??",
	"Suho Jeonsa (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_suhocheoRomInfo, sms_suhocheoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sukeban Deka II - Shoujo Tekkamen Densetsu (Jpn)

static struct BurnRomInfo sms_sukebanRomDesc[] = {
	{ "sukeban deka ii - shoujo tekkamen densetsu (japan).bin",	0x20000, 0xb13df647, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sukeban)
STD_ROM_FN(sms_sukeban)

struct BurnDriver BurnDrvsms_sukeban = {
	"sms_sukeban", NULL, NULL, NULL, "1987",
	"Sukeban Deka II - Shoujo Tekkamen Densetsu (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sukebanRomInfo, sms_sukebanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Summer Games (Euro)

static struct BurnRomInfo sms_sumgamesRomDesc[] = {
	{ "summer games (europe).bin",	0x20000, 0x8da5c93f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sumgames)
STD_ROM_FN(sms_sumgames)

struct BurnDriver BurnDrvsms_sumgames = {
	"sms_sumgames", NULL, NULL, NULL, "1991",
	"Summer Games (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sumgamesRomInfo, sms_sumgamesRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Summer Games (Euro, Prototype)

static struct BurnRomInfo sms_sumgamespRomDesc[] = {
	{ "summer games (europe) (beta).bin",	0x20000, 0x80eb1fff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sumgamesp)
STD_ROM_FN(sms_sumgamesp)

struct BurnDriver BurnDrvsms_sumgamesp = {
	"sms_sumgamesp", "sms_sumgames", NULL, NULL, "1991",
	"Summer Games (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sumgamespRomInfo, sms_sumgamespRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Superman - The Man of Steel (Euro, Bra)

static struct BurnRomInfo sms_supermanRomDesc[] = {
	{ "mpr-15506.ic1",	0x40000, 0x6f9ac98f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_superman)
STD_ROM_FN(sms_superman)

struct BurnDriver BurnDrvsms_superman = {
	"sms_superman", NULL, NULL, NULL, "1993",
	"Superman - The Man of Steel (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_supermanRomInfo, sms_supermanRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Arkanoid (Kor)

static struct BurnRomInfo sms_superarkRomDesc[] = {
	{ "woody pop (super arkanoid) (kr).bin",	0x08000, 0xc9dd4e5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_superark)
STD_ROM_FN(sms_superark)

struct BurnDriver BurnDrvsms_superark = {
	"sms_superark", "sms_woodypop", NULL, NULL, "1989",
	"Super Arkanoid (Kor)\0", NULL, "HiCom", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_JAPANESE, GBF_MISC, 0,
	SMSGetZipName, sms_superarkRomInfo, sms_superarkRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Basketball (USA, CES Demo)

static struct BurnRomInfo sms_suprbsktRomDesc[] = {
	{ "super basketball (usa) (sample).bin",	0x10000, 0x0dbf3b4a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_suprbskt)
STD_ROM_FN(sms_suprbskt)

struct BurnDriver BurnDrvsms_suprbskt = {
	"sms_suprbskt", NULL, NULL, NULL, "1989?",
	"Super Basketball (USA, CES Demo)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_suprbsktRomInfo, sms_suprbsktRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Bioman 1 (Kor)

static struct BurnRomInfo sms_sbioman1RomDesc[] = {
	{ "super bioman 1.bin",	0x10000, 0xa66d26cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sbioman1)
STD_ROM_FN(sms_sbioman1)

struct BurnDriver BurnDrvsms_sbioman1 = {
	"sms_sbioman1", NULL, NULL, NULL, "1992",
	"Super Bioman 1 (Kor)\0", NULL, "HiCom", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sbioman1RomInfo, sms_sbioman1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Boy I (Kor)

static struct BurnRomInfo sms_sboy1RomDesc[] = {
	{ "super boy 1.bin",	0x0c000, 0xbf5a994a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sboy1)
STD_ROM_FN(sms_sboy1)

struct BurnDriver BurnDrvsms_sboy1 = {
	"sms_sboy1", NULL, NULL, NULL, "1989",
	"Super Boy I (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sboy1RomInfo, sms_sboy1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Boy II (Kor)

static struct BurnRomInfo sms_sboy2RomDesc[] = {
	{ "super boy ii (kor).bin",	0x0c000, 0x67c2f0ff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sboy2)
STD_ROM_FN(sms_sboy2)

struct BurnDriver BurnDrvsms_sboy2 = {
	"sms_sboy2", NULL, NULL, NULL, "1989",
	"Super Boy II (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_KOREA, GBF_MISC, 0,
	SMSGetZipName, sms_sboy2RomInfo, sms_sboy2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Boy 3 (Kor)

static struct BurnRomInfo sms_sboy3RomDesc[] = {
	{ "super boy 3.bin",	0x20000, 0x9195c34c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sboy3)
STD_ROM_FN(sms_sboy3)

struct BurnDriver BurnDrvsms_sboy3 = {
	"sms_sboy3", NULL, NULL, NULL, "1991",
	"Super Boy 3 (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_sboy3RomInfo, sms_sboy3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Boy 4 (Kor)

static struct BurnRomInfo sms_sboy4RomDesc[] = {
	{ "super boy 4 (kor).bin",	0x40000, 0xb995b4f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sboy4)
STD_ROM_FN(sms_sboy4)

struct BurnDriver BurnDrvsms_sboy4 = {
	"sms_sboy4", NULL, NULL, NULL, "1992",
	"Super Boy 4 (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sboy4RomInfo, sms_sboy4RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Kick Off (Euro, Bra)

static struct BurnRomInfo sms_skickoffRomDesc[] = {
	{ "mpr-14397-f.ic1",	0x40000, 0x406aa0c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_skickoff)
STD_ROM_FN(sms_skickoff)

struct BurnDriver BurnDrvsms_skickoff = {
	"sms_skickoff", NULL, NULL, NULL, "1991",
	"Super Kick Off (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_skickoffRomInfo, sms_skickoffRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Off Road (Euro)

static struct BurnRomInfo sms_superoffRomDesc[] = {
	{ "super off road (europe).bin",	0x20000, 0x54f68c2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_superoff)
STD_ROM_FN(sms_superoff)

struct BurnDriver BurnDrvsms_superoff = {
	"sms_superoff", NULL, NULL, NULL, "1992",
	"Super Off Road (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_superoffRomInfo, sms_superoffRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Racing (Jpn)

static struct BurnRomInfo sms_superracRomDesc[] = {
	{ "super racing (japan).bin",	0x40000, 0x7e0ef8cb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_superrac)
STD_ROM_FN(sms_superrac)

struct BurnDriver BurnDrvsms_superrac = {
	"sms_superrac", NULL, NULL, NULL, "1988",
	"Super Racing (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_superracRomInfo, sms_superracRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Tennis (Euro, USA)

static struct BurnRomInfo sms_stennisRomDesc[] = {
	{ "mpr-12584.ic1",	0x08000, 0x914514e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_stennis)
STD_ROM_FN(sms_stennis)

struct BurnDriver BurnDrvsms_stennis = {
	"sms_stennis", NULL, NULL, NULL, "1985",
	"Super Tennis (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_stennisRomInfo, sms_stennisRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// T2 - The Arcade Game (Euro)

static struct BurnRomInfo sms_t2agRomDesc[] = {
	{ "mpr-16073.ic1",	0x80000, 0x93ca8152, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_t2ag)
STD_ROM_FN(sms_t2ag)

struct BurnDriver BurnDrvsms_t2ag = {
	"sms_t2ag", NULL, NULL, NULL, "1993",
	"T2 - The Arcade Game (Euro)\0", NULL, "Arena", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_t2agRomInfo, sms_t2agRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz-Mania (Euro, Bra)

static struct BurnRomInfo sms_tazmaniaRomDesc[] = {
	{ "mpr-15161.ic1",	0x40000, 0x7cc3e837, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tazmania)
STD_ROM_FN(sms_tazmania)

struct BurnDriver BurnDrvsms_tazmania = {
	"sms_tazmania", NULL, NULL, NULL, "1992",
	"Taz-Mania (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tazmaniaRomInfo, sms_tazmaniaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz-Mania (Euro, Prototype)

static struct BurnRomInfo sms_tazmaniapRomDesc[] = {
	{ "taz-mania (europe) (beta).bin",	0x40000, 0x1b312e04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tazmaniap)
STD_ROM_FN(sms_tazmaniap)

struct BurnDriver BurnDrvsms_tazmaniap = {
	"sms_tazmaniap", "sms_tazmania", NULL, NULL, "1992",
	"Taz-Mania (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tazmaniapRomInfo, sms_tazmaniapRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Bra)

static struct BurnRomInfo sms_tazmarsRomDesc[] = {
	{ "taz in escape from mars (brazil).bin",	0x80000, 0x11ce074c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tazmars)
STD_ROM_FN(sms_tazmars)

struct BurnDriver BurnDrvsms_tazmars = {
	"sms_tazmars", NULL, NULL, NULL, "19??",
	"Taz in Escape from Mars (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tazmarsRomInfo, sms_tazmarsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tecmo World Cup '92 (Euro, Prototype)

static struct BurnRomInfo sms_tecmow92RomDesc[] = {
	{ "tecmo world cup '92 (europe) (beta).bin",	0x40000, 0x96e75f48, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tecmow92)
STD_ROM_FN(sms_tecmow92)

struct BurnDriver BurnDrvsms_tecmow92 = {
	"sms_tecmow92", NULL, NULL, NULL, "19??",
	"Tecmo World Cup '92 (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tecmow92RomInfo, sms_tecmow92RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tecmo World Cup '93 (Euro)

static struct BurnRomInfo sms_tecmow93RomDesc[] = {
	{ "tecmo world cup '93 (europe).bin",	0x40000, 0x5a1c3dde, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tecmow93)
STD_ROM_FN(sms_tecmow93)

struct BurnDriver BurnDrvsms_tecmow93 = {
	"sms_tecmow93", NULL, NULL, NULL, "1993",
	"Tecmo World Cup '93 (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tecmow93RomInfo, sms_tecmow93RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Teddy Boy (Euro, USA, Bra)

static struct BurnRomInfo sms_teddyboyRomDesc[] = {
	{ "mpr-12668.ic1",	0x08000, 0x2728faa3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_teddyboy)
STD_ROM_FN(sms_teddyboy)

struct BurnDriver BurnDrvsms_teddyboy = {
	"sms_teddyboy", NULL, NULL, NULL, "1985",
	"Teddy Boy (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_teddyboyRomInfo, sms_teddyboyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Teddy Boy Blues (Jpn, Pirate?)

static struct BurnRomInfo sms_teddyboyj1RomDesc[] = {
	{ "teddy boy blues (j) [hack].bin",	0x08000, 0x9dfa67ee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_teddyboyj1)
STD_ROM_FN(sms_teddyboyj1)

struct BurnDriver BurnDrvsms_teddyboyj1 = {
	"sms_teddyboyj1", "sms_teddyboy", NULL, NULL, "1985",
	"Teddy Boy Blues (Jpn, Pirate?)\0", NULL, "pirate", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_teddyboyj1RomInfo, sms_teddyboyj1RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tennis Ace (Euro, Bra)

static struct BurnRomInfo sms_tennisRomDesc[] = {
	{ "tennis ace (europe).bin",	0x40000, 0x1a390b93, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tennis)
STD_ROM_FN(sms_tennis)

struct BurnDriver BurnDrvsms_tennis = {
	"sms_tennis", NULL, NULL, NULL, "1989",
	"Tennis Ace (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tennisRomInfo, sms_tennisRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tensai Bakabon (Jpn)

static struct BurnRomInfo sms_bakabonRomDesc[] = {
	{ "tensai bakabon (japan).bin",	0x40000, 0x8132ab2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bakabon)
STD_ROM_FN(sms_bakabon)

struct BurnDriver BurnDrvsms_bakabon = {
	"sms_bakabon", NULL, NULL, NULL, "1988",
	"Tensai Bakabon (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bakabonRomInfo, sms_bakabonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Terminator 2 - Judgment Day (Euro)

static struct BurnRomInfo sms_term2RomDesc[] = {
	{ "terminator 2 - judgment day (europe).bin",	0x40000, 0xac56104f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_term2)
STD_ROM_FN(sms_term2)

struct BurnDriver BurnDrvsms_term2 = {
	"sms_term2", NULL, NULL, NULL, "1993",
	"Terminator 2 - Judgment Day (Euro)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_term2RomInfo, sms_term2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Terminator (Euro)

static struct BurnRomInfo sms_termntrRomDesc[] = {
	{ "terminator, the (europe).bin",	0x40000, 0xedc5c012, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_termntr)
STD_ROM_FN(sms_termntr)

struct BurnDriver BurnDrvsms_termntr = {
	"sms_termntr", NULL, NULL, NULL, "1992",
	"The Terminator (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_termntrRomInfo, sms_termntrRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Terminator (Bra)

static struct BurnRomInfo sms_termntrbRomDesc[] = {
	{ "terminator, the (brazil).bin",	0x40000, 0xe3d5ce9a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_termntrb)
STD_ROM_FN(sms_termntrb)

struct BurnDriver BurnDrvsms_termntrb = {
	"sms_termntrb", "sms_termntr", NULL, NULL, "1992",
	"The Terminator (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_termntrbRomInfo, sms_termntrbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Thunder Blade (Euro, USA, Bra)

static struct BurnRomInfo sms_tbladeRomDesc[] = {
	{ "mpr-11820f.ic1",	0x40000, 0xae920e4b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tblade)
STD_ROM_FN(sms_tblade)

struct BurnDriver BurnDrvsms_tblade = {
	"sms_tblade", NULL, NULL, NULL, "1988",
	"Thunder Blade (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tbladeRomInfo, sms_tbladeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Thunder Blade (Jpn)

static struct BurnRomInfo sms_tbladejRomDesc[] = {
	{ "thunder blade (japan).bin",	0x40000, 0xc0ce19b1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tbladej)
STD_ROM_FN(sms_tbladej)

struct BurnDriver BurnDrvsms_tbladej = {
	"sms_tbladej", "sms_tblade", NULL, NULL, "1988",
	"Thunder Blade (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tbladejRomInfo, sms_tbladejRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Time Soldiers (Euro, USA, Bra)

static struct BurnRomInfo sms_timesoldRomDesc[] = {
	{ "mpr-12129f.ic1",	0x40000, 0x51bd14be, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_timesold)
STD_ROM_FN(sms_timesold)

struct BurnDriver BurnDrvsms_timesold = {
	"sms_timesold", NULL, NULL, NULL, "1989",
	"Time Soldiers (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_timesoldRomInfo, sms_timesoldRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The New Zealand Story (Euro)

static struct BurnRomInfo sms_tnzsRomDesc[] = {
	{ "new zealand story, the (europe).bin",	0x40000, 0xc660ff34, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tnzs)
STD_ROM_FN(sms_tnzs)

struct BurnDriver BurnDrvsms_tnzs = {
	"sms_tnzs", NULL, NULL, NULL, "1992",
	"The New Zealand Story (Euro)\0", NULL, "TecMagik", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_tnzsRomInfo, sms_tnzsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tom and Jerry - The Movie (Euro, Bra)

static struct BurnRomInfo sms_tomjermvRomDesc[] = {
	{ "tom and jerry - the movie (europe).bin",	0x40000, 0xbf7b7285, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tomjermv)
STD_ROM_FN(sms_tomjermv)

struct BurnDriver BurnDrvsms_tomjermv = {
	"sms_tomjermv", NULL, NULL, NULL, "1992",
	"Tom and Jerry - The Movie (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tomjermvRomInfo, sms_tomjermvRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tom and Jerry (Prototype)

static struct BurnRomInfo sms_tomjerryRomDesc[] = {
	{ "tom and jerry (europe) (beta).bin",	0x40000, 0x0c2fc2de, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tomjerry)
STD_ROM_FN(sms_tomjerry)

struct BurnDriver BurnDrvsms_tomjerry = {
	"sms_tomjerry", "sms_tomjermv", NULL, NULL, "1992",
	"Tom and Jerry (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tomjerryRomInfo, sms_tomjerryRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Toto World 3 (Kor)

static struct BurnRomInfo sms_totowld3RomDesc[] = {
	{ "toto world 3 (korea) (unl).bin",	0x40000, 0x4f8d75ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_totowld3)
STD_ROM_FN(sms_totowld3)

struct BurnDriver BurnDrvsms_totowld3 = {
	"sms_totowld3", NULL, NULL, NULL, "1993",
	"Toto World 3 (Kor)\0", NULL, "Open Corp.", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_totowld3RomInfo, sms_totowld3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// TransBot (Euro, USA, Bra)

static struct BurnRomInfo sms_transbotRomDesc[] = {
	{ "mpr-12552.ic1",	0x08000, 0x4bc42857, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_transbot)
STD_ROM_FN(sms_transbot)

struct BurnDriver BurnDrvsms_transbot = {
	"sms_transbot", NULL, NULL, NULL, "1985",
	"TransBot (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_transbotRomInfo, sms_transbotRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// TransBot (USA, Prototype)

static struct BurnRomInfo sms_transbotpRomDesc[] = {
	{ "transbot [proto].bin",	0x08000, 0x58b99750, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_transbotp)
STD_ROM_FN(sms_transbotp)

struct BurnDriver BurnDrvsms_transbotp = {
	"sms_transbotp", "sms_transbot", NULL, NULL, "1985",
	"TransBot (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_transbotpRomInfo, sms_transbotpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Treinamento Do Mymo (Bra)

static struct BurnRomInfo sms_treinamRomDesc[] = {
	{ "treinamento do mymo (b).bin",	0x40000, 0xe94784f2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_treinam)
STD_ROM_FN(sms_treinam)

struct BurnDriver BurnDrvsms_treinam = {
	"sms_treinam", NULL, NULL, NULL, "19??",
	"Treinamento Do Mymo (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_treinamRomInfo, sms_treinamRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Trivial Pursuit - Genus Edition (Euro)

static struct BurnRomInfo sms_trivialRomDesc[] = {
	{ "trivial pursuit - genus edition (europe) (en,fr,de,es).bin",	0x80000, 0xe5374022, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_trivial)
STD_ROM_FN(sms_trivial)

struct BurnDriver BurnDrvsms_trivial = {
	"sms_trivial", NULL, NULL, NULL, "1992",
	"Trivial Pursuit - Genus Edition (Euro)\0", NULL, "Domark", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_trivialRomInfo, sms_trivialRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ttoriui Moheom (Kor)

static struct BurnRomInfo sms_ttoriuiRomDesc[] = {
	{ "ttoriui moheom (kr).bin",	0x08000, 0x178801d2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ttoriui)
STD_ROM_FN(sms_ttoriui)

struct BurnDriver BurnDrvsms_ttoriui = {
	"sms_ttoriui", "sms_myhero", NULL, NULL, "19??",
	"Ttoriui Moheom (Kor)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ttoriuiRomInfo, sms_ttoriuiRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Turma da Mônica em O Resgate (Bra)

static struct BurnRomInfo sms_turmamonRomDesc[] = {
	{ "mpr-15999.ic1",	0x40000, 0x22cca9bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_turmamon)
STD_ROM_FN(sms_turmamon)

struct BurnDriver BurnDrvsms_turmamon = {
	"sms_turmamon", "sms_wboy3", NULL, NULL, "1993",
	"Turma da Monica em O Resgate (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_turmamonRomInfo, sms_turmamonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// As Aventuras da TV Colosso (Bra)

static struct BurnRomInfo sms_tvcolosRomDesc[] = {
	{ "tv colosso (brazil).bin",	0x80000, 0xe1714a88, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_tvcolos)
STD_ROM_FN(sms_tvcolos)

struct BurnDriver BurnDrvsms_tvcolos = {
	"sms_tvcolos", "sms_asterix", NULL, NULL, "1996",
	"As Aventuras da TV Colosso (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_tvcolosRomInfo, sms_tvcolosRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ultima IV - Quest of the Avatar (Euro, Bra)

static struct BurnRomInfo sms_ultima4RomDesc[] = {
	{ "mpr-13135.ic1",	0x80000, 0xb52d60c8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ultima4)
STD_ROM_FN(sms_ultima4)

struct BurnDriver BurnDrvsms_ultima4 = {
	"sms_ultima4", NULL, NULL, NULL, "1990",
	"Ultima IV - Quest of the Avatar (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ultima4RomInfo, sms_ultima4RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ultima IV - Quest of the Avatar (Euro, Prototype)

static struct BurnRomInfo sms_ultima4pRomDesc[] = {
	{ "ultima iv - quest of the avatar (europe) (beta).bin",	0x80000, 0xde9f8517, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ultima4p)
STD_ROM_FN(sms_ultima4p)

struct BurnDriver BurnDrvsms_ultima4p = {
	"sms_ultima4p", "sms_ultima4", NULL, NULL, "1990",
	"Ultima IV - Quest of the Avatar (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ultima4pRomInfo, sms_ultima4pRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ultimate Soccer (Euro, Bra)

static struct BurnRomInfo sms_ultsoccrRomDesc[] = {
	{ "ultimate soccer (europe) (en,fr,de,es,it).bin",	0x40000, 0x15668ca4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ultsoccr)
STD_ROM_FN(sms_ultsoccr)

struct BurnDriver BurnDrvsms_ultsoccr = {
	"sms_ultsoccr", NULL, NULL, NULL, "1993",
	"Ultimate Soccer (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ultsoccrRomInfo, sms_ultsoccrRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Vampire (Euro, Prototype)

static struct BurnRomInfo sms_vampireRomDesc[] = {
	{ "vampire (europe) (beta).bin",	0x40000, 0x20f40cae, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_vampire)
STD_ROM_FN(sms_vampire)

struct BurnDriver BurnDrvsms_vampire = {
	"sms_vampire", "sms_mastdark", NULL, NULL, "19??",
	"Vampire (Euro, Prototype)\0", NULL, "Unknown", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_vampireRomInfo, sms_vampireRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Virtua Fighter Animation (Bra)

static struct BurnRomInfo sms_vfaRomDesc[] = {
	{ "virtua fighter animation (brazil).bin",	0x100000, 0x57f1545b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_vfa)
STD_ROM_FN(sms_vfa)

struct BurnDriver BurnDrvsms_vfa = {
	"sms_vfa", NULL, NULL, NULL, "1997",
	"Virtua Fighter Animation (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_vfaRomInfo, sms_vfaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Vigilante (Euro, USA, Bra)

static struct BurnRomInfo sms_vigilantRomDesc[] = {
	{ "mpr-12138f.ic1",	0x40000, 0xdfb0b161, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_vigilant)
STD_ROM_FN(sms_vigilant)

struct BurnDriver BurnDrvsms_vigilant = {
	"sms_vigilant", NULL, NULL, NULL, "1989",
	"Vigilante (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_vigilantRomInfo, sms_vigilantRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Walter Payton Football (USA)

static struct BurnRomInfo sms_wpaytonRomDesc[] = {
	{ "walter payton football (usa).bin",	0x40000, 0x3d55759b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wpayton)
STD_ROM_FN(sms_wpayton)

struct BurnDriver BurnDrvsms_wpayton = {
	"sms_wpayton", "sms_ameripf", NULL, NULL, "1989",
	"Walter Payton Football (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wpaytonRomInfo, sms_wpaytonRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wanted (Euro, USA, Bra)

static struct BurnRomInfo sms_wantedRomDesc[] = {
	{ "mpr-12533f.ic1",	0x20000, 0x5359762d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wanted)
STD_ROM_FN(sms_wanted)

struct BurnDriver BurnDrvsms_wanted = {
	"sms_wanted", NULL, NULL, NULL, "1989",
	"Wanted (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wantedRomInfo, sms_wantedRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy (Euro, USA, Bra, v1)

static struct BurnRomInfo sms_wboyRomDesc[] = {
	{ "wonder boy (usa, europe).bin",	0x20000, 0x73705c02, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboy)
STD_ROM_FN(sms_wboy)

struct BurnDriver BurnDrvsms_wboy = {
	"sms_wboy", NULL, NULL, NULL, "1987",
	"Wonder Boy (Euro, USA, Bra, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboyRomInfo, sms_wboyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy (Euro, Kor, v0) ~ Super Wonder Boy (Jpn, v0)

static struct BurnRomInfo sms_wboyaRomDesc[] = {
	{ "super wonder boy (japan).bin",	0x20000, 0xe2fcb6f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboya)
STD_ROM_FN(sms_wboya)

struct BurnDriver BurnDrvsms_wboya = {
	"sms_wboya", "sms_wboy", NULL, NULL, "1987",
	"Wonder Boy (Euro, Kor, v0) ~ Super Wonder Boy (Jpn, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboyaRomInfo, sms_wboyaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy III - The Dragon's Trap (Euro, USA, Kor)

static struct BurnRomInfo sms_wboy3RomDesc[] = {
	{ "mpr-12570.ic1",	0x40000, 0x679e1676, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboy3)
STD_ROM_FN(sms_wboy3)

struct BurnDriver BurnDrvsms_wboy3 = {
	"sms_wboy3", NULL, NULL, NULL, "1989",
	"Wonder Boy III - The Dragon's Trap (Euro, USA, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboy3RomInfo, sms_wboy3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy in Monster Land (Euro, USA)

static struct BurnRomInfo sms_wboymlndRomDesc[] = {
	{ "mpr-11487.ic1",	0x40000, 0x8cbef0c1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboymlnd)
STD_ROM_FN(sms_wboymlnd)

struct BurnDriver BurnDrvsms_wboymlnd = {
	"sms_wboymlnd", NULL, NULL, NULL, "1988",
	"Wonder Boy in Monster Land (Euro, USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboymlndRomInfo, sms_wboymlndRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy in Monster Land (Prototype)

static struct BurnRomInfo sms_wboymlndpRomDesc[] = {
	{ "wonder boy in monster land [proto].bin",	0x40000, 0x8312c429, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboymlndp)
STD_ROM_FN(sms_wboymlndp)

struct BurnDriver BurnDrvsms_wboymlndp = {
	"sms_wboymlndp", "sms_wboymlnd", NULL, NULL, "1988",
	"Wonder Boy in Monster Land (Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboymlndpRomInfo, sms_wboymlndpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Wonder Boy - Monster World (Jpn)

static struct BurnRomInfo sms_wboymlndjRomDesc[] = {
	{ "super wonder boy - monster world (japan).bin",	0x40000, 0xb1da6a30, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboymlndj)
STD_ROM_FN(sms_wboymlndj)

struct BurnDriver BurnDrvsms_wboymlndj = {
	"sms_wboymlndj", "sms_wboymlnd", NULL, NULL, "1988",
	"Super Wonder Boy - Monster World (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboymlndjRomInfo, sms_wboymlndjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy in Monster Land (Euro, USA, Hacked?)

static struct BurnRomInfo sms_wboymlndaRomDesc[] = {
	{ "wonder boy in monster land [hack].bin",	0x40000, 0x7522cf0a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboymlnda)
STD_ROM_FN(sms_wboymlnda)

struct BurnDriver BurnDrvsms_wboymlnda = {
	"sms_wboymlnda", "sms_wboymlnd", NULL, NULL, "1988",
	"Wonder Boy in Monster Land (Euro, USA, Hacked?)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboymlndaRomInfo, sms_wboymlndaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy in Monster World (Euro, Kor)

static struct BurnRomInfo sms_wboymwldRomDesc[] = {
	{ "mpr-15317.ic1",	0x80000, 0x7d7ce80b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboymwld)
STD_ROM_FN(sms_wboymwld)

struct BurnDriver BurnDrvsms_wboymwld = {
	"sms_wboymwld", NULL, NULL, NULL, "1993",
	"Wonder Boy in Monster World (Euro, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboymwldRomInfo, sms_wboymwldRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy in Monster World (Euro, Prototype)

static struct BurnRomInfo sms_wboymwldpRomDesc[] = {
	{ "wonder boy in monster world (europe) (beta).bin",	0x80000, 0x81bff9bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wboymwldp)
STD_ROM_FN(sms_wboymwldp)

struct BurnDriver BurnDrvsms_wboymwldp = {
	"sms_wboymwldp", "sms_wboymwld", NULL, NULL, "1993",
	"Wonder Boy in Monster World (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wboymwldpRomInfo, sms_wboymwldpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonsiin (Kor)

static struct BurnRomInfo sms_wonsiinRomDesc[] = {
	{ "wonsiin (kr).bin",	0x20000, 0xa05258f5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wonsiin)
STD_ROM_FN(sms_wonsiin)

struct BurnDriver BurnDrvsms_wonsiin = {
	"sms_wonsiin", NULL, NULL, NULL, "1991",
	"Wonsiin (Kor)\0", NULL, "Zemina", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_wonsiinRomInfo, sms_wonsiinRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Class Leader Board (Euro, Bra)

static struct BurnRomInfo sms_wcleadRomDesc[] = {
	{ "world class leader board (europe).bin",	0x40000, 0xc9a449b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wclead)
STD_ROM_FN(sms_wclead)

struct BurnDriver BurnDrvsms_wclead = {
	"sms_wclead", NULL, NULL, NULL, "1991",
	"World Class Leader Board (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wcleadRomInfo, sms_wcleadRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Cup Italia '90 (Euro, Bra)

static struct BurnRomInfo sms_wcup90RomDesc[] = {
	{ "mpr-13292.ic1",	0x20000, 0x6e1ad6fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wcup90)
STD_ROM_FN(sms_wcup90)

struct BurnDriver BurnDrvsms_wcup90 = {
	"sms_wcup90", NULL, NULL, NULL, "1990",
	"World Cup Italia '90 (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wcup90RomInfo, sms_wcup90RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Cup Italia '90 (USA, Demo)

static struct BurnRomInfo sms_wcup90dRomDesc[] = {
	{ "world cup italia '90 [demo].bin",	0x20000, 0xa0cf8b15, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wcup90d)
STD_ROM_FN(sms_wcup90d)

struct BurnDriver BurnDrvsms_wcup90d = {
	"sms_wcup90d", "sms_wcup90", NULL, NULL, "1990",
	"World Cup Italia '90 (USA, Demo)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wcup90dRomInfo, sms_wcup90dRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Cup USA 94 (Euro, Bra)

static struct BurnRomInfo sms_wcup94RomDesc[] = {
	{ "world cup usa 94 (europe) (en,fr,de,es,it,nl,pt,sv).bin",	0x80000, 0xa6bf8f9e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wcup94)
STD_ROM_FN(sms_wcup94)

struct BurnDriver BurnDrvsms_wcup94 = {
	"sms_wcup94", NULL, NULL, NULL, "1992",
	"World Cup USA 94 (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wcup94RomInfo, sms_wcup94RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wimbledon II (Euro, Bra)

static struct BurnRomInfo sms_wimbled2RomDesc[] = {
	{ "wimbledon ii (europe).bin",	0x40000, 0x7f3afe58, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wimbled2)
STD_ROM_FN(sms_wimbled2)

struct BurnDriver BurnDrvsms_wimbled2 = {
	"sms_wimbled2", NULL, NULL, NULL, "1993",
	"Wimbledon II (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wimbled2RomInfo, sms_wimbled2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wimbledon (Euro)

static struct BurnRomInfo sms_wimbledRomDesc[] = {
	{ "mpr-14686.ic1",	0x40000, 0x912d92af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wimbled)
STD_ROM_FN(sms_wimbled)

struct BurnDriver BurnDrvsms_wimbled = {
	"sms_wimbled", NULL, NULL, NULL, "1992",
	"Wimbledon (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wimbledRomInfo, sms_wimbledRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Winter Olympics - Lillehammer '94 (Euro)

static struct BurnRomInfo sms_wintolRomDesc[] = {
	{ "winter olympics - lillehammer '94 (europe) (en,fr,de,es,it,pt,sv,no).bin",	0x80000, 0xa20290b6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wintol)
STD_ROM_FN(sms_wintol)

struct BurnDriver BurnDrvsms_wintol = {
	"sms_wintol", NULL, NULL, NULL, "1993",
	"Winter Olympics - Lillehammer '94 (Euro)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wintolRomInfo, sms_wintolRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Winter Olympics - Lillehammer '94 (Bra)

static struct BurnRomInfo sms_wintolbRomDesc[] = {
	{ "winter olympics - lillehammer '94 (brazil) (en,fr,de,es,it,pt,sv,no).bin",	0x80000, 0x2fec2b4a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wintolb)
STD_ROM_FN(sms_wintolb)

struct BurnDriver BurnDrvsms_wintolb = {
	"sms_wintolb", "sms_wintol", NULL, NULL, "1993",
	"Winter Olympics - Lillehammer '94 (Bra)\0", NULL, "U.S. Gold", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wintolbRomInfo, sms_wintolbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wolfchild (Euro, Bra)

static struct BurnRomInfo sms_wolfchldRomDesc[] = {
	{ "mpr-15714.ic1",	0x40000, 0x1f8efa1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wolfchld)
STD_ROM_FN(sms_wolfchld)

struct BurnDriver BurnDrvsms_wolfchld = {
	"sms_wolfchld", NULL, NULL, NULL, "1993",
	"Wolfchild (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wolfchldRomInfo, sms_wolfchldRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Games (Euro, Bra)

static struct BurnRomInfo sms_wldgamesRomDesc[] = {
	{ "world games (europe).bin",	0x20000, 0xa2a60bc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wldgames)
STD_ROM_FN(sms_wldgames)

struct BurnDriver BurnDrvsms_wldgames = {
	"sms_wldgames", NULL, NULL, NULL, "1989",
	"World Games (Euro, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wldgamesRomInfo, sms_wldgamesRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Games (Euro, Prototype)

static struct BurnRomInfo sms_wldgamespRomDesc[] = {
	{ "world games (europe) (beta).bin",	0x20000, 0x914d3fc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wldgamesp)
STD_ROM_FN(sms_wldgamesp)

struct BurnDriver BurnDrvsms_wldgamesp = {
	"sms_wldgamesp", "sms_wldgames", NULL, NULL, "1989",
	"World Games (Euro, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wldgamespRomInfo, sms_wldgamespRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Grand Prix (Euro)

static struct BurnRomInfo sms_worldgpRomDesc[] = {
	{ "mpr-10156.ic1",	0x20000, 0x4aaad0d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_worldgp)
STD_ROM_FN(sms_worldgp)

struct BurnDriver BurnDrvsms_worldgp = {
	"sms_worldgp", NULL, NULL, NULL, "1986",
	"World Grand Prix (Euro)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_worldgpRomInfo, sms_worldgpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Grand Prix (USA, Bra, Kor)

static struct BurnRomInfo sms_worldgpuRomDesc[] = {
	{ "mpr-10151.ic2",	0x20000, 0x7b369892, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_worldgpu)
STD_ROM_FN(sms_worldgpu)

struct BurnDriver BurnDrvsms_worldgpu = {
	"sms_worldgpu", "sms_worldgp", NULL, NULL, "1986",
	"World Grand Prix (USA, Bra, Kor)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_worldgpuRomInfo, sms_worldgpuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Grand Prix (USA, Prototype)

static struct BurnRomInfo sms_worldgppRomDesc[] = {
	{ "world grand prix [proto].bin",	0x20000, 0xb5a9f824, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_worldgpp)
STD_ROM_FN(sms_worldgpp)

struct BurnDriver BurnDrvsms_worldgpp = {
	"sms_worldgpp", "sms_worldgp", NULL, NULL, "1986",
	"World Grand Prix (USA, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_worldgppRomInfo, sms_worldgppRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Soccer (Euro, Jpn, Kor) ~ Great Soccer (USA)

static struct BurnRomInfo sms_worldsocRomDesc[] = {
	{ "mpr-10747f.ic1",	0x20000, 0x72112b75, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_worldsoc)
STD_ROM_FN(sms_worldsoc)

struct BurnDriver BurnDrvsms_worldsoc = {
	"sms_worldsoc", NULL, NULL, NULL, "1987",
	"World Soccer (Euro, Jpn, Kor) ~ Great Soccer (USA)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_worldsocRomInfo, sms_worldsocRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// WWF Wrestlemania - Steel Cage Challenge (Euro, Bra)

static struct BurnRomInfo sms_wwfsteelRomDesc[] = {
	{ "wwf wrestlemania - steel cage challenge (europe).bin",	0x40000, 0x2db21448, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_wwfsteel)
STD_ROM_FN(sms_wwfsteel)

struct BurnDriver BurnDrvsms_wwfsteel = {
	"sms_wwfsteel", NULL, NULL, NULL, "1993",
	"WWF Wrestlemania - Steel Cage Challenge (Euro, Bra)\0", NULL, "Flying Edge", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_wwfsteelRomInfo, sms_wwfsteelRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Where in the World is Carmen Sandiego? (USA)

static struct BurnRomInfo sms_carmnwldRomDesc[] = {
	{ "where in the world is carmen sandiego (usa).bin",	0x20000, 0x428b1e7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_carmnwld)
STD_ROM_FN(sms_carmnwld)

struct BurnDriver BurnDrvsms_carmnwld = {
	"sms_carmnwld", NULL, NULL, NULL, "1989",
	"Where in the World is Carmen Sandiego? (USA)\0", NULL, "Parker Brothers", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_carmnwldRomInfo, sms_carmnwldRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Where in the World is Carmen Sandiego? (Bra)

static struct BurnRomInfo sms_carmnwldbRomDesc[] = {
	{ "where in the world is carmen sandiego (brazil).bin",	0x20000, 0x88aa8ca6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_carmnwldb)
STD_ROM_FN(sms_carmnwldb)

struct BurnDriver BurnDrvsms_carmnwldb = {
	"sms_carmnwldb", "sms_carmnwld", NULL, NULL, "1989",
	"Where in the World is Carmen Sandiego? (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_carmnwldbRomInfo, sms_carmnwldbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Xenon 2 - Megablast (Image Works) (Euro)

static struct BurnRomInfo sms_xenon2RomDesc[] = {
	{ "xenon 2 - megablast (europe) (image works).bin",	0x40000, 0x5c205ee1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_xenon2)
STD_ROM_FN(sms_xenon2)

struct BurnDriver BurnDrvsms_xenon2 = {
	"sms_xenon2", NULL, NULL, NULL, "1991",
	"Xenon 2 - Megablast (Image Works) (Euro)\0", NULL, "Image Works", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_xenon2RomInfo, sms_xenon2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Xenon 2 - Megablast (Virgin) (Euro)

static struct BurnRomInfo sms_xenon2vRomDesc[] = {
	{ "xenon 2 - megablast (europe) (virgin).bin",	0x40000, 0xec726c0d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_xenon2v)
STD_ROM_FN(sms_xenon2v)

struct BurnDriver BurnDrvsms_xenon2v = {
	"sms_xenon2v", "sms_xenon2", NULL, NULL, "1991",
	"Xenon 2 - Megablast (Virgin) (Euro)\0", NULL, "Virgin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_xenon2vRomInfo, sms_xenon2vRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Bra)

static struct BurnRomInfo sms_xmenmojoRomDesc[] = {
	{ "x-men - mojo world (brazil).bin",	0x80000, 0x3e1387f6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_xmenmojo)
STD_ROM_FN(sms_xmenmojo)

struct BurnDriver BurnDrvsms_xmenmojo = {
	"sms_xmenmojo", NULL, NULL, NULL, "1996",
	"X-Men - Mojo World (Bra)\0", NULL, "Tec Toy", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_xmenmojoRomInfo, sms_xmenmojoRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Xyzolog (Kor)

static struct BurnRomInfo sms_xyzologRomDesc[] = {
	{ "xyzolog (kr).sms",	0x0c000, 0x565c799f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_xyzolog)
STD_ROM_FN(sms_xyzolog)

struct BurnDriver BurnDrvsms_xyzolog = {
	"sms_xyzolog", NULL, NULL, NULL, "19??",
	"Xyzolog (Kor)\0", NULL, "Taito?", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_MAPPER_MSX, GBF_MISC, 0,
	SMSGetZipName, sms_xyzologRomInfo, sms_xyzologRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ys (Jpn)

static struct BurnRomInfo sms_ysjRomDesc[] = {
	{ "ys (japan).bin",	0x40000, 0x32759751, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ysj)
STD_ROM_FN(sms_ysj)

struct BurnDriver BurnDrvsms_ysj = {
	"sms_ysj", "sms_ys", NULL, NULL, "1988",
	"Ys (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ysjRomInfo, sms_ysjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ys - The Vanished Omens (Euro, USA, Bra)

static struct BurnRomInfo sms_ysRomDesc[] = {
	{ "mpr-12044f.ic1",	0x40000, 0xb33e2827, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ys)
STD_ROM_FN(sms_ys)

struct BurnDriver BurnDrvsms_ys = {
	"sms_ys", NULL, NULL, NULL, "1988",
	"Ys - The Vanished Omens (Euro, USA, Bra)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ysRomInfo, sms_ysRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ys (USA, Demo)

static struct BurnRomInfo sms_ysdRomDesc[] = {
	{ "ys - the vanished omens [demo].bin",	0x40000, 0xe8b82066, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ysd)
STD_ROM_FN(sms_ysd)

struct BurnDriver BurnDrvsms_ysd = {
	"sms_ysd", "sms_ys", NULL, NULL, "1988",
	"Ys (USA, Demo)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ysdRomInfo, sms_ysdRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zaxxon 3-D (World)

static struct BurnRomInfo sms_zaxxon3dRomDesc[] = {
	{ "mpr-11197.ic1",	0x40000, 0xa3ef13cb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_zaxxon3d)
STD_ROM_FN(sms_zaxxon3d)

struct BurnDriver BurnDrvsms_zaxxon3d = {
	"sms_zaxxon3d", NULL, NULL, NULL, "1987",
	"Zaxxon 3-D (World)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_zaxxon3dRomInfo, sms_zaxxon3dRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zaxxon 3-D (World, Prototype)

static struct BurnRomInfo sms_zaxxon3dpRomDesc[] = {
	{ "zaxxon 3-d (world) (beta).bin",	0x40000, 0xbba74147, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_zaxxon3dp)
STD_ROM_FN(sms_zaxxon3dp)

struct BurnDriver BurnDrvsms_zaxxon3dp = {
	"sms_zaxxon3dp", "sms_zaxxon3d", NULL, NULL, "1987",
	"Zaxxon 3-D (World, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_zaxxon3dpRomInfo, sms_zaxxon3dpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zillion (Euro, v2)

static struct BurnRomInfo sms_zillionRomDesc[] = {
	{ "zillion (europe) (v1.2).bin",	0x20000, 0x7ba54510, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_zillion)
STD_ROM_FN(sms_zillion)

struct BurnDriver BurnDrvsms_zillion = {
	"sms_zillion", NULL, NULL, NULL, "1987",
	"Zillion (Euro, v2)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_zillionRomInfo, sms_zillionRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zillion (Euro, v0) ~ Akai Koudan Zillion (Jpn, v0)

static struct BurnRomInfo sms_zillionbRomDesc[] = {
	{ "zillion (japan, europe) (en,ja).bin",	0x20000, 0x60c19645, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_zillionb)
STD_ROM_FN(sms_zillionb)

struct BurnDriver BurnDrvsms_zillionb = {
	"sms_zillionb", "sms_zillion", NULL, NULL, "1987",
	"Zillion (Euro, v0) ~ Akai Koudan Zillion (Jpn, v0)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_zillionbRomInfo, sms_zillionbRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zillion (USA, v1)

static struct BurnRomInfo sms_zillionaRomDesc[] = {
	{ "zillion (usa) (v1.1).bin",	0x20000, 0x5718762c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_zilliona)
STD_ROM_FN(sms_zilliona)

struct BurnDriver BurnDrvsms_zilliona = {
	"sms_zilliona", "sms_zillion", NULL, NULL, "1987",
	"Zillion (USA, v1)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_zillionaRomInfo, sms_zillionaRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zillion II - The Tri Formation (Euro, USA) ~ Tri Formation (Jpn)

static struct BurnRomInfo sms_zillion2RomDesc[] = {
	{ "zillion ii - the tri formation (world).bin",	0x20000, 0x2f2e3bc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_zillion2)
STD_ROM_FN(sms_zillion2)

struct BurnDriver BurnDrvsms_zillion2 = {
	"sms_zillion2", NULL, NULL, NULL, "1987",
	"Zillion II - The Tri Formation (Euro, USA) ~ Tri Formation (Jpn)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_zillion2RomInfo, sms_zillion2RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zool - Ninja of the 'Nth' Dimension (Euro)

static struct BurnRomInfo sms_zoolRomDesc[] = {
	{ "mpr-15992.ic1",	0x40000, 0x9d9d0a5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_zool)
STD_ROM_FN(sms_zool)

struct BurnDriver BurnDrvsms_zool = {
	"sms_zool", NULL, NULL, NULL, "1993",
	"Zool - Ninja of the 'Nth' Dimension (Euro)\0", NULL, "Gremlin Interactive", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_zoolRomInfo, sms_zoolRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The A-Team (Music prototype)

static struct BurnRomInfo sms_sn_ateamRomDesc[] = {
	{ "a-team music [proto].bin",	0x08000, 0x0eb430ff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sn_ateam)
STD_ROM_FN(sms_sn_ateam)

struct BurnDriver BurnDrvsms_sn_ateam = {
	"sms_sn_ateam", NULL, NULL, NULL, "1992",
	"The A-Team (Music prototype)\0", NULL, "Probe", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sn_ateamRomInfo, sms_sn_ateamRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lethal Weapon 3 (Music prototype)

static struct BurnRomInfo sms_sn_lwep3RomDesc[] = {
	{ "lethal weapon 3 music [proto].bin",	0x08000, 0xeb71247b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_sn_lwep3)
STD_ROM_FN(sms_sn_lwep3)

struct BurnDriver BurnDrvsms_sn_lwep3 = {
	"sms_sn_lwep3", NULL, NULL, NULL, "1992",
	"Lethal Weapon 3 (Music prototype)\0", NULL, "Probe", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sn_lwep3RomInfo, sms_sn_lwep3RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astro Flash (Jpn, MyCard)

static struct BurnRomInfo sms_astroflRomDesc[] = {
	{ "astro flash (japan).bin",	0x08000, 0xc795182d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_astrofl)
STD_ROM_FN(sms_astrofl)

struct BurnDriver BurnDrvsms_astrofl = {
	"sms_astrofl", "sms_transbot", NULL, NULL, "1985",
	"Astro Flash (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_astroflRomInfo, sms_astroflRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bank Panic (Euro, Sega Card)

static struct BurnRomInfo sms_bankpcRomDesc[] = {
	{ "mpr-12554.ic1",	0x08000, 0x655fb1f4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_bankpc)
STD_ROM_FN(sms_bankpc)

struct BurnDriver BurnDrvsms_bankpc = {
	"sms_bankpc", "sms_bankp", NULL, NULL, "1987",
	"Bank Panic (Euro, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bankpcRomInfo, sms_bankpcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Comical Machine Gun Joe (Jpn, MyCard)

static struct BurnRomInfo sms_comicalRomDesc[] = {
	{ "comical machine gun joe (japan).bin",	0x08000, 0x9d549e08, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_comical)
STD_ROM_FN(sms_comical)

struct BurnDriver BurnDrvsms_comical = {
	"sms_comical", NULL, NULL, NULL, "1986",
	"Comical Machine Gun Joe (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_comicalRomInfo, sms_comicalRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F-16 Fighter (Euro, USA, Sega Card)

static struct BurnRomInfo sms_f16fightcRomDesc[] = {
	{ "f-16 fighter (euro, usa).bin",	0x08000, 0xeaebf323, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f16fightc)
STD_ROM_FN(sms_f16fightc)

struct BurnDriver BurnDrvsms_f16fightc = {
	"sms_f16fightc", "sms_f16fight", NULL, NULL, "1986",
	"F-16 Fighter (Euro, USA, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_f16fightcRomInfo, sms_f16fightcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F-16 Fighting Falcon (Jpn, MyCard)

static struct BurnRomInfo sms_f16falcjcRomDesc[] = {
	{ "f-16 fighting falcon (japan).bin",	0x08000, 0x7ce06fce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f16falcjc)
STD_ROM_FN(sms_f16falcjc)

struct BurnDriver BurnDrvsms_f16falcjc = {
	"sms_f16falcjc", "sms_f16fight", NULL, NULL, "1985",
	"F-16 Fighting Falcon (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_f16falcjcRomInfo, sms_f16falcjcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F-16 Fighting Falcon (USA, Sega Card for Display Unit)

static struct BurnRomInfo sms_f16falccRomDesc[] = {
	{ "f-16 fighting falcon (usa).bin",	0x08000, 0x184c23b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f16falcc)
STD_ROM_FN(sms_f16falcc)

struct BurnDriver BurnDrvsms_f16falcc = {
	"sms_f16falcc", "sms_f16fight", NULL, NULL, "1985",
	"F-16 Fighting Falcon (USA, Sega Card for Display Unit)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_f16falccRomInfo, sms_f16falccRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fushigi no Oshiro Pit Pot (Jpn, MyCard)

static struct BurnRomInfo sms_pitpotRomDesc[] = {
	{ "fushigi no oshiro pit pot (japan).bin",	0x08000, 0xe6795c53, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_pitpot)
STD_ROM_FN(sms_pitpot)

struct BurnDriver BurnDrvsms_pitpot = {
	"sms_pitpot", NULL, NULL, NULL, "1985",
	"Fushigi no Oshiro Pit Pot (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_pitpotRomInfo, sms_pitpotRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghost House (Euro, USA, Bra, Sega Card)

static struct BurnRomInfo sms_ghosthcRomDesc[] = {
	{ "ghost house (euro, usa, bra).bin",	0x08000, 0xf1f8ff2d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghosthc)
STD_ROM_FN(sms_ghosthc)

struct BurnDriver BurnDrvsms_ghosthc = {
	"sms_ghosthc", "sms_ghosth", NULL, NULL, "1986",
	"Ghost House (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghosthcRomInfo, sms_ghosthcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghost House (Sega Card, Prototype)

static struct BurnRomInfo sms_ghosthcpRomDesc[] = {
	{ "ghost house (usa, europe) (beta).bin",	0x08000, 0xc3e7c1ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghosthcp)
STD_ROM_FN(sms_ghosthcp)

struct BurnDriver BurnDrvsms_ghosthcp = {
	"sms_ghosthcp", "sms_ghosth", NULL, NULL, "1986",
	"Ghost House (Sega Card, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghosthcpRomInfo, sms_ghosthcpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ghost House (Jpn, MyCard)

static struct BurnRomInfo sms_ghosthjRomDesc[] = {
	{ "ghost house (japan).bin",	0x08000, 0xc0f3ce7e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_ghosthj)
STD_ROM_FN(sms_ghosthj)

struct BurnDriver BurnDrvsms_ghosthj = {
	"sms_ghosthj", "sms_ghosth", NULL, NULL, "1986",
	"Ghost House (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_ghosthjRomInfo, sms_ghosthjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Baseball (Jpn, MyCard)

static struct BurnRomInfo sms_greatbasjRomDesc[] = {
	{ "great baseball (japan).bin",	0x08000, 0x89e98a7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatbasj)
STD_ROM_FN(sms_greatbasj)

struct BurnDriver BurnDrvsms_greatbasj = {
	"sms_greatbasj", "sms_greatbas", NULL, NULL, "1985",
	"Great Baseball (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatbasjRomInfo, sms_greatbasjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Soccer (Euro, Sega Card)

static struct BurnRomInfo sms_greatscrcRomDesc[] = {
	{ "great soccer (europe).bin",	0x08000, 0x0ed170c9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatscrc)
STD_ROM_FN(sms_greatscrc)

struct BurnDriver BurnDrvsms_greatscrc = {
	"sms_greatscrc", "sms_greatscr", NULL, NULL, "1985",
	"Great Soccer (Euro, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatscrcRomInfo, sms_greatscrcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Soccer (Jpn, MyCard)

static struct BurnRomInfo sms_greatscrjRomDesc[] = {
	{ "great soccer (japan).bin",	0x08000, 0x2d7fd7ef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greatscrj)
STD_ROM_FN(sms_greatscrj)

struct BurnDriver BurnDrvsms_greatscrj = {
	"sms_greatscrj", "sms_greatscr", NULL, NULL, "1985",
	"Great Soccer (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greatscrjRomInfo, sms_greatscrjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Great Tennis (Jpn, MyCard)

static struct BurnRomInfo sms_greattnsRomDesc[] = {
	{ "great tennis (japan).bin",	0x08000, 0x95cbf3dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_greattns)
STD_ROM_FN(sms_greattns)

struct BurnDriver BurnDrvsms_greattns = {
	"sms_greattns", "sms_stennis", NULL, NULL, "1985",
	"Great Tennis (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_greattnsRomInfo, sms_greattnsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hang-On (Euro, Bra, Aus, Sega Card)

static struct BurnRomInfo sms_hangoncRomDesc[] = {
	{ "hang-on (europe).bin",	0x08000, 0x071b045e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hangonc)
STD_ROM_FN(sms_hangonc)

struct BurnDriver BurnDrvsms_hangonc = {
	"sms_hangonc", "sms_hangon", NULL, NULL, "1985",
	"Hang-On (Euro, Bra, Aus, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hangoncRomInfo, sms_hangoncRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hang-On (Jpn, MyCard)

static struct BurnRomInfo sms_hangonjRomDesc[] = {
	{ "hang-on (japan).bin",	0x08000, 0x5c01adf9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hangonj)
STD_ROM_FN(sms_hangonj)

struct BurnDriver BurnDrvsms_hangonj = {
	"sms_hangonj", "sms_hangon", NULL, NULL, "1985",
	"Hang-On (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_hangonjRomInfo, sms_hangonjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// My Hero (Euro, USA, Bra, Sega Card)

static struct BurnRomInfo sms_myherocRomDesc[] = {
	{ "my hero (usa, europe).bin",	0x08000, 0x62f0c23d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_myheroc)
STD_ROM_FN(sms_myheroc)

struct BurnDriver BurnDrvsms_myheroc = {
	"sms_myheroc", "sms_myhero", NULL, NULL, "1986",
	"My Hero (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_myherocRomInfo, sms_myherocRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Satellite 7 (Jpn, MyCard)

static struct BurnRomInfo sms_satell7RomDesc[] = {
	{ "satellite 7 (japan).bin",	0x08000, 0x16249e19, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_satell7)
STD_ROM_FN(sms_satell7)

struct BurnDriver BurnDrvsms_satell7 = {
	"sms_satell7", NULL, NULL, NULL, "1985",
	"Satellite 7 (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_satell7RomInfo, sms_satell7RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Seishun Scandal (Jpn, MyCard)

static struct BurnRomInfo sms_seishunRomDesc[] = {
	{ "seishun scandal (japan).bin",	0x08000, 0xf0ba2bc6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_seishun)
STD_ROM_FN(sms_seishun)

struct BurnDriver BurnDrvsms_seishun = {
	"sms_seishun", "sms_myhero", NULL, NULL, "1986",
	"Seishun Scandal (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_seishunRomInfo, sms_seishunRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spy vs. Spy (Euro, USA, Bra, Sega Card)

static struct BurnRomInfo sms_spyvsspycRomDesc[] = {
	{ "spy vs. spy (usa, europe).bin",	0x08000, 0x78d7faab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spyvsspyc)
STD_ROM_FN(sms_spyvsspyc)

struct BurnDriver BurnDrvsms_spyvsspyc = {
	"sms_spyvsspyc", "sms_spyvsspy", NULL, NULL, "1986",
	"Spy vs. Spy (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spyvsspycRomInfo, sms_spyvsspycRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spy vs Spy (Jpn, MyCard)

static struct BurnRomInfo sms_spyvsspyjRomDesc[] = {
	{ "spy vs spy (japan).bin",	0x08000, 0xd41b9a08, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_spyvsspyj)
STD_ROM_FN(sms_spyvsspyj)

struct BurnDriver BurnDrvsms_spyvsspyj = {
	"sms_spyvsspyj", "sms_spyvsspy", NULL, NULL, "1986",
	"Spy vs Spy (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_spyvsspyjRomInfo, sms_spyvsspyjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Tennis (Euro, USA, Sega Card)

static struct BurnRomInfo sms_stenniscRomDesc[] = {
	{ "super tennis (euro, usa).bin",	0x08000, 0x914514e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_stennisc)
STD_ROM_FN(sms_stennisc)

struct BurnDriver BurnDrvsms_stennisc = {
	"sms_stennisc", "sms_stennis", NULL, NULL, "1985",
	"Super Tennis (Euro, USA, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_stenniscRomInfo, sms_stenniscRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Teddy Boy (Euro, USA, Bra, Sega Card)

static struct BurnRomInfo sms_teddyboycRomDesc[] = {
	{ "teddy boy (euro, usa, bra).bin",	0x08000, 0x2728faa3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_teddyboyc)
STD_ROM_FN(sms_teddyboyc)

struct BurnDriver BurnDrvsms_teddyboyc = {
	"sms_teddyboyc", "sms_teddyboy", NULL, NULL, "1985",
	"Teddy Boy (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_teddyboycRomInfo, sms_teddyboycRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Teddy Boy Blues (Jpn, Ep-MyCard, Prototype)

static struct BurnRomInfo sms_teddyboyjpRomDesc[] = {
	{ "teddy boy blues (japan) (proto) (ep-mycard).bin",	0x0c000, 0xd7508041, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_teddyboyjp)
STD_ROM_FN(sms_teddyboyjp)

struct BurnDriver BurnDrvsms_teddyboyjp = {
	"sms_teddyboyjp", "sms_teddyboy", NULL, NULL, "1985",
	"Teddy Boy Blues (Jpn, Ep-MyCard, Prototype)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_teddyboyjpRomInfo, sms_teddyboyjpRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Teddy Boy Blues (Jpn, MyCard)

static struct BurnRomInfo sms_teddyboyjRomDesc[] = {
	{ "teddy boy blues (japan).bin",	0x08000, 0x316727dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_teddyboyj)
STD_ROM_FN(sms_teddyboyj)

struct BurnDriver BurnDrvsms_teddyboyj = {
	"sms_teddyboyj", "sms_teddyboy", NULL, NULL, "1985",
	"Teddy Boy Blues (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_teddyboyjRomInfo, sms_teddyboyjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// TransBot (Euro, USA, Bra, Sega Card)

static struct BurnRomInfo sms_transbotcRomDesc[] = {
	{ "transbot (euro, usa, bra).bin",	0x08000, 0x4bc42857, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_transbotc)
STD_ROM_FN(sms_transbotc)

struct BurnDriver BurnDrvsms_transbotc = {
	"sms_transbotc", "sms_transbot", NULL, NULL, "1985",
	"TransBot (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_transbotcRomInfo, sms_transbotcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Woody Pop - Shinjinrui no Block Kuzushi (Jpn, MyCard)

static struct BurnRomInfo sms_woodypopRomDesc[] = {
	{ "woody pop - shinjinrui no block kuzushi (japan).bin",	0x08000, 0x315917d4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_woodypop)
STD_ROM_FN(sms_woodypop)

struct BurnDriver BurnDrvsms_woodypop = {
	"sms_woodypop", NULL, NULL, NULL, "1987",
	"Woody Pop - Shinjinrui no Block Kuzushi (Jpn, MyCard)\0", NULL, "Sega", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_woodypopRomInfo, sms_woodypopRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// 5 in 1 Funpak (USA)

static struct BurnRomInfo gg_5in1funRomDesc[] = {
	{ "5 in 1 funpak (usa).bin",	0x40000, 0xf85a8ce8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_5in1fun)
STD_ROM_FN(gg_5in1fun)

struct BurnDriver BurnDrvgg_5in1fun = {
	"gg_5in1fun", NULL, NULL, NULL, "1994",
	"5 in 1 Funpak (USA)\0", NULL, "Interplay", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_5in1funRomInfo, gg_5in1funRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aa Harimanada (Jpn)

static struct BurnRomInfo gg_aaharimaRomDesc[] = {
	{ "aa harimanada (japan).bin",	0x80000, 0x1d17d2a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aaharima)
STD_ROM_FN(gg_aaharima)

struct BurnDriver BurnDrvgg_aaharima = {
	"gg_aaharima", NULL, NULL, NULL, "1993",
	"Aa Harimanada (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aaharimaRomInfo, gg_aaharimaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Addams Family (World)

static struct BurnRomInfo gg_addfamRomDesc[] = {
	{ "mpr-16234-f.ic1",	0x40000, 0x1d01f999, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_addfam)
STD_ROM_FN(gg_addfam)

struct BurnDriver BurnDrvgg_addfam = {
	"gg_addfam", NULL, NULL, NULL, "1993",
	"The Addams Family (World)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_addfamRomInfo, gg_addfamRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Adventures of Batman and Robin (Euro, USA)

static struct BurnRomInfo gg_advbatmrRomDesc[] = {
	{ "adventures of batman and robin, the (usa, europe).bin",	0x80000, 0xbb4f23ff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_advbatmr)
STD_ROM_FN(gg_advbatmr)

struct BurnDriver BurnDrvgg_advbatmr = {
	"gg_advbatmr", NULL, NULL, NULL, "1995",
	"The Adventures of Batman and Robin (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_advbatmrRomInfo, gg_advbatmrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aerial Assault (Jpn, v1)

static struct BurnRomInfo gg_aerialasjRomDesc[] = {
	{ "aerial assault (japan) (v1.1).bin",	0x20000, 0x04fe6dde, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aerialasj)
STD_ROM_FN(gg_aerialasj)

struct BurnDriver BurnDrvgg_aerialasj = {
	"gg_aerialasj", "gg_aerialas", NULL, NULL, "1992",
	"Aerial Assault (Jpn, v1)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aerialasjRomInfo, gg_aerialasjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Aerial Assault (World, v0)

static struct BurnRomInfo gg_aerialasRomDesc[] = {
	{ "aerial assault (world).bin",	0x20000, 0x3e549b7a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aerialas)
STD_ROM_FN(gg_aerialas)

struct BurnDriver BurnDrvgg_aerialas = {
	"gg_aerialas", NULL, NULL, NULL, "1992",
	"Aerial Assault (World, v0)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aerialasRomInfo, gg_aerialasRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Aladdin (Jpn)

static struct BurnRomInfo gg_aladdinjRomDesc[] = {
	{ "aladdin (japan).bin",	0x80000, 0x770e95e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aladdinj)
STD_ROM_FN(gg_aladdinj)

struct BurnDriver BurnDrvgg_aladdinj = {
	"gg_aladdinj", "gg_aladdin", NULL, NULL, "1994",
	"Disney's Aladdin (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aladdinjRomInfo, gg_aladdinjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Aladdin (Euro, USA, Bra)

static struct BurnRomInfo gg_aladdinRomDesc[] = {
	{ "aladdin (usa, europe).bin",	0x80000, 0x7a41c1dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aladdin)
STD_ROM_FN(gg_aladdin)

struct BurnDriver BurnDrvgg_aladdin = {
	"gg_aladdin", NULL, NULL, NULL, "1994",
	"Disney's Aladdin (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aladdinRomInfo, gg_aladdinRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Aladdin (Prototype, 19931229)

static struct BurnRomInfo gg_aladdinp3RomDesc[] = {
	{ "aladdin (prototype - dec 29,  1993).bin",	0x80000, 0x5b1f717f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aladdinp3)
STD_ROM_FN(gg_aladdinp3)

struct BurnDriver BurnDrvgg_aladdinp3 = {
	"gg_aladdinp3", "gg_aladdin", NULL, NULL, "1993",
	"Disney's Aladdin (Prototype, 19931229)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aladdinp3RomInfo, gg_aladdinp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Aladdin (Prototype, 19940111)

static struct BurnRomInfo gg_aladdinp2RomDesc[] = {
	{ "aladdin (prototype - jan 11,  1994).bin",	0x80000, 0x8a1693ab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aladdinp2)
STD_ROM_FN(gg_aladdinp2)

struct BurnDriver BurnDrvgg_aladdinp2 = {
	"gg_aladdinp2", "gg_aladdin", NULL, NULL, "1994",
	"Disney's Aladdin (Prototype, 19940111)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aladdinp2RomInfo, gg_aladdinp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Aladdin (Prototype, 19940113)

static struct BurnRomInfo gg_aladdinp1RomDesc[] = {
	{ "aladdin (prototype - jan 13,  1994).bin",	0x80000, 0x75a7f142, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aladdinp1)
STD_ROM_FN(gg_aladdinp1)

struct BurnDriver BurnDrvgg_aladdinp1 = {
	"gg_aladdinp1", "gg_aladdin", NULL, NULL, "1994",
	"Disney's Aladdin (Prototype, 19940113)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aladdinp1RomInfo, gg_aladdinp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alex Kidd in Miracle World (Tw, SMS Mode)

static struct BurnRomInfo gg_alexkiddRomDesc[] = {
	{ "alex kidd in miracle world [sms-gg] (tw).bin",	0x20000, 0x6f8e46cf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_alexkidd)
STD_ROM_FN(gg_alexkidd)

struct BurnDriver BurnDrvgg_alexkidd = {
	"gg_alexkidd", NULL, NULL, NULL, "198?",
	"Alex Kidd in Miracle World (Tw, SMS Mode)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE | HARDWARE_SMS_JAPANESE, GBF_MISC, 0,
	GGGetZipName, gg_alexkiddRomInfo, gg_alexkiddRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien� (Euro, USA)

static struct BurnRomInfo gg_alien3RomDesc[] = {
	{ "alien 3 (usa, europe).bin",	0x40000, 0x11a68c08, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_alien3)
STD_ROM_FN(gg_alien3)

struct BurnDriver BurnDrvgg_alien3 = {
	"gg_alien3", NULL, NULL, NULL, "1992",
	"Alien 3 (Euro, USA)\0", NULL, "Arena", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_alien3RomInfo, gg_alien3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien� (Jpn)

static struct BurnRomInfo gg_alien3jRomDesc[] = {
	{ "alien 3 (jp).bin",	0x40000, 0x06f6eebb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_alien3j)
STD_ROM_FN(gg_alien3j)

struct BurnDriver BurnDrvgg_alien3j = {
	"gg_alien3j", "gg_alien3", NULL, NULL, "1992",
	"Alien 3 (Jpn)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_alien3jRomInfo, gg_alien3jRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien Syndrome (Jpn)

static struct BurnRomInfo gg_aliensynjRomDesc[] = {
	{ "alien syndrome (japan).bin",	0x20000, 0xffe4ed47, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aliensynj)
STD_ROM_FN(gg_aliensynj)

struct BurnDriver BurnDrvgg_aliensynj = {
	"gg_aliensynj", "gg_aliensyn", NULL, NULL, "1992",
	"Alien Syndrome (Jpn)\0", NULL, "SIMS", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aliensynjRomInfo, gg_aliensynjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Alien Syndrome (Euro, USA)

static struct BurnRomInfo gg_aliensynRomDesc[] = {
	{ "alien syndrome (usa, europe).bin",	0x20000, 0xbb5e10e0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_aliensyn)
STD_ROM_FN(gg_aliensyn)

struct BurnDriver BurnDrvgg_aliensyn = {
	"gg_aliensyn", NULL, NULL, NULL, "1992",
	"Alien Syndrome (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_aliensynRomInfo, gg_aliensynRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Andre Agassi Tennis (USA)

static struct BurnRomInfo gg_agassiRomDesc[] = {
	{ "andre agassi tennis (usa).bin",	0x40000, 0x46c9fa3e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_agassi)
STD_ROM_FN(gg_agassi)

struct BurnDriver BurnDrvgg_agassi = {
	"gg_agassi", NULL, NULL, NULL, "1993",
	"Andre Agassi Tennis (USA)\0", NULL, "TecMagik", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_agassiRomInfo, gg_agassiRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Arcade Classics (USA)

static struct BurnRomInfo gg_arcadeclRomDesc[] = {
	{ "arcade classics (usa).bin",	0x40000, 0x3deca813, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_arcadecl)
STD_ROM_FN(gg_arcadecl)

struct BurnDriver BurnDrvgg_arcadecl = {
	"gg_arcadecl", NULL, NULL, NULL, "1996",
	"Arcade Classics (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_arcadeclRomInfo, gg_arcadeclRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Arch Rivals (USA)

static struct BurnRomInfo gg_archrivlRomDesc[] = {
	{ "arch rivals (usa).bin",	0x40000, 0xf0204191, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_archrivl)
STD_ROM_FN(gg_archrivl)

struct BurnDriver BurnDrvgg_archrivl = {
	"gg_archrivl", NULL, NULL, NULL, "1992",
	"Arch Rivals (USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_archrivlRomInfo, gg_archrivlRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Arena (Euro, USA)

static struct BurnRomInfo gg_arenaRomDesc[] = {
	{ "arena (usa, europe).bin",	0x80000, 0x7cb3facf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_arena)
STD_ROM_FN(gg_arena)

struct BurnDriver BurnDrvgg_arena = {
	"gg_arena", NULL, NULL, NULL, "1995",
	"Arena (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_arenaRomInfo, gg_arenaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Ariel the Little Mermaid (Euro, USA)

static struct BurnRomInfo gg_arielRomDesc[] = {
	{ "mpr-15394-f.ic1",	0x40000, 0x97e3a18c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ariel)
STD_ROM_FN(gg_ariel)

struct BurnDriver BurnDrvgg_ariel = {
	"gg_ariel", NULL, NULL, NULL, "1991",
	"Disney's Ariel the Little Mermaid (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_arielRomInfo, gg_arielRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Arliel - Crystal Densetsu (Jpn)

static struct BurnRomInfo gg_arlielRomDesc[] = {
	{ "mpr-14389.ic1",	0x40000, 0x35fa3f68, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_arliel)
STD_ROM_FN(gg_arliel)

struct BurnDriver BurnDrvgg_arliel = {
	"gg_arliel", "gg_crystalw", NULL, NULL, "1991",
	"Arliel - Crystal Densetsu (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_arlielRomInfo, gg_arlielRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (Euro)

static struct BurnRomInfo gg_astergreRomDesc[] = {
	{ "mpr-16718.ic1",	0x80000, 0x328c5cc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astergre)
STD_ROM_FN(gg_astergre)

struct BurnDriver BurnDrvgg_astergre = {
	"gg_astergre", NULL, NULL, NULL, "1993",
	"Asterix and the Great Rescue (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astergreRomInfo, gg_astergreRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (USA)

static struct BurnRomInfo gg_astergreuRomDesc[] = {
	{ "asterix and the great rescue (usa).bin",	0x80000, 0x78208b40, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astergreu)
STD_ROM_FN(gg_astergreu)

struct BurnDriver BurnDrvgg_astergreu = {
	"gg_astergreu", "gg_astergre", NULL, NULL, "1994",
	"Asterix and the Great Rescue (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astergreuRomInfo, gg_astergreuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (Prototype, 19940216)

static struct BurnRomInfo gg_astergrep5RomDesc[] = {
	{ "asterix (prototype - feb 16,  1994).bin",	0x80000, 0x1e8d735c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astergrep5)
STD_ROM_FN(gg_astergrep5)

struct BurnDriver BurnDrvgg_astergrep5 = {
	"gg_astergrep5", "gg_astergre", NULL, NULL, "1994",
	"Asterix and the Great Rescue (Prototype, 19940216)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astergrep5RomInfo, gg_astergrep5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (Prototype, 19940222)

static struct BurnRomInfo gg_astergrep4RomDesc[] = {
	{ "asterix (prototype - feb 22,  1994).bin",	0x80000, 0x31bd3d8a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astergrep4)
STD_ROM_FN(gg_astergrep4)

struct BurnDriver BurnDrvgg_astergrep4 = {
	"gg_astergrep4", "gg_astergre", NULL, NULL, "1994",
	"Asterix and the Great Rescue (Prototype, 19940222)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astergrep4RomInfo, gg_astergrep4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (Prototype, 19940125)

static struct BurnRomInfo gg_astergrep3RomDesc[] = {
	{ "asterix (prototype - jan 25,  1994).bin",	0x80000, 0xba45f00f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astergrep3)
STD_ROM_FN(gg_astergrep3)

struct BurnDriver BurnDrvgg_astergrep3 = {
	"gg_astergrep3", "gg_astergre", NULL, NULL, "1994",
	"Asterix and the Great Rescue (Prototype, 19940125)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astergrep3RomInfo, gg_astergrep3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (Prototype, 19940313)

static struct BurnRomInfo gg_astergrep2RomDesc[] = {
	{ "asterix (prototype - mar 13,  1994).bin",	0x80000, 0xfaf09846, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astergrep2)
STD_ROM_FN(gg_astergrep2)

struct BurnDriver BurnDrvgg_astergrep2 = {
	"gg_astergrep2", "gg_astergre", NULL, NULL, "1994",
	"Asterix and the Great Rescue (Prototype, 19940313)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astergrep2RomInfo, gg_astergrep2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Astérix and the Great Rescue (Prototype, 19940315)

static struct BurnRomInfo gg_astergrep1RomDesc[] = {
	{ "asterix (prototype - mar 15,  1994).bin",	0x80000, 0x66d8eb63, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astergrep1)
STD_ROM_FN(gg_astergrep1)

struct BurnDriver BurnDrvgg_astergrep1 = {
	"gg_astergrep1", "gg_astergre", NULL, NULL, "1994",
	"Asterix and the Great Rescue (Prototype, 19940315)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astergrep1RomInfo, gg_astergrep1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Asterix and the Secret Mission (Euro)

static struct BurnRomInfo gg_astermisRomDesc[] = {
	{ "asterix and the secret mission (europe) (en,fr,de).bin",	0x80000, 0xc01a8051, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_astermis)
STD_ROM_FN(gg_astermis)

struct BurnDriver BurnDrvgg_astermis = {
	"gg_astermis", NULL, NULL, NULL, "1993",
	"Asterix and the Secret Mission (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_astermisRomInfo, gg_astermisRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ax Battler - A Legend of Golden Axe (Euro, USA, v2.4)

static struct BurnRomInfo gg_axbattlrRomDesc[] = {
	{ "ax battler - a legend of golden axe (usa, europe) (v2.4).bin",	0x40000, 0x663bcf8a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_axbattlr)
STD_ROM_FN(gg_axbattlr)

struct BurnDriver BurnDrvgg_axbattlr = {
	"gg_axbattlr", NULL, NULL, NULL, "1991",
	"Ax Battler - A Legend of Golden Axe (Euro, USA, v2.4)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_axbattlrRomInfo, gg_axbattlrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ax Battler - A Legend of Golden Axe (Euro, USA, Prototype v2.0)

static struct BurnRomInfo gg_axbattlrpRomDesc[] = {
	{ "ax battler - a legend of golden axe [v2.0] [proto].bin",	0x40000, 0x3435ab54, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_axbattlrp)
STD_ROM_FN(gg_axbattlrp)

struct BurnDriver BurnDrvgg_axbattlrp = {
	"gg_axbattlrp", "gg_axbattlr", NULL, NULL, "1991",
	"Ax Battler - A Legend of Golden Axe (Euro, USA, Prototype v2.0)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_axbattlrpRomInfo, gg_axbattlrpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ax Battler - Golden Axe Densetsu (Jpn, v1.5)

static struct BurnRomInfo gg_axbattlrjRomDesc[] = {
	{ "ax battler - golden axe densetsu (japan) (v1.5).bin",	0x40000, 0xdfcf555f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_axbattlrj)
STD_ROM_FN(gg_axbattlrj)

struct BurnDriver BurnDrvgg_axbattlrj = {
	"gg_axbattlrj", "gg_axbattlr", NULL, NULL, "1991",
	"Ax Battler - Golden Axe Densetsu (Jpn, v1.5)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_axbattlrjRomInfo, gg_axbattlrjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ayrton Senna's Super Monaco GP II (Jpn)

static struct BurnRomInfo gg_smgp2jRomDesc[] = {
	{ "mpr-15044.ic1",	0x40000, 0x661faeff, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smgp2j)
STD_ROM_FN(gg_smgp2j)

struct BurnDriver BurnDrvgg_smgp2j = {
	"gg_smgp2j", "gg_smgp2", NULL, NULL, "1992",
	"Ayrton Senna's Super Monaco GP II (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smgp2jRomInfo, gg_smgp2jRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ayrton Senna's Super Monaco GP II (Euro, USA)

static struct BurnRomInfo gg_smgp2RomDesc[] = {
	{ "mpr-15049-f.ic1",	0x40000, 0x1d1b1dd3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smgp2)
STD_ROM_FN(gg_smgp2)

struct BurnDriver BurnDrvgg_smgp2 = {
	"gg_smgp2", NULL, NULL, NULL, "1992",
	"Ayrton Senna's Super Monaco GP II (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smgp2RomInfo, gg_smgp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ayrton Senna's Super Monaco GP II (Euro, USA, Prototype)

static struct BurnRomInfo gg_smgp2pRomDesc[] = {
	{ "ayrton senna's super monaco gp ii (proto).bin",	0x40000, 0x6afb8ec6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smgp2p)
STD_ROM_FN(gg_smgp2p)

struct BurnDriver BurnDrvgg_smgp2p = {
	"gg_smgp2p", "gg_smgp2", NULL, NULL, "1992",
	"Ayrton Senna's Super Monaco GP II (Euro, USA, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smgp2pRomInfo, gg_smgp2pRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Baku Baku Animal (Euro)

static struct BurnRomInfo gg_bakubakuRomDesc[] = {
	{ "baku baku animal (europe).bin",	0x40000, 0x10ac9374, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bakubaku)
STD_ROM_FN(gg_bakubaku)

struct BurnDriver BurnDrvgg_bakubaku = {
	"gg_bakubaku", NULL, NULL, NULL, "1996",
	"Baku Baku Animal (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bakubakuRomInfo, gg_bakubakuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Baku Baku Animal (USA)

static struct BurnRomInfo gg_bakubakuuRomDesc[] = {
	{ "baku baku animal (usa).bin",	0x40000, 0x8d8bfdc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bakubakuu)
STD_ROM_FN(gg_bakubakuu)

struct BurnDriver BurnDrvgg_bakubakuu = {
	"gg_bakubakuu", "gg_bakubaku", NULL, NULL, "1996",
	"Baku Baku Animal (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bakubakuuRomInfo, gg_bakubakuuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Baku Baku Animal - Sekai Shiikugakari Senshu-ken (Jpn)

static struct BurnRomInfo gg_bakubakujRomDesc[] = {
	{ "mpr-18706-s.ic1",	0x40000, 0x7174b802, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bakubakuj)
STD_ROM_FN(gg_bakubakuj)

struct BurnDriver BurnDrvgg_bakubakuj = {
	"gg_bakubakuj", "gg_bakubaku", NULL, NULL, "1996",
	"Baku Baku Animal - Sekai Shiikugakari Senshu-ken (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bakubakujRomInfo, gg_bakubakujRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Barbie Super Model (Prototype)

static struct BurnRomInfo gg_barbieRomDesc[] = {
	{ "barbie super model (unknown) (proto).bin",	0x40000, 0x03a0ce9e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_barbie)
STD_ROM_FN(gg_barbie)

struct BurnDriver BurnDrvgg_barbie = {
	"gg_barbie", NULL, NULL, NULL, "199?",
	"Barbie Super Model (Prototype)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_barbieRomInfo, gg_barbieRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Simpsons - Bartman Meets Radioactive Man (USA)

static struct BurnRomInfo gg_bartmanRomDesc[] = {
	{ "mpr-16373.ic1",	0x40000, 0xffa447a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bartman)
STD_ROM_FN(gg_bartman)

struct BurnDriver BurnDrvgg_bartman = {
	"gg_bartman", NULL, NULL, NULL, "1992",
	"The Simpsons - Bartman Meets Radioactive Man (USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bartmanRomInfo, gg_bartmanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Batman Forever (World)

static struct BurnRomInfo gg_batmanfRomDesc[] = {
	{ "batman forever (world).bin",	0x80000, 0x618b19e2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_batmanf)
STD_ROM_FN(gg_batmanf)

struct BurnDriver BurnDrvgg_batmanf = {
	"gg_batmanf", NULL, NULL, NULL, "1995",
	"Batman Forever (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_batmanfRomInfo, gg_batmanfRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Batman Returns (World)

static struct BurnRomInfo gg_batmanrnRomDesc[] = {
	{ "epoxy.u1",	0x40000, 0x7ac4a3ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_batmanrn)
STD_ROM_FN(gg_batmanrn)

struct BurnDriver BurnDrvgg_batmanrn = {
	"gg_batmanrn", NULL, NULL, NULL, "1992",
	"Batman Returns (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_batmanrnRomInfo, gg_batmanrnRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Batter Up (Euro, USA)

static struct BurnRomInfo gg_batterupRomDesc[] = {
	{ "batter up (usa, europe).bin",	0x20000, 0x16448209, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_batterup)
STD_ROM_FN(gg_batterup)

struct BurnDriver BurnDrvgg_batterup = {
	"gg_batterup", NULL, NULL, NULL, "1991",
	"Batter Up (Euro, USA)\0", NULL, "Namco", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_batterupRomInfo, gg_batterupRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Battleship (Euro, USA)

static struct BurnRomInfo gg_bshipRomDesc[] = {
	{ "battleship (usa, europe).bin",	0x10000, 0xfddd8cd9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bship)
STD_ROM_FN(gg_bship)

struct BurnDriver BurnDrvgg_bship = {
	"gg_bship", NULL, NULL, NULL, "1993",
	"Battleship (Euro, USA)\0", NULL, "Mindscape", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bshipRomInfo, gg_bshipRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Battletoads (Euro, Jpn)

static struct BurnRomInfo gg_btoadsRomDesc[] = {
	{ "battletoads (japan, europe).bin",	0x40000, 0xcb3cd075, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_btoads)
STD_ROM_FN(gg_btoads)

struct BurnDriver BurnDrvgg_btoads = {
	"gg_btoads", NULL, NULL, NULL, "1993",
	"Battletoads (Euro, Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_btoadsRomInfo, gg_btoadsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Battletoads (USA)

static struct BurnRomInfo gg_btoadsuRomDesc[] = {
	{ "battletoads (usa).bin",	0x40000, 0x817cc0ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_btoadsu)
STD_ROM_FN(gg_btoadsu)

struct BurnDriver BurnDrvgg_btoadsu = {
	"gg_btoadsu", "gg_btoads", NULL, NULL, "1993",
	"Battletoads (USA)\0", NULL, "Tradewest", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_btoadsuRomInfo, gg_btoadsuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Beavis and Butt-head (Euro, USA)

static struct BurnRomInfo gg_beavisRomDesc[] = {
	{ "beavis and butt-head (europe).bin",	0x80000, 0xa6bf865e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_beavis)
STD_ROM_FN(gg_beavis)

struct BurnDriver BurnDrvgg_beavis = {
	"gg_beavis", NULL, NULL, NULL, "1994",
	"Beavis and Butt-head (Euro, USA)\0", NULL, "Viacom New Media", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_beavisRomInfo, gg_beavisRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Beavis and Butt-head (Prototype?)

static struct BurnRomInfo gg_beavispRomDesc[] = {
	{ "beavis and butt-head (usa).bin",	0x80000, 0x3858f14f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_beavisp)
STD_ROM_FN(gg_beavisp)

struct BurnDriver BurnDrvgg_beavisp = {
	"gg_beavisp", "gg_beavis", NULL, NULL, "1994",
	"Beavis and Butt-head (Prototype?)\0", NULL, "Viacom New Media", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_beavispRomInfo, gg_beavispRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (USA)

static struct BurnRomInfo gg_berensRomDesc[] = {
	{ "berenstain bears' camping adventure, the (usa).bin",	0x40000, 0xa4bb9ffb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berens)
STD_ROM_FN(gg_berens)

struct BurnDriver BurnDrvgg_berens = {
	"gg_berens", NULL, NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensRomInfo, gg_berensRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940801)

static struct BurnRomInfo gg_berensp12RomDesc[] = {
	{ "berenstain bears (prototype - aug 01,  1994).bin",	0x40000, 0x882b8f53, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp12)
STD_ROM_FN(gg_berensp12)

struct BurnDriver BurnDrvgg_berensp12 = {
	"gg_berensp12", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940801)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp12RomInfo, gg_berensp12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940811-A)

static struct BurnRomInfo gg_berensp11RomDesc[] = {
	{ "berenstain bears (prototype - aug 11,  1994 - a).bin",	0x40000, 0x81a5e68f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp11)
STD_ROM_FN(gg_berensp11)

struct BurnDriver BurnDrvgg_berensp11 = {
	"gg_berensp11", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940811-A)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp11RomInfo, gg_berensp11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940811)

static struct BurnRomInfo gg_berensp10RomDesc[] = {
	{ "berenstain bears (prototype - aug 11,  1994).bin",	0x40000, 0x50d400b8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp10)
STD_ROM_FN(gg_berensp10)

struct BurnDriver BurnDrvgg_berensp10 = {
	"gg_berensp10", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940811)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp10RomInfo, gg_berensp10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940812)

static struct BurnRomInfo gg_berensp09RomDesc[] = {
	{ "berenstain bears (prototype - aug 12,  1994).bin",	0x40000, 0x0e676927, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp09)
STD_ROM_FN(gg_berensp09)

struct BurnDriver BurnDrvgg_berensp09 = {
	"gg_berensp09", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940812)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp09RomInfo, gg_berensp09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940813)

static struct BurnRomInfo gg_berensp08RomDesc[] = {
	{ "berenstain bears (prototype - aug 13,  1994).bin",	0x40000, 0xf8f11b2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp08)
STD_ROM_FN(gg_berensp08)

struct BurnDriver BurnDrvgg_berensp08 = {
	"gg_berensp08", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940813)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp08RomInfo, gg_berensp08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940814)

static struct BurnRomInfo gg_berensp07RomDesc[] = {
	{ "berenstain bears (prototype - aug 14,  1994).bin",	0x40000, 0xd0971f78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp07)
STD_ROM_FN(gg_berensp07)

struct BurnDriver BurnDrvgg_berensp07 = {
	"gg_berensp07", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940814)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp07RomInfo, gg_berensp07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940819)

static struct BurnRomInfo gg_berensp06RomDesc[] = {
	{ "berenstain bears (prototype - aug 19,  1994).bin",	0x40000, 0x177d65ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp06)
STD_ROM_FN(gg_berensp06)

struct BurnDriver BurnDrvgg_berensp06 = {
	"gg_berensp06", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940819)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp06RomInfo, gg_berensp06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940820)

static struct BurnRomInfo gg_berensp05RomDesc[] = {
	{ "berenstain bears (prototype - aug 20,  1994).bin",	0x40000, 0xf61ac1e8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp05)
STD_ROM_FN(gg_berensp05)

struct BurnDriver BurnDrvgg_berensp05 = {
	"gg_berensp05", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940820)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp05RomInfo, gg_berensp05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940824)

static struct BurnRomInfo gg_berensp04RomDesc[] = {
	{ "berenstain bears (prototype - aug 24,  1994).bin",	0x40000, 0xb73486d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp04)
STD_ROM_FN(gg_berensp04)

struct BurnDriver BurnDrvgg_berensp04 = {
	"gg_berensp04", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940824)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp04RomInfo, gg_berensp04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940825-B)

static struct BurnRomInfo gg_berensp03RomDesc[] = {
	{ "berenstain bears (prototype - aug 25,  1994 - b).bin",	0x40000, 0xe07b865e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp03)
STD_ROM_FN(gg_berensp03)

struct BurnDriver BurnDrvgg_berensp03 = {
	"gg_berensp03", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940825-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp03RomInfo, gg_berensp03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940826-B)

static struct BurnRomInfo gg_berensp02RomDesc[] = {
	{ "berenstain bears (prototype - aug 26,  1994 - b).bin",	0x40000, 0x217b1ef0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp02)
STD_ROM_FN(gg_berensp02)

struct BurnDriver BurnDrvgg_berensp02 = {
	"gg_berensp02", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940826-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp02RomInfo, gg_berensp02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940826)

static struct BurnRomInfo gg_berensp01RomDesc[] = {
	{ "berenstain bears (prototype - aug 26,  1994).bin",	0x40000, 0x167a4af6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp01)
STD_ROM_FN(gg_berensp01)

struct BurnDriver BurnDrvgg_berensp01 = {
	"gg_berensp01", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940826)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp01RomInfo, gg_berensp01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940715-B)

static struct BurnRomInfo gg_berensp16RomDesc[] = {
	{ "berenstain bears (prototype - jul 15,  1994 - b).bin",	0x40000, 0xfcf47bc9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp16)
STD_ROM_FN(gg_berensp16)

struct BurnDriver BurnDrvgg_berensp16 = {
	"gg_berensp16", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940715-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp16RomInfo, gg_berensp16RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940721)

static struct BurnRomInfo gg_berensp15RomDesc[] = {
	{ "berenstain bears (prototype - jul 21,  1994).bin",	0x40000, 0xa279c6bc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp15)
STD_ROM_FN(gg_berensp15)

struct BurnDriver BurnDrvgg_berensp15 = {
	"gg_berensp15", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940721)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp15RomInfo, gg_berensp15RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940722)

static struct BurnRomInfo gg_berensp14RomDesc[] = {
	{ "berenstain bears (prototype - jul 22,  1994).bin",	0x40000, 0xffbf5abe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp14)
STD_ROM_FN(gg_berensp14)

struct BurnDriver BurnDrvgg_berensp14 = {
	"gg_berensp14", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940722)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp14RomInfo, gg_berensp14RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940727)

static struct BurnRomInfo gg_berensp13RomDesc[] = {
	{ "berenstain bears (prototype - jul 27,  1994).bin",	0x40000, 0x112b123f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp13)
STD_ROM_FN(gg_berensp13)

struct BurnDriver BurnDrvgg_berensp13 = {
	"gg_berensp13", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940727)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp13RomInfo, gg_berensp13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Berenstain Bears' Camping Adventure (Prototype, 19940628)

static struct BurnRomInfo gg_berensp17RomDesc[] = {
	{ "berenstain bears (prototype - jun 28,  1994).bin",	0x40000, 0x75a7a8ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berensp17)
STD_ROM_FN(gg_berensp17)

struct BurnDriver BurnDrvgg_berensp17 = {
	"gg_berensp17", "gg_berens", NULL, NULL, "1994",
	"The Berenstain Bears' Camping Adventure (Prototype, 19940628)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berensp17RomInfo, gg_berensp17RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Berlin no Kabe - The Berlin Wall (Jpn)

static struct BurnRomInfo gg_berlinRomDesc[] = {
	{ "berlin no kabe (japan).bin",	0x40000, 0x325b1797, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_berlin)
STD_ROM_FN(gg_berlin)

struct BurnDriver BurnDrvgg_berlin = {
	"gg_berlin", NULL, NULL, NULL, "1991",
	"Berlin no Kabe - The Berlin Wall (Jpn)\0", NULL, "Kaneko", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_berlinRomInfo, gg_berlinRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bishoujo Senshi Sailor Moon S (Jpn)

static struct BurnRomInfo gg_sailormsRomDesc[] = {
	{ "mpr-17715.ic1",	0x80000, 0xfe7374d2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sailorms)
STD_ROM_FN(gg_sailorms)

struct BurnDriver BurnDrvgg_sailorms = {
	"gg_sailorms", NULL, NULL, NULL, "1995",
	"Bishoujo Senshi Sailor Moon S (Jpn)\0", NULL, "Bandai", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sailormsRomInfo, gg_sailormsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Bonkers Wax Up! (Euro, USA)

static struct BurnRomInfo gg_bonkersRomDesc[] = {
	{ "bonkers wax up! (usa, europe).bin",	0x80000, 0xbfceba5f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkers)
STD_ROM_FN(gg_bonkers)

struct BurnDriver BurnDrvgg_bonkers = {
	"gg_bonkers", NULL, NULL, NULL, "1995",
	"Disney's Bonkers Wax Up! (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersRomInfo, gg_bonkersRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disneys Bonkers Wax Up! (Prototype, 19941203)

static struct BurnRomInfo gg_bonkersp07RomDesc[] = {
	{ "bonkers wax up! (prototype - dec 03,  1994).bin",	0x80000, 0x63429025, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp07)
STD_ROM_FN(gg_bonkersp07)

struct BurnDriver BurnDrvgg_bonkersp07 = {
	"gg_bonkersp07", "gg_bonkers", NULL, NULL, "1994",
	"Disneys Bonkers Wax Up! (Prototype, 19941203)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp07RomInfo, gg_bonkersp07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disneys Bonkers Wax Up! (Prototype, 19941205)

static struct BurnRomInfo gg_bonkersp06RomDesc[] = {
	{ "bonkers wax up! (prototype - dec 05,  1994).bin",	0x80000, 0xc2229b6e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp06)
STD_ROM_FN(gg_bonkersp06)

struct BurnDriver BurnDrvgg_bonkersp06 = {
	"gg_bonkersp06", "gg_bonkers", NULL, NULL, "1994",
	"Disneys Bonkers Wax Up! (Prototype, 19941205)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp06RomInfo, gg_bonkersp06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Bonkers Wax Up! (Prototype, 19941207-C)

static struct BurnRomInfo gg_bonkersp05RomDesc[] = {
	{ "bonkers wax up! (prototype - dec 07,  1994 - c).bin",	0x80000, 0xedaf25e7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp05)
STD_ROM_FN(gg_bonkersp05)

struct BurnDriver BurnDrvgg_bonkersp05 = {
	"gg_bonkersp05", "gg_bonkers", NULL, NULL, "1994",
	"Disney's Bonkers Wax Up! (Prototype, 19941207-C)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp05RomInfo, gg_bonkersp05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Bonkers Wax Up! (Prototype, 19941208-B)

static struct BurnRomInfo gg_bonkersp04RomDesc[] = {
	{ "bonkers wax up! (prototype - dec 08,  1994 - b).bin",	0x80000, 0x9907a738, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp04)
STD_ROM_FN(gg_bonkersp04)

struct BurnDriver BurnDrvgg_bonkersp04 = {
	"gg_bonkersp04", "gg_bonkers", NULL, NULL, "1994",
	"Disney's Bonkers Wax Up! (Prototype, 19941208-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp04RomInfo, gg_bonkersp04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Bonkers Wax Up! (Prototype, 19941208-C)

static struct BurnRomInfo gg_bonkersp03RomDesc[] = {
	{ "bonkers wax up! (prototype - dec 08,  1994 - c).bin",	0x80000, 0x91258adc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp03)
STD_ROM_FN(gg_bonkersp03)

struct BurnDriver BurnDrvgg_bonkersp03 = {
	"gg_bonkersp03", "gg_bonkers", NULL, NULL, "1994",
	"Disney's Bonkers Wax Up! (Prototype, 19941208-C)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp03RomInfo, gg_bonkersp03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Bonkers Wax Up! (Prototype, 19941211)

static struct BurnRomInfo gg_bonkersp02RomDesc[] = {
	{ "bonkers wax up! (prototype - dec 11,  1994).bin",	0x80000, 0x9a405d0d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp02)
STD_ROM_FN(gg_bonkersp02)

struct BurnDriver BurnDrvgg_bonkersp02 = {
	"gg_bonkersp02", "gg_bonkers", NULL, NULL, "1994",
	"Disney's Bonkers Wax Up! (Prototype, 19941211)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp02RomInfo, gg_bonkersp02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's Bonkers Wax Up! (Prototype, 19941212-D)

static struct BurnRomInfo gg_bonkersp01RomDesc[] = {
	{ "bonkers wax up! (prototype - dec 12,  1994 - d).bin",	0x80000, 0x6d295894, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp01)
STD_ROM_FN(gg_bonkersp01)

struct BurnDriver BurnDrvgg_bonkersp01 = {
	"gg_bonkersp01", "gg_bonkers", NULL, NULL, "1994",
	"Disney's Bonkers Wax Up! (Prototype, 19941212-D)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp01RomInfo, gg_bonkersp01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bonkers Wax Up! (Prototype, 19941115)

static struct BurnRomInfo gg_bonkersp12RomDesc[] = {
	{ "bonkers wax up! (prototype - nov 15,  1994).bin",	0x80000, 0x85510a57, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp12)
STD_ROM_FN(gg_bonkersp12)

struct BurnDriver BurnDrvgg_bonkersp12 = {
	"gg_bonkersp12", "gg_bonkers", NULL, NULL, "1994",
	"Bonkers Wax Up! (Prototype, 19941115)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp12RomInfo, gg_bonkersp12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bonkers Wax Up! (Prototype, 19941121)

static struct BurnRomInfo gg_bonkersp11RomDesc[] = {
	{ "bonkers wax up! (prototype - nov 21,  1994).bin",	0x80000, 0xdab43f8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp11)
STD_ROM_FN(gg_bonkersp11)

struct BurnDriver BurnDrvgg_bonkersp11 = {
	"gg_bonkersp11", "gg_bonkers", NULL, NULL, "1994",
	"Bonkers Wax Up! (Prototype, 19941121)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp11RomInfo, gg_bonkersp11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bonkers Wax Up! (Prototype, 19941123)

static struct BurnRomInfo gg_bonkersp10RomDesc[] = {
	{ "bonkers wax up! (prototype - nov 23,  1994).bin",	0x80000, 0x1b081950, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp10)
STD_ROM_FN(gg_bonkersp10)

struct BurnDriver BurnDrvgg_bonkersp10 = {
	"gg_bonkersp10", "gg_bonkers", NULL, NULL, "1994",
	"Bonkers Wax Up! (Prototype, 19941123)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp10RomInfo, gg_bonkersp10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bonkers Wax Up! (Prototype, 19941126)

static struct BurnRomInfo gg_bonkersp09RomDesc[] = {
	{ "bonkers wax up! (prototype - nov 26,  1994).bin",	0x80000, 0xeb3f22c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp09)
STD_ROM_FN(gg_bonkersp09)

struct BurnDriver BurnDrvgg_bonkersp09 = {
	"gg_bonkersp09", "gg_bonkers", NULL, NULL, "1994",
	"Bonkers Wax Up! (Prototype, 19941126)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp09RomInfo, gg_bonkersp09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bonkers Wax Up! (Prototype, 19941129)

static struct BurnRomInfo gg_bonkersp08RomDesc[] = {
	{ "bonkers wax up! (prototype - nov 29,  1994).bin",	0x80000, 0xad6a62fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp08)
STD_ROM_FN(gg_bonkersp08)

struct BurnDriver BurnDrvgg_bonkersp08 = {
	"gg_bonkersp08", "gg_bonkers", NULL, NULL, "1994",
	"Bonkers Wax Up! (Prototype, 19941129)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp08RomInfo, gg_bonkersp08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bonkers Wax Up! (Prototype, 19941031)

static struct BurnRomInfo gg_bonkersp13RomDesc[] = {
	{ "bonkers wax up! (prototype - oct 31,  1994).bin",	0x80000, 0x777dfd80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bonkersp13)
STD_ROM_FN(gg_bonkersp13)

struct BurnDriver BurnDrvgg_bonkersp13 = {
	"gg_bonkersp13", "gg_bonkers", NULL, NULL, "1994",
	"Bonkers Wax Up! (Prototype, 19941031)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bonkersp13RomInfo, gg_bonkersp13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bram Stoker's Dracula (Euro)

static struct BurnRomInfo gg_draculaRomDesc[] = {
	{ "bram stoker's dracula (usa).bin",	0x40000, 0x69ebe5fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dracula)
STD_ROM_FN(gg_dracula)

struct BurnDriver BurnDrvgg_dracula = {
	"gg_dracula", NULL, NULL, NULL, "1994",
	"Bram Stoker's Dracula (Euro)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_draculaRomInfo, gg_draculaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bram Stoker's Dracula (USA)

static struct BurnRomInfo gg_draculauRomDesc[] = {
	{ "dracula (bram stoker's) (us).bin",	0x40000, 0xd966ec47, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_draculau)
STD_ROM_FN(gg_draculau)

struct BurnDriver BurnDrvgg_draculau = {
	"gg_draculau", "gg_dracula", NULL, NULL, "1992",
	"Bram Stoker's Dracula (USA)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_draculauRomInfo, gg_draculauRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bubble Bobble (Euro, USA)

static struct BurnRomInfo gg_bublboblRomDesc[] = {
	{ "bubble bobble (usa, europe).bin",	0x40000, 0xfba338c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bublbobl)
STD_ROM_FN(gg_bublbobl)

struct BurnDriver BurnDrvgg_bublbobl = {
	"gg_bublbobl", NULL, NULL, NULL, "1994",
	"Bubble Bobble (Euro, USA)\0", NULL, "Taito", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bublboblRomInfo, gg_bublboblRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bugs Bunny in Double Trouble (Euro, USA)

static struct BurnRomInfo gg_bugsbunRomDesc[] = {
	{ "mpr-18846-s.ic1",	0x80000, 0x5c34d9cd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bugsbun)
STD_ROM_FN(gg_bugsbun)

struct BurnDriver BurnDrvgg_bugsbun = {
	"gg_bugsbun", NULL, NULL, NULL, "1996",
	"Bugs Bunny in Double Trouble (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bugsbunRomInfo, gg_bugsbunRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Bust-A-Move (Euro, USA)

static struct BurnRomInfo gg_bamRomDesc[] = {
	{ "bust-a-move (usa, europe).bin",	0x40000, 0xc90f29ef, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bam)
STD_ROM_FN(gg_bam)

struct BurnDriver BurnDrvgg_bam = {
	"gg_bam", NULL, NULL, NULL, "1995",
	"Bust-A-Move (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bamRomInfo, gg_bamRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Buster Ball (Jpn)

static struct BurnRomInfo gg_busterbRomDesc[] = {
	{ "buster ball (japan).bin",	0x20000, 0x7cb079d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_busterb)
STD_ROM_FN(gg_busterb)

struct BurnDriver BurnDrvgg_busterb = {
	"gg_busterb", NULL, NULL, NULL, "1992",
	"Buster Ball (Jpn)\0", NULL, "Riverhill Software", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_busterbRomInfo, gg_busterbRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Buster Fight (Jpn)

static struct BurnRomInfo gg_busterfRomDesc[] = {
	{ "buster fight (japan).bin",	0x40000, 0xa72a1753, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_busterf)
STD_ROM_FN(gg_busterf)

struct BurnDriver BurnDrvgg_busterf = {
	"gg_busterf", NULL, NULL, NULL, "1994",
	"Buster Fight (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_busterfRomInfo, gg_busterfRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Caesars Palace (USA)

static struct BurnRomInfo gg_caesarsRomDesc[] = {
	{ "caesars palace (usa).bin",	0x40000, 0xc53b60b8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_caesars)
STD_ROM_FN(gg_caesars)

struct BurnDriver BurnDrvgg_caesars = {
	"gg_caesars", NULL, NULL, NULL, "1993",
	"Caesars Palace (USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_caesarsRomInfo, gg_caesarsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Captain America and the Avengers (Euro, USA)

static struct BurnRomInfo gg_captavenRomDesc[] = {
	{ "captain america and the avengers (usa, europe).bin",	0x40000, 0x5675dfdd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_captaven)
STD_ROM_FN(gg_captaven)

struct BurnDriver BurnDrvgg_captaven = {
	"gg_captaven", NULL, NULL, NULL, "1993",
	"Captain America and the Avengers (Euro, USA)\0", NULL, "Mindscape", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_captavenRomInfo, gg_captavenRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Car Licence (Jpn)

static struct BurnRomInfo gg_carlicenRomDesc[] = {
	{ "car licence (japan).bin",	0x80000, 0xf6a697f8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_carlicen)
STD_ROM_FN(gg_carlicen)

struct BurnDriver BurnDrvgg_carlicen = {
	"gg_carlicen", NULL, NULL, NULL, "199?",
	"Car Licence (Jpn)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_carlicenRomInfo, gg_carlicenRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Casino Funpak (USA)

static struct BurnRomInfo gg_casinofnRomDesc[] = {
	{ "casino funpak (usa).bin",	0x40000, 0x2b732056, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_casinofn)
STD_ROM_FN(gg_casinofn)

struct BurnDriver BurnDrvgg_casinofn = {
	"gg_casinofn", NULL, NULL, NULL, "1995",
	"Casino Funpak (USA)\0", NULL, "Interplay", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_casinofnRomInfo, gg_casinofnRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Castle of Illusion Starring Mickey Mouse (Euro, USA, SMS Mode)

static struct BurnRomInfo gg_castlillRomDesc[] = {
	{ "mpr-13704-f.ic1",	0x40000, 0x59840fd6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_castlill)
STD_ROM_FN(gg_castlill)

struct BurnDriver BurnDrvgg_castlill = {
	"gg_castlill", NULL, NULL, NULL, "1991",
	"Castle of Illusion Starring Mickey Mouse (Euro, USA, SMS Mode)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_castlillRomInfo, gg_castlillRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chakan (Euro, USA)

static struct BurnRomInfo gg_chakanRomDesc[] = {
	{ "chakan (usa, europe).bin",	0x40000, 0xdfc7adc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chakan)
STD_ROM_FN(gg_chakan)

struct BurnDriver BurnDrvgg_chakan = {
	"gg_chakan", NULL, NULL, NULL, "1992",
	"Chakan (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chakanRomInfo, gg_chakanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Championship Hockey (Euro)

static struct BurnRomInfo gg_champhckRomDesc[] = {
	{ "championship hockey (euro).bin",	0x40000, 0xe21d2a88, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_champhck)
STD_ROM_FN(gg_champhck)

struct BurnDriver BurnDrvgg_champhck = {
	"gg_champhck", NULL, NULL, NULL, "1992",
	"Championship Hockey (Euro)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_champhckRomInfo, gg_champhckRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Chessmaster (Euro, USA)

static struct BurnRomInfo gg_chessmstRomDesc[] = {
	{ "chessmaster, the (usa, europe).bin",	0x20000, 0xda811ba6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chessmst)
STD_ROM_FN(gg_chessmst)

struct BurnDriver BurnDrvgg_chessmst = {
	"gg_chessmst", NULL, NULL, NULL, "1991",
	"The Chessmaster (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chessmstRomInfo, gg_chessmstRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (USA, Bra)

static struct BurnRomInfo gg_chicagoRomDesc[] = {
	{ "mpr-18190-s.ic1",	0x80000, 0x6b0fcec3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicago)
STD_ROM_FN(gg_chicago)

struct BurnDriver BurnDrvgg_chicago = {
	"gg_chicago", NULL, NULL, NULL, "1995",
	"Chicago Syndicate (USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagoRomInfo, gg_chicagoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950703)

static struct BurnRomInfo gg_chicagop09RomDesc[] = {
	{ "chicago syndicate (prototype - jul 03,  1995).bin",	0x80000, 0xe3c0ea98, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop09)
STD_ROM_FN(gg_chicagop09)

struct BurnDriver BurnDrvgg_chicagop09 = {
	"gg_chicagop09", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950703)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop09RomInfo, gg_chicagop09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950705)

static struct BurnRomInfo gg_chicagop08RomDesc[] = {
	{ "chicago syndicate (prototype - jul 05,  1995).bin",	0x80000, 0x4158e203, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop08)
STD_ROM_FN(gg_chicagop08)

struct BurnDriver BurnDrvgg_chicagop08 = {
	"gg_chicagop08", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950705)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop08RomInfo, gg_chicagop08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950707)

static struct BurnRomInfo gg_chicagop07RomDesc[] = {
	{ "chicago syndicate (prototype - jul 07,  1995).bin",	0x80000, 0xe44d1b01, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop07)
STD_ROM_FN(gg_chicagop07)

struct BurnDriver BurnDrvgg_chicagop07 = {
	"gg_chicagop07", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950707)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop07RomInfo, gg_chicagop07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950710)

static struct BurnRomInfo gg_chicagop06RomDesc[] = {
	{ "chicago syndicate (prototype - jul 10,  1995).bin",	0x80000, 0x42becef6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop06)
STD_ROM_FN(gg_chicagop06)

struct BurnDriver BurnDrvgg_chicagop06 = {
	"gg_chicagop06", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950710)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop06RomInfo, gg_chicagop06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950711)

static struct BurnRomInfo gg_chicagop05RomDesc[] = {
	{ "chicago syndicate (prototype - jul 11,  1995).bin",	0x80000, 0xafe46aa8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop05)
STD_ROM_FN(gg_chicagop05)

struct BurnDriver BurnDrvgg_chicagop05 = {
	"gg_chicagop05", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950711)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop05RomInfo, gg_chicagop05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950719)

static struct BurnRomInfo gg_chicagop04RomDesc[] = {
	{ "chicago syndicate (prototype - jul 19,  1995).bin",	0x80000, 0xdd5f9327, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop04)
STD_ROM_FN(gg_chicagop04)

struct BurnDriver BurnDrvgg_chicagop04 = {
	"gg_chicagop04", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950719)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop04RomInfo, gg_chicagop04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950720)

static struct BurnRomInfo gg_chicagop03RomDesc[] = {
	{ "chicago syndicate (prototype - jul 20,  1995).bin",	0x80000, 0xb0597a54, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop03)
STD_ROM_FN(gg_chicagop03)

struct BurnDriver BurnDrvgg_chicagop03 = {
	"gg_chicagop03", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950720)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop03RomInfo, gg_chicagop03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950721-B)

static struct BurnRomInfo gg_chicagop02RomDesc[] = {
	{ "chicago syndicate (prototype - jul 21,  1995 - b).bin",	0x80000, 0x26ced733, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop02)
STD_ROM_FN(gg_chicagop02)

struct BurnDriver BurnDrvgg_chicagop02 = {
	"gg_chicagop02", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950721-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop02RomInfo, gg_chicagop02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950721)

static struct BurnRomInfo gg_chicagop01RomDesc[] = {
	{ "chicago syndicate (prototype - jul 21,  1995).bin",	0x80000, 0x8e149bb8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop01)
STD_ROM_FN(gg_chicagop01)

struct BurnDriver BurnDrvgg_chicagop01 = {
	"gg_chicagop01", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950721)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop01RomInfo, gg_chicagop01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950601)

static struct BurnRomInfo gg_chicagop23RomDesc[] = {
	{ "chicago syndicate (prototype - jun 01,  1995).bin",	0x80000, 0x448c6890, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop23)
STD_ROM_FN(gg_chicagop23)

struct BurnDriver BurnDrvgg_chicagop23 = {
	"gg_chicagop23", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950601)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop23RomInfo, gg_chicagop23RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950605)

static struct BurnRomInfo gg_chicagop22RomDesc[] = {
	{ "chicago syndicate (prototype - jun 05,  1995).bin",	0x80000, 0xaf43b940, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop22)
STD_ROM_FN(gg_chicagop22)

struct BurnDriver BurnDrvgg_chicagop22 = {
	"gg_chicagop22", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950605)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop22RomInfo, gg_chicagop22RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950607)

static struct BurnRomInfo gg_chicagop21RomDesc[] = {
	{ "chicago syndicate (prototype - jun 07,  1995).bin",	0x80000, 0x9b2fd7f3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop21)
STD_ROM_FN(gg_chicagop21)

struct BurnDriver BurnDrvgg_chicagop21 = {
	"gg_chicagop21", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950607)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop21RomInfo, gg_chicagop21RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950608)

static struct BurnRomInfo gg_chicagop20RomDesc[] = {
	{ "chicago syndicate (prototype - jun 08,  1995).bin",	0x80000, 0x2036d4b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop20)
STD_ROM_FN(gg_chicagop20)

struct BurnDriver BurnDrvgg_chicagop20 = {
	"gg_chicagop20", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950608)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop20RomInfo, gg_chicagop20RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950612)

static struct BurnRomInfo gg_chicagop19RomDesc[] = {
	{ "chicago syndicate (prototype - jun 12,  1995).bin",	0x80000, 0xf2c24745, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop19)
STD_ROM_FN(gg_chicagop19)

struct BurnDriver BurnDrvgg_chicagop19 = {
	"gg_chicagop19", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950612)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop19RomInfo, gg_chicagop19RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950614)

static struct BurnRomInfo gg_chicagop18RomDesc[] = {
	{ "chicago syndicate (prototype - jun 14,  1995).bin",	0x80000, 0x463a2197, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop18)
STD_ROM_FN(gg_chicagop18)

struct BurnDriver BurnDrvgg_chicagop18 = {
	"gg_chicagop18", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950614)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop18RomInfo, gg_chicagop18RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950619)

static struct BurnRomInfo gg_chicagop17RomDesc[] = {
	{ "chicago syndicate (prototype - jun 19,  1995).bin",	0x80000, 0x3cdb35b1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop17)
STD_ROM_FN(gg_chicagop17)

struct BurnDriver BurnDrvgg_chicagop17 = {
	"gg_chicagop17", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950619)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop17RomInfo, gg_chicagop17RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950620)

static struct BurnRomInfo gg_chicagop16RomDesc[] = {
	{ "chicago syndicate (prototype - jun 20,  1995).bin",	0x80000, 0xbf82cf83, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop16)
STD_ROM_FN(gg_chicagop16)

struct BurnDriver BurnDrvgg_chicagop16 = {
	"gg_chicagop16", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950620)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop16RomInfo, gg_chicagop16RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950622)

static struct BurnRomInfo gg_chicagop15RomDesc[] = {
	{ "chicago syndicate (prototype - jun 22,  1995).bin",	0x80000, 0x4ac79cec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop15)
STD_ROM_FN(gg_chicagop15)

struct BurnDriver BurnDrvgg_chicagop15 = {
	"gg_chicagop15", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950622)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop15RomInfo, gg_chicagop15RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950626)

static struct BurnRomInfo gg_chicagop14RomDesc[] = {
	{ "chicago syndicate (prototype - jun 26,  1995).bin",	0x80000, 0xabe696e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop14)
STD_ROM_FN(gg_chicagop14)

struct BurnDriver BurnDrvgg_chicagop14 = {
	"gg_chicagop14", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950626)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop14RomInfo, gg_chicagop14RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950628)

static struct BurnRomInfo gg_chicagop13RomDesc[] = {
	{ "chicago syndicate (prototype - jun 28,  1995).bin",	0x80000, 0x5cd07aed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop13)
STD_ROM_FN(gg_chicagop13)

struct BurnDriver BurnDrvgg_chicagop13 = {
	"gg_chicagop13", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950628)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop13RomInfo, gg_chicagop13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950629-B)

static struct BurnRomInfo gg_chicagop12RomDesc[] = {
	{ "chicago syndicate (prototype - jun 29,  1995 - b).bin",	0x80000, 0xd3614e4e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop12)
STD_ROM_FN(gg_chicagop12)

struct BurnDriver BurnDrvgg_chicagop12 = {
	"gg_chicagop12", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950629-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop12RomInfo, gg_chicagop12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950629)

static struct BurnRomInfo gg_chicagop11RomDesc[] = {
	{ "chicago syndicate (prototype - jun 29,  1995).bin",	0x80000, 0xe1112763, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop11)
STD_ROM_FN(gg_chicagop11)

struct BurnDriver BurnDrvgg_chicagop11 = {
	"gg_chicagop11", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950629)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop11RomInfo, gg_chicagop11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950630-B)

static struct BurnRomInfo gg_chicagop10RomDesc[] = {
	{ "chicago syndicate (prototype - jun 30,  1995 - b).bin",	0x80000, 0xf68a3581, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop10)
STD_ROM_FN(gg_chicagop10)

struct BurnDriver BurnDrvgg_chicagop10 = {
	"gg_chicagop10", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950630-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop10RomInfo, gg_chicagop10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chicago Syndicate (Prototype, 19950505)

static struct BurnRomInfo gg_chicagop24RomDesc[] = {
	{ "chicago syndicate (prototype - may 05,  1995).bin",	0x80000, 0xa5fa4eaf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chicagop24)
STD_ROM_FN(gg_chicagop24)

struct BurnDriver BurnDrvgg_chicagop24 = {
	"gg_chicagop24", "gg_chicago", NULL, NULL, "1995",
	"Chicago Syndicate (Prototype, 19950505)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chicagop24RomInfo, gg_chicagop24RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Choplifter III (USA)

static struct BurnRomInfo gg_choplft3RomDesc[] = {
	{ "choplifter iii (usa, europe).bin",	0x40000, 0xc2e8a692, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_choplft3)
STD_ROM_FN(gg_choplft3)

struct BurnDriver BurnDrvgg_choplft3 = {
	"gg_choplft3", NULL, NULL, NULL, "1993",
	"Choplifter III (USA)\0", NULL, "Extreme Entertainment Group", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_choplft3RomInfo, gg_choplft3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chuck Rock (World)

static struct BurnRomInfo gg_chuckrckRomDesc[] = {
	{ "mpr-15054 j20.ic1",	0x40000, 0x191b1ed8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chuckrck)
STD_ROM_FN(gg_chuckrck)

struct BurnDriver BurnDrvgg_chuckrck = {
	"gg_chuckrck", NULL, NULL, NULL, "1992",
	"Chuck Rock (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chuckrckRomInfo, gg_chuckrckRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chuck Rock (World, Prototype)

static struct BurnRomInfo gg_chuckrckpRomDesc[] = {
	{ "chuck rock (proto).bin",	0x40000, 0xcb3a1d23, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chuckrckp)
STD_ROM_FN(gg_chuckrckp)

struct BurnDriver BurnDrvgg_chuckrckp = {
	"gg_chuckrckp", "gg_chuckrck", NULL, NULL, "1992",
	"Chuck Rock (World, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chuckrckpRomInfo, gg_chuckrckpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chuck Rock II - Son of Chuck (Euro)

static struct BurnRomInfo gg_chukrck2RomDesc[] = {
	{ "chuck rock ii - son of chuck (europe).bin",	0x40000, 0x656708bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chukrck2)
STD_ROM_FN(gg_chukrck2)

struct BurnDriver BurnDrvgg_chukrck2 = {
	"gg_chukrck2", NULL, NULL, NULL, "1994",
	"Chuck Rock II - Son of Chuck (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chukrck2RomInfo, gg_chukrck2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Chuck Rock II - Son of Chuck (USA)

static struct BurnRomInfo gg_chukrck2uRomDesc[] = {
	{ "chuck rock ii - son of chuck (usa).bin",	0x40000, 0x3fcc28c9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chukrck2u)
STD_ROM_FN(gg_chukrck2u)

struct BurnDriver BurnDrvgg_chukrck2u = {
	"gg_chukrck2u", "gg_chukrck2", NULL, NULL, "1994",
	"Chuck Rock II - Son of Chuck (USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_chukrck2uRomInfo, gg_chukrck2uRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// CJ Elephant Fugitive (Euro)

static struct BurnRomInfo gg_cjRomDesc[] = {
	{ "cj elephant fugitive.bin",	0x40000, 0x72981057, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_cj)
STD_ROM_FN(gg_cj)

struct BurnDriver BurnDrvgg_cj = {
	"gg_cj", NULL, NULL, NULL, "1991",
	"CJ Elephant Fugitive (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_cjRomInfo, gg_cjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cliffhanger (USA)

static struct BurnRomInfo gg_cliffhRomDesc[] = {
	{ "cliffhanger (usa, europe).bin",	0x40000, 0xbf75b153, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_cliffh)
STD_ROM_FN(gg_cliffh)

struct BurnDriver BurnDrvgg_cliffh = {
	"gg_cliffh", NULL, NULL, NULL, "1993",
	"Cliffhanger (USA)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_cliffhRomInfo, gg_cliffhRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Clutch Hitter (USA)

static struct BurnRomInfo gg_clutchitRomDesc[] = {
	{ "clutch hitter (usa).bin",	0x20000, 0xd228a467, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_clutchit)
STD_ROM_FN(gg_clutchit)

struct BurnDriver BurnDrvgg_clutchit = {
	"gg_clutchit", NULL, NULL, NULL, "1991",
	"Clutch Hitter (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_clutchitRomInfo, gg_clutchitRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Coca Cola Kid (Jpn)

static struct BurnRomInfo gg_cocakidRomDesc[] = {
	{ "mpr-16686-s.ic1",	0x80000, 0xc7598b81, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_cocakid)
STD_ROM_FN(gg_cocakid)

struct BurnDriver BurnDrvgg_cocakid = {
	"gg_cocakid", NULL, NULL, NULL, "1994",
	"Coca Cola Kid (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_cocakidRomInfo, gg_cocakidRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Columns (Jpn, Older)

static struct BurnRomInfo gg_columnsj1RomDesc[] = {
	{ "columns (japan).bin",	0x08000, 0xf3ca6676, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_columnsj1)
STD_ROM_FN(gg_columnsj1)

struct BurnDriver BurnDrvgg_columnsj1 = {
	"gg_columnsj1", "gg_columns", NULL, NULL, "1990",
	"Columns (Jpn, Older)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_columnsj1RomInfo, gg_columnsj1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Columns (Jpn)

static struct BurnRomInfo gg_columnsjRomDesc[] = {
	{ "columns [v1] (jp).bin",	0x08000, 0xac37e092, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_columnsj)
STD_ROM_FN(gg_columnsj)

struct BurnDriver BurnDrvgg_columnsj = {
	"gg_columnsj", "gg_columns", NULL, NULL, "1990",
	"Columns (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_columnsjRomInfo, gg_columnsjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Columns (Euro, USA, Bra)

static struct BurnRomInfo gg_columnsRomDesc[] = {
	{ "mpr-13696.ic1",	0x08000, 0x83fa26d9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_columns)
STD_ROM_FN(gg_columns)

struct BurnDriver BurnDrvgg_columns = {
	"gg_columns", NULL, NULL, NULL, "1990",
	"Columns (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_columnsRomInfo, gg_columnsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cool Spot (Euro)

static struct BurnRomInfo gg_coolspotRomDesc[] = {
	{ "cool spot.bin",	0x40000, 0xba0714db, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_coolspot)
STD_ROM_FN(gg_coolspot)

struct BurnDriver BurnDrvgg_coolspot = {
	"gg_coolspot", NULL, NULL, NULL, "1993",
	"Cool Spot (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_coolspotRomInfo, gg_coolspotRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cool Spot (USA)

static struct BurnRomInfo gg_coolspotuRomDesc[] = {
	{ "cool spot (usa).bin",	0x40000, 0x2c758fc8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_coolspotu)
STD_ROM_FN(gg_coolspotu)

struct BurnDriver BurnDrvgg_coolspotu = {
	"gg_coolspotu", "gg_coolspot", NULL, NULL, "1993",
	"Cool Spot (USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_coolspotuRomInfo, gg_coolspotuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cosmic Spacehead (Euro)

static struct BurnRomInfo gg_cosmicRomDesc[] = {
	{ "cosmic spacehead (europe).bin",	0x40000, 0x6caa625b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_cosmic)
STD_ROM_FN(gg_cosmic)

struct BurnDriver BurnDrvgg_cosmic = {
	"gg_cosmic", NULL, NULL, NULL, "1993",
	"Cosmic Spacehead (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_cosmicRomInfo, gg_cosmicRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Crayon Shin-chan - Taiketsu! Kantamu Panic!! (Jpn)

static struct BurnRomInfo gg_crayonRomDesc[] = {
	{ "mpr-17805.ic1",	0x80000, 0x03d28eab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_crayon)
STD_ROM_FN(gg_crayon)

struct BurnDriver BurnDrvgg_crayon = {
	"gg_crayon", NULL, NULL, NULL, "1995",
	"Crayon Shin-chan - Taiketsu! Kantamu Panic!! (Jpn)\0", NULL, "Bandai", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_crayonRomInfo, gg_crayonRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Crazy Faces (Euro, Prototype)

static struct BurnRomInfo gg_crazyfacRomDesc[] = {
	{ "crazy faces [proto].bin",	0x20000, 0x46ad6257, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_crazyfac)
STD_ROM_FN(gg_crazyfac)

struct BurnDriver BurnDrvgg_crazyfac = {
	"gg_crazyfac", NULL, NULL, NULL, "1993",
	"Crazy Faces (Euro, Prototype)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_crazyfacRomInfo, gg_crazyfacRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Crystal Warriors (Euro, USA)

static struct BurnRomInfo gg_crystalwRomDesc[] = {
	{ "mpr-14633.ic1",	0x40000, 0x529c864e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_crystalw)
STD_ROM_FN(gg_crystalw)

struct BurnDriver BurnDrvgg_crystalw = {
	"gg_crystalw", NULL, NULL, NULL, "1992",
	"Crystal Warriors (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_crystalwRomInfo, gg_crystalwRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cutthroat Island (Euro, USA)

static struct BurnRomInfo gg_cutthrRomDesc[] = {
	{ "cutthroat island (usa, europe).bin",	0x80000, 0x6a24e47e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_cutthr)
STD_ROM_FN(gg_cutthr)

struct BurnDriver BurnDrvgg_cutthr = {
	"gg_cutthr", NULL, NULL, NULL, "1995",
	"Cutthroat Island (Euro, USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_cutthrRomInfo, gg_cutthrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cutthroat Island (Prototype 19950916)

static struct BurnRomInfo gg_cutthrpRomDesc[] = {
	{ "cutthroat island [proto].bin",	0x80000, 0x6352ef00, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_cutthrp)
STD_ROM_FN(gg_cutthrp)

struct BurnDriver BurnDrvgg_cutthrp = {
	"gg_cutthrp", "gg_cutthr", NULL, NULL, "1995",
	"Cutthroat Island (Prototype 19950916)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_cutthrpRomInfo, gg_cutthrpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Daffy Duck in Hollywood (Euro)

static struct BurnRomInfo gg_daffyRomDesc[] = {
	{ "daffy duck in hollywood (europe) (en,fr,de,es,it).bin",	0x80000, 0xaae268fc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_daffy)
STD_ROM_FN(gg_daffy)

struct BurnDriver BurnDrvgg_daffy = {
	"gg_daffy", NULL, NULL, NULL, "1994",
	"Daffy Duck in Hollywood (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_daffyRomInfo, gg_daffyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Deep Duck Trouble Starring Donald Duck (Euro, USA)

static struct BurnRomInfo gg_deepduckRomDesc[] = {
	{ "deep duck trouble starring donald duck (usa, europe).bin",	0x80000, 0x5a136d7e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_deepduck)
STD_ROM_FN(gg_deepduck)

struct BurnDriver BurnDrvgg_deepduck = {
	"gg_deepduck", NULL, NULL, NULL, "1994",
	"Deep Duck Trouble Starring Donald Duck (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_deepduckRomInfo, gg_deepduckRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Defenders of Oasis (Euro, USA)

static struct BurnRomInfo gg_defoasisRomDesc[] = {
	{ "defenders of oasis (usa, europe).bin",	0x80000, 0xe2791cc1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_defoasis)
STD_ROM_FN(gg_defoasis)

struct BurnDriver BurnDrvgg_defoasis = {
	"gg_defoasis", NULL, NULL, NULL, "1992",
	"Defenders of Oasis (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_defoasisRomInfo, gg_defoasisRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Defenders of Oasis (Euro, USA, Prototype)

static struct BurnRomInfo gg_defoasispRomDesc[] = {
	{ "defenders of oasis (proto).bin",	0x40000, 0xc674eccc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_defoasisp)
STD_ROM_FN(gg_defoasisp)

struct BurnDriver BurnDrvgg_defoasisp = {
	"gg_defoasisp", "gg_defoasis", NULL, NULL, "1992",
	"Defenders of Oasis (Euro, USA, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_defoasispRomInfo, gg_defoasispRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Desert Speedtrap Starring Road Runner and Wile E. Coyote (Euro)

static struct BurnRomInfo gg_desertRomDesc[] = {
	{ "mpr-16032.ic1",	0x40000, 0xec808026, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_desert)
STD_ROM_FN(gg_desert)

struct BurnDriver BurnDrvgg_desert = {
	"gg_desert", NULL, NULL, NULL, "1994",
	"Desert Speedtrap Starring Road Runner and Wile E. Coyote (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_desertRomInfo, gg_desertRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Desert Speedtrap Starring Road Runner and Wile E. Coyote (USA)

static struct BurnRomInfo gg_desertuRomDesc[] = {
	{ "desert speedtrap (usa).bin",	0x40000, 0xc2e111ac, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_desertu)
STD_ROM_FN(gg_desertu)

struct BurnDriver BurnDrvgg_desertu = {
	"gg_desertu", "gg_desert", NULL, NULL, "1994",
	"Desert Speedtrap Starring Road Runner and Wile E. Coyote (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_desertuRomInfo, gg_desertuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Desert Strike (Euro)

static struct BurnRomInfo gg_dstrikeRomDesc[] = {
	{ "desert strike (eu).bin",	0x80000, 0x3e44eca3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dstrike)
STD_ROM_FN(gg_dstrike)

struct BurnDriver BurnDrvgg_dstrike = {
	"gg_dstrike", NULL, NULL, NULL, "1992",
	"Desert Strike (Euro)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dstrikeRomInfo, gg_dstrikeRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Desert Strike - Return to the Gulf (USA)

static struct BurnRomInfo gg_dstrikeuRomDesc[] = {
	{ "desert strike - return to the gulf (usa).bin",	0x80000, 0xf6c400da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dstrikeu)
STD_ROM_FN(gg_dstrikeu)

struct BurnDriver BurnDrvgg_dstrikeu = {
	"gg_dstrikeu", "gg_dstrike", NULL, NULL, "1992",
	"Desert Strike - Return to the Gulf (USA)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dstrikeuRomInfo, gg_dstrikeuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Devilish (Euro, Bra)

static struct BurnRomInfo gg_devilishRomDesc[] = {
	{ "devilish (europe).bin",	0x20000, 0xd43ac6c5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_devilish)
STD_ROM_FN(gg_devilish)

struct BurnDriver BurnDrvgg_devilish = {
	"gg_devilish", NULL, NULL, NULL, "1992",
	"Devilish (Euro, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_devilishRomInfo, gg_devilishRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Devilish (Jpn)

static struct BurnRomInfo gg_devilishjRomDesc[] = {
	{ "devilish (japan).bin",	0x20000, 0x25db174f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_devilishj)
STD_ROM_FN(gg_devilishj)

struct BurnDriver BurnDrvgg_devilishj = {
	"gg_devilishj", "gg_devilish", NULL, NULL, "1991",
	"Devilish (Jpn)\0", NULL, "Genki", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_devilishjRomInfo, gg_devilishjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Devilish (USA)

static struct BurnRomInfo gg_devilishuRomDesc[] = {
	{ "devilish (usa).bin",	0x20000, 0xc01293b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_devilishu)
STD_ROM_FN(gg_devilishu)

struct BurnDriver BurnDrvgg_devilishu = {
	"gg_devilishu", "gg_devilish", NULL, NULL, "1991",
	"Devilish (USA)\0", NULL, "Sage's Creations", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_devilishuRomInfo, gg_devilishuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Donald Duck no 4-Tsu no Hihou (Jpn)

static struct BurnRomInfo gg_donald42RomDesc[] = {
	{ "donald duck no 4-tsu no hihou (japan).bin",	0x80000, 0x4457e7c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_donald42)
STD_ROM_FN(gg_donald42)

struct BurnDriver BurnDrvgg_donald42 = {
	"gg_donald42", "gg_deepduck", NULL, NULL, "1993",
	"Donald Duck no 4-Tsu no Hihou (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_donald42RomInfo, gg_donald42RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Donald Duck no Lucky Dime (Jpn)

static struct BurnRomInfo gg_donaldldRomDesc[] = {
	{ "donald duck no lucky dime (japan).bin",	0x40000, 0x2d98bd87, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_donaldld)
STD_ROM_FN(gg_donaldld)

struct BurnDriver BurnDrvgg_donaldld = {
	"gg_donaldld", "gg_luckydim", NULL, NULL, "1991",
	"Donald Duck no Lucky Dime (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_donaldldRomInfo, gg_donaldldRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Donald no Magical World (Jpn)

static struct BurnRomInfo gg_donaldmwRomDesc[] = {
	{ "donald no magical world (japan) (en,ja).bin",	0x80000, 0x87b8b612, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_donaldmw)
STD_ROM_FN(gg_donaldmw)

struct BurnDriver BurnDrvgg_donaldmw = {
	"gg_donaldmw", NULL, NULL, NULL, "1994",
	"Donald no Magical World (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_donaldmwRomInfo, gg_donaldmwRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Doraemon - Waku Waku Pocket Paradise (Jpn)

static struct BurnRomInfo gg_doraemonRomDesc[] = {
	{ "doraemon - waku waku pocket paradise (japan).bin",	0x80000, 0x733292a6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_doraemon)
STD_ROM_FN(gg_doraemon)

struct BurnDriver BurnDrvgg_doraemon = {
	"gg_doraemon", NULL, NULL, NULL, "1996",
	"Doraemon - Waku Waku Pocket Paradise (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_doraemonRomInfo, gg_doraemonRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Double Dragon (Euro, USA, Prototype)

static struct BurnRomInfo gg_ddragonpRomDesc[] = {
	{ "double dragon (usa, europe) (beta).bin",	0x40000, 0x331904c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ddragonp)
STD_ROM_FN(gg_ddragonp)

struct BurnDriver BurnDrvgg_ddragonp = {
	"gg_ddragonp", "gg_ddragon", NULL, NULL, "1993",
	"Double Dragon (Euro, USA, Prototype)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ddragonpRomInfo, gg_ddragonpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Double Dragon (Euro, USA)

static struct BurnRomInfo gg_ddragonRomDesc[] = {
	{ "double dragon (usa, europe).bin",	0x40000, 0x1307a290, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ddragon)
STD_ROM_FN(gg_ddragon)

struct BurnDriver BurnDrvgg_ddragon = {
	"gg_ddragon", NULL, NULL, NULL, "1993",
	"Double Dragon (Euro, USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ddragonRomInfo, gg_ddragonRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dr. Robotnik's Mean Bean Machine (Euro, USA)

static struct BurnRomInfo gg_drrobotnRomDesc[] = {
	{ "mpr-16075-f.ic1",	0x40000, 0x3c2d4f48, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_drrobotn)
STD_ROM_FN(gg_drrobotn)

struct BurnDriver BurnDrvgg_drrobotn = {
	"gg_drrobotn", NULL, NULL, NULL, "1993",
	"Dr. Robotnik's Mean Bean Machine (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_drrobotnRomInfo, gg_drrobotnRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dr. Franken (Prototype Demo)

static struct BurnRomInfo gg_drfranknRomDesc[] = {
	{ "dr. franken (demo).bin",	0x20000, 0xc9907dce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_drfrankn)
STD_ROM_FN(gg_drfrankn)

struct BurnDriver BurnDrvgg_drfrankn = {
	"gg_drfrankn", NULL, NULL, NULL, "1993",
	"Dr. Franken (Prototype Demo)\0", NULL, "Elite", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_drfranknRomInfo, gg_drfranknRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dragon - The Bruce Lee Story (Euro)

static struct BurnRomInfo gg_dragonRomDesc[] = {
	{ "dragon - the bruce lee story (europe).bin",	0x40000, 0x19e1cf2b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dragon)
STD_ROM_FN(gg_dragon)

struct BurnDriver BurnDrvgg_dragon = {
	"gg_dragon", NULL, NULL, NULL, "1994",
	"Dragon - The Bruce Lee Story (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dragonRomInfo, gg_dragonRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dragon - The Bruce Lee Story (USA)

static struct BurnRomInfo gg_dragonuRomDesc[] = {
	{ "dragon - the bruce lee story (usa).bin",	0x40000, 0x17f588e8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dragonu)
STD_ROM_FN(gg_dragonu)

struct BurnDriver BurnDrvgg_dragonu = {
	"gg_dragonu", "gg_dragon", NULL, NULL, "1995",
	"Dragon - The Bruce Lee Story (USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dragonuRomInfo, gg_dragonuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dragon Crystal (Euro, USA)

static struct BurnRomInfo gg_dcrystalRomDesc[] = {
	{ "mpr-13800-s.ic1",	0x20000, 0x0ef2ed93, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dcrystal)
STD_ROM_FN(gg_dcrystal)

struct BurnDriver BurnDrvgg_dcrystal = {
	"gg_dcrystal", NULL, NULL, NULL, "1990",
	"Dragon Crystal (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dcrystalRomInfo, gg_dcrystalRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dragon Crystal - Tsurani no Meikyuu (Jpn)

static struct BurnRomInfo gg_dcrystaljRomDesc[] = {
	{ "dragon crystal - tsurani no meikyuu (japan).bin",	0x20000, 0x89f12e1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dcrystalj)
STD_ROM_FN(gg_dcrystalj)

struct BurnDriver BurnDrvgg_dcrystalj = {
	"gg_dcrystalj", "gg_dcrystal", NULL, NULL, "1990",
	"Dragon Crystal - Tsurani no Meikyuu (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dcrystaljRomInfo, gg_dcrystaljRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dropzone (Euro)

static struct BurnRomInfo gg_dropzoneRomDesc[] = {
	{ "dropzone ggdz e7f1.u1",	0x20000, 0x152f0dcc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dropzone)
STD_ROM_FN(gg_dropzone)

struct BurnDriver BurnDrvgg_dropzone = {
	"gg_dropzone", NULL, NULL, NULL, "1994",
	"Dropzone (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_dropzoneRomInfo, gg_dropzoneRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dunk Kids (Jpn)

static struct BurnRomInfo gg_dunkkidsRomDesc[] = {
	{ "mpr-17023.ic1",	0x80000, 0x77ed48f5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dunkkids)
STD_ROM_FN(gg_dunkkids)

struct BurnDriver BurnDrvgg_dunkkids = {
	"gg_dunkkids", NULL, NULL, NULL, "1994",
	"Dunk Kids (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dunkkidsRomInfo, gg_dunkkidsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Headdy (Jpn)

static struct BurnRomInfo gg_dheadjRomDesc[] = {
	{ "dynamite headdy (japan).bin",	0x80000, 0xf6af4b6b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dheadj)
STD_ROM_FN(gg_dheadj)

struct BurnDriver BurnDrvgg_dheadj = {
	"gg_dheadj", "gg_dhead", NULL, NULL, "1994",
	"Dynamite Headdy (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dheadjRomInfo, gg_dheadjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Headdy (Euro, USA)

static struct BurnRomInfo gg_dheadRomDesc[] = {
	{ "dynamite headdy (usa, europe).bin",	0x80000, 0x610ff95c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dhead)
STD_ROM_FN(gg_dhead)

struct BurnDriver BurnDrvgg_dhead = {
	"gg_dhead", NULL, NULL, NULL, "1994",
	"Dynamite Headdy (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dheadRomInfo, gg_dheadRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Headdy (Prototype, 19940701)

static struct BurnRomInfo gg_dheadp2RomDesc[] = {
	{ "dynamite headdy (prototype - jul 01,  1994).bin",	0x80000, 0x49389319, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dheadp2)
STD_ROM_FN(gg_dheadp2)

struct BurnDriver BurnDrvgg_dheadp2 = {
	"gg_dheadp2", "gg_dhead", NULL, NULL, "1994",
	"Dynamite Headdy (Prototype, 19940701)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dheadp2RomInfo, gg_dheadp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Headdy (Prototype, 19940705)

static struct BurnRomInfo gg_dheadp1RomDesc[] = {
	{ "dynamite headdy (prototype - jul 05,  1994).bin",	0x80000, 0xc7bea89a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dheadp1)
STD_ROM_FN(gg_dheadp1)

struct BurnDriver BurnDrvgg_dheadp1 = {
	"gg_dheadp1", "gg_dhead", NULL, NULL, "1994",
	"Dynamite Headdy (Prototype, 19940705)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dheadp1RomInfo, gg_dheadp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Dynamite Headdy (Prototype, 19940615)

static struct BurnRomInfo gg_dheadp3RomDesc[] = {
	{ "dynamite headdy (prototype - jun 15,  1994).bin",	0x80000, 0x7bf339c0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_dheadp3)
STD_ROM_FN(gg_dheadp3)

struct BurnDriver BurnDrvgg_dheadp3 = {
	"gg_dheadp3", "gg_dhead", NULL, NULL, "1994",
	"Dynamite Headdy (Prototype, 19940615)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_dheadp3RomInfo, gg_dheadp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Earthworm Jim (Euro)

static struct BurnRomInfo gg_ejimRomDesc[] = {
	{ "earthworm jim.bin",	0x80000, 0x691ae339, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ejim)
STD_ROM_FN(gg_ejim)

struct BurnDriver BurnDrvgg_ejim = {
	"gg_ejim", NULL, NULL, NULL, "1995",
	"Earthworm Jim (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ejimRomInfo, gg_ejimRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Earthworm Jim (USA)

static struct BurnRomInfo gg_ejimuRomDesc[] = {
	{ "earthworm jim (usa).bin",	0x80000, 0x5d3f23a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ejimu)
STD_ROM_FN(gg_ejimu)

struct BurnDriver BurnDrvgg_ejimu = {
	"gg_ejimu", "gg_ejim", NULL, NULL, "1995",
	"Earthworm Jim (USA)\0", NULL, "Playmates Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ejimuRomInfo, gg_ejimuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ecco the Dolphin (Jpn)

static struct BurnRomInfo gg_eccojRomDesc[] = {
	{ "mpr-16428-f.ic1",	0x80000, 0xa32eb9d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_eccoj)
STD_ROM_FN(gg_eccoj)

struct BurnDriver BurnDrvgg_eccoj = {
	"gg_eccoj", "gg_ecco", NULL, NULL, "1994",
	"Ecco the Dolphin (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_eccojRomInfo, gg_eccojRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ecco the Dolphin (Euro, USA)

static struct BurnRomInfo gg_eccoRomDesc[] = {
	{ "ecco the dolphin (usa, europe).bin",	0x80000, 0x866c7113, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ecco)
STD_ROM_FN(gg_ecco)

struct BurnDriver BurnDrvgg_ecco = {
	"gg_ecco", NULL, NULL, NULL, "1994",
	"Ecco the Dolphin (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_eccoRomInfo, gg_eccoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ecco the Dolphin II (Jpn)

static struct BurnRomInfo gg_ecco2jRomDesc[] = {
	{ "ecco the dolphin ii (japan).bin",	0x80000, 0xba9cef4f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ecco2j)
STD_ROM_FN(gg_ecco2j)

struct BurnDriver BurnDrvgg_ecco2j = {
	"gg_ecco2j", "gg_ecco2", NULL, NULL, "1995",
	"Ecco the Dolphin II (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ecco2jRomInfo, gg_ecco2jRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ecco II - The Tides of Time (Euro, USA)

static struct BurnRomInfo gg_ecco2RomDesc[] = {
	{ "ecco ii - the tides of time (usa, europe).bin",	0x80000, 0xe2f3b203, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ecco2)
STD_ROM_FN(gg_ecco2)

struct BurnDriver BurnDrvgg_ecco2 = {
	"gg_ecco2", NULL, NULL, NULL, "1994",
	"Ecco II - The Tides of Time (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ecco2RomInfo, gg_ecco2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ernie Els Golf (Euro)

static struct BurnRomInfo gg_ernieegRomDesc[] = {
	{ "ernie els golf (europe) (en,fr,de,es,it).bin",	0x40000, 0x5e53c7f7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ernieeg)
STD_ROM_FN(gg_ernieeg)

struct BurnDriver BurnDrvgg_ernieeg = {
	"gg_ernieeg", NULL, NULL, NULL, "1994",
	"Ernie Els Golf (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_ernieegRomInfo, gg_ernieegRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Eternal Legend - Eien no Densetsu (Jpn)

static struct BurnRomInfo gg_eternlegRomDesc[] = {
	{ "eternal legend - eien no densetsu (japan).bin",	0x40000, 0x04302bbd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_eternleg)
STD_ROM_FN(gg_eternleg)

struct BurnDriver BurnDrvgg_eternleg = {
	"gg_eternleg", NULL, NULL, NULL, "1991",
	"Eternal Legend - Eien no Densetsu (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_eternlegRomInfo, gg_eternlegRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Evander Holyfield Boxing (Euro, USA)

static struct BurnRomInfo gg_evanderRomDesc[] = {
	{ "mpr-15391.ic1",	0x40000, 0x36aaf536, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_evander)
STD_ROM_FN(gg_evander)

struct BurnDriver BurnDrvgg_evander = {
	"gg_evander", NULL, NULL, NULL, "1992",
	"Evander Holyfield Boxing (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_evanderRomInfo, gg_evanderRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Excellent Dizzy Collection (Euro, SMS Mode)

static struct BurnRomInfo gg_excdizzyRomDesc[] = {
	{ "excellent dizzy collection, the (europe).bin",	0x80000, 0xaa140c9c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_excdizzy)
STD_ROM_FN(gg_excdizzy)

struct BurnDriver BurnDrvgg_excdizzy = {
	"gg_excdizzy", NULL, NULL, NULL, "1995",
	"The Excellent Dizzy Collection (Euro, SMS Mode)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	GGGetZipName, gg_excdizzyRomInfo, gg_excdizzyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 224, 4, 3
};


// The Excellent Dizzy Collection (Prototype, SMS Mode)

static struct BurnRomInfo gg_excdizzypRomDesc[] = {
	{ "excellent dizzy collection, the (prototype) [s][!].bin",	0x40000, 0x8813514b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_excdizzyp)
STD_ROM_FN(gg_excdizzyp)

struct BurnDriver BurnDrvgg_excdizzyp = {
	"gg_excdizzyp", "gg_excdizzy", NULL, NULL, "1995",
	"The Excellent Dizzy Collection (Prototype, SMS Mode)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	GGGetZipName, gg_excdizzypRomInfo, gg_excdizzypRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 224, 4, 3
};


// F-15 Strike Eagle (Euro, USA)

static struct BurnRomInfo gg_f15seRomDesc[] = {
	{ "f-15 strike eagle (usa, europe).bin",	0x40000, 0x8bdb0806, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_f15se)
STD_ROM_FN(gg_f15se)

struct BurnDriver BurnDrvgg_f15se = {
	"gg_f15se", NULL, NULL, NULL, "1993",
	"F-15 Strike Eagle (Euro, USA)\0", NULL, "Microprose", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_f15seRomInfo, gg_f15seRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F1 (Euro, USA)

static struct BurnRomInfo gg_f1RomDesc[] = {
	{ "f1 (usa, europe).bin",	0x40000, 0xd0a93e00, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_f1)
STD_ROM_FN(gg_f1)

struct BurnDriver BurnDrvgg_f1 = {
	"gg_f1", NULL, NULL, NULL, "1993",
	"F1 (Euro, USA)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_f1RomInfo, gg_f1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F1 World Championship Edition (Euro)

static struct BurnRomInfo gg_f1wceRomDesc[] = {
	{ "f1 - world championship edition (euro).bin",	0x40000, 0x107092d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_f1wce)
STD_ROM_FN(gg_f1wce)

struct BurnDriver BurnDrvgg_f1wce = {
	"gg_f1wce", NULL, NULL, NULL, "1995",
	"F1 World Championship Edition (Euro)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_f1wceRomInfo, gg_f1wceRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Faceball 2000 (Jpn)

static struct BurnRomInfo gg_faceballRomDesc[] = {
	{ "faceball 2000 (japan).bin",	0x40000, 0xaaf6f87d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_faceball)
STD_ROM_FN(gg_faceball)

struct BurnDriver BurnDrvgg_faceball = {
	"gg_faceball", NULL, NULL, NULL, "1993",
	"Faceball 2000 (Jpn)\0", NULL, "Riverhill Software", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_faceballRomInfo, gg_faceballRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Factory Panic (Euro, Bra)

static struct BurnRomInfo gg_factorypRomDesc[] = {
	{ "mpr-14238.ic1",	0x20000, 0x59e3be92, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_factoryp)
STD_ROM_FN(gg_factoryp)

struct BurnDriver BurnDrvgg_factoryp = {
	"gg_factoryp", NULL, NULL, NULL, "1991",
	"Factory Panic (Euro, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_factorypRomInfo, gg_factorypRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantastic Dizzy (Euro, SMS Mode)

static struct BurnRomInfo gg_fantdizzRomDesc[] = {
	{ "fantastic dizzy (europe) (en,fr,de,es,it).bin",	0x40000, 0xc888222b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fantdizz)
STD_ROM_FN(gg_fantdizz)

struct BurnDriver BurnDrvgg_fantdizz = {
	"gg_fantdizz", NULL, NULL, NULL, "1993",
	"Fantastic Dizzy (Euro, SMS Mode)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_fantdizzRomInfo, gg_fantdizzRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone Gear (Euro, Jpn)

static struct BurnRomInfo gg_fantzoneRomDesc[] = {
	{ "fantasy zone gear (japan, europe).bin",	0x20000, 0xd69097e8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fantzone)
STD_ROM_FN(gg_fantzone)

struct BurnDriver BurnDrvgg_fantzone = {
	"gg_fantzone", NULL, NULL, NULL, "1991",
	"Fantasy Zone Gear (Euro, Jpn)\0", NULL, "Sanritsu", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fantzoneRomInfo, gg_fantzoneRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fantasy Zone Gear (USA)

static struct BurnRomInfo gg_fantzoneuRomDesc[] = {
	{ "fantasy zone (us).bin",	0x20000, 0xfad007df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fantzoneu)
STD_ROM_FN(gg_fantzoneu)

struct BurnDriver BurnDrvgg_fantzoneu = {
	"gg_fantzoneu", "gg_fantzone", NULL, NULL, "1991",
	"Fantasy Zone Gear (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fantzoneuRomInfo, gg_fantzoneuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fatal Fury Special (Euro)

static struct BurnRomInfo gg_fatfurspRomDesc[] = {
	{ "fatal fury special.bin",	0x80000, 0xfbd76387, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fatfursp)
STD_ROM_FN(gg_fatfursp)

struct BurnDriver BurnDrvgg_fatfursp = {
	"gg_fatfursp", NULL, NULL, NULL, "1994",
	"Fatal Fury Special (Euro)\0", NULL, "Takara", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fatfurspRomInfo, gg_fatfurspRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fatal Fury Special (USA)

static struct BurnRomInfo gg_fatfurspuRomDesc[] = {
	{ "fatal fury special (usa).bin",	0x80000, 0x449787e2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fatfurspu)
STD_ROM_FN(gg_fatfurspu)

struct BurnDriver BurnDrvgg_fatfurspu = {
	"gg_fatfurspu", "gg_fatfursp", NULL, NULL, "1994",
	"Fatal Fury Special (USA)\0", NULL, "Takara", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fatfurspuRomInfo, gg_fatfurspuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// FIFA International Soccer (Jpn)

static struct BurnRomInfo gg_fifajRomDesc[] = {
	{ "fifa international soccer (japan).bin",	0x80000, 0x78159325, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fifaj)
STD_ROM_FN(gg_fifaj)

struct BurnDriver BurnDrvgg_fifaj = {
	"gg_fifaj", "gg_fifa", NULL, NULL, "1995",
	"FIFA International Soccer (Jpn)\0", NULL, "Electronic Arts Victor", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fifajRomInfo, gg_fifajRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// FIFA International Soccer (Euro, USA)

static struct BurnRomInfo gg_fifaRomDesc[] = {
	{ "fifa international soccer (usa, europe).bin",	0x80000, 0xe632a3a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fifa)
STD_ROM_FN(gg_fifa)

struct BurnDriver BurnDrvgg_fifa = {
	"gg_fifa", NULL, NULL, NULL, "1994",
	"FIFA International Soccer (Euro, USA)\0", NULL, "Electronic Arts", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fifaRomInfo, gg_fifaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// FIFA Soccer 96 (Euro, USA)

static struct BurnRomInfo gg_fifa96RomDesc[] = {
	{ "fifa soccer 96 (usa, europe) (en,fr,de,es).bin",	0x80000, 0xc379de95, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fifa96)
STD_ROM_FN(gg_fifa96)

struct BurnDriver BurnDrvgg_fifa96 = {
	"gg_fifa96", NULL, NULL, NULL, "1995",
	"FIFA Soccer 96 (Euro, USA)\0", NULL, "Black Pearl", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fifa96RomInfo, gg_fifa96RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Foreman for Real (World)

static struct BurnRomInfo gg_foremanRomDesc[] = {
	{ "mpr-18216.ic1",	0x80000, 0xd46d5685, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_foreman)
STD_ROM_FN(gg_foreman)

struct BurnDriver BurnDrvgg_foreman = {
	"gg_foreman", NULL, NULL, NULL, "1995",
	"Foreman for Real (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_foremanRomInfo, gg_foremanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Frank Thomas Big Hurt Baseball (USA)

static struct BurnRomInfo gg_bighurtRomDesc[] = {
	{ "frank thomas big hurt baseball (usa).bin",	0x80000, 0xc443c35c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bighurt)
STD_ROM_FN(gg_bighurt)

struct BurnDriver BurnDrvgg_bighurt = {
	"gg_bighurt", NULL, NULL, NULL, "1995",
	"Frank Thomas Big Hurt Baseball (USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bighurtRomInfo, gg_bighurtRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fray - Shugyou Hen (Jpn)

static struct BurnRomInfo gg_frayRomDesc[] = {
	{ "fray - shugyou hen (japan).bin",	0x40000, 0xe123d9b8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fray)
STD_ROM_FN(gg_fray)

struct BurnDriver BurnDrvgg_fray = {
	"gg_fray", NULL, NULL, NULL, "1991",
	"Fray - Shugyou Hen (Jpn)\0", NULL, "Micro Cabin", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_frayRomInfo, gg_frayRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fred Couples' Golf (Jpn)

static struct BurnRomInfo gg_fredcoupjRomDesc[] = {
	{ "fred couples' golf (japan).bin",	0x80000, 0xb1196cd7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fredcoupj)
STD_ROM_FN(gg_fredcoupj)

struct BurnDriver BurnDrvgg_fredcoupj = {
	"gg_fredcoupj", "gg_fredcoup", NULL, NULL, "1994",
	"Fred Couples' Golf (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fredcoupjRomInfo, gg_fredcoupjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Fred Couples' Golf (USA)

static struct BurnRomInfo gg_fredcoupRomDesc[] = {
	{ "fred couples' golf (usa).bin",	0x80000, 0x46f40b9f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_fredcoup)
STD_ROM_FN(gg_fredcoup)

struct BurnDriver BurnDrvgg_fredcoup = {
	"gg_fredcoup", NULL, NULL, NULL, "1994",
	"Fred Couples' Golf (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_fredcoupRomInfo, gg_fredcoupRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Frogger (USA, Prototype)

static struct BurnRomInfo gg_froggerRomDesc[] = {
	{ "frogger (usa) (proto).bin",	0x20000, 0x02bbf994, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_frogger)
STD_ROM_FN(gg_frogger)

struct BurnDriver BurnDrvgg_frogger = {
	"gg_frogger", NULL, NULL, NULL, "199?",
	"Frogger (USA, Prototype)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_froggerRomInfo, gg_froggerRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// From TV Animation - Slam Dunk - Shouri e no Starting 5 (Jpn)

static struct BurnRomInfo gg_slamdunkRomDesc[] = {
	{ "from tv animation - slam dunk - shouri e no starting 5 (japan).bin",	0x80000, 0x751dad4c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_slamdunk)
STD_ROM_FN(gg_slamdunk)

struct BurnDriver BurnDrvgg_slamdunk = {
	"gg_slamdunk", NULL, NULL, NULL, "1994",
	"From TV Animation - Slam Dunk - Shouri e no Starting 5 (Jpn)\0", NULL, "Bandai", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_slamdunkRomInfo, gg_slamdunkRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// G-LOC Air Battle (Jpn, v1)

static struct BurnRomInfo gg_glocjRomDesc[] = {
	{ "g-loc air battle (japanv1.1).bin",	0x20000, 0x33237f50, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_glocj)
STD_ROM_FN(gg_glocj)

struct BurnDriver BurnDrvgg_glocj = {
	"gg_glocj", "gg_gloc", NULL, NULL, "1990",
	"G-LOC Air Battle (Jpn, v1)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_glocjRomInfo, gg_glocjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// G-LOC Air Battle (Jpn, v0)

static struct BurnRomInfo gg_glocj1RomDesc[] = {
	{ "g-loc air battle (japan).bin",	0x20000, 0x2333f615, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_glocj1)
STD_ROM_FN(gg_glocj1)

struct BurnDriver BurnDrvgg_glocj1 = {
	"gg_glocj1", "gg_gloc", NULL, NULL, "1990",
	"G-LOC Air Battle (Jpn, v0)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_glocj1RomInfo, gg_glocj1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// G-LOC Air Battle (Euro, USA, Bra)

static struct BurnRomInfo gg_glocRomDesc[] = {
	{ "g-loc air battle (usa, europe).bin",	0x20000, 0x18de59ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gloc)
STD_ROM_FN(gg_gloc)

struct BurnDriver BurnDrvgg_gloc = {
	"gg_gloc", NULL, NULL, NULL, "1991",
	"G-LOC Air Battle (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_glocRomInfo, gg_glocRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Galaga '91 (Jpn)

static struct BurnRomInfo gg_galaga91RomDesc[] = {
	{ "galaga '91 (japan).bin",	0x20000, 0x0593ba24, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_galaga91)
STD_ROM_FN(gg_galaga91)

struct BurnDriver BurnDrvgg_galaga91 = {
	"gg_galaga91", "gg_galaga2", NULL, NULL, "1991",
	"Galaga '91 (Jpn)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_galaga91RomInfo, gg_galaga91RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Galaga 2 (Euro)

static struct BurnRomInfo gg_galaga2RomDesc[] = {
	{ "galaga 2 (europe).bin",	0x20000, 0x95ecece2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_galaga2)
STD_ROM_FN(gg_galaga2)

struct BurnDriver BurnDrvgg_galaga2 = {
	"gg_galaga2", NULL, NULL, NULL, "1993",
	"Galaga 2 (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_galaga2RomInfo, gg_galaga2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gamble Panic (Jpn)

static struct BurnRomInfo gg_gambleRomDesc[] = {
	{ "gamble panic (japan).bin",	0x80000, 0x09534742, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gamble)
STD_ROM_FN(gg_gamble)

struct BurnDriver BurnDrvgg_gamble = {
	"gg_gamble", NULL, NULL, NULL, "1995",
	"Gamble Panic (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gambleRomInfo, gg_gambleRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gambler Jikochuushinha (Jpn)

static struct BurnRomInfo gg_gamblerRomDesc[] = {
	{ "gambler jikochuushinha (japan).bin",	0x40000, 0x423803a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gambler)
STD_ROM_FN(gg_gambler)

struct BurnDriver BurnDrvgg_gambler = {
	"gg_gambler", NULL, NULL, NULL, "1992",
	"Gambler Jikochuushinha (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gamblerRomInfo, gg_gamblerRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ganbare Gorby! (Jpn)

static struct BurnRomInfo gg_ggorbyRomDesc[] = {
	{ "ganbare gorby! (japan).bin",	0x20000, 0xa1f2f4a1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ggorby)
STD_ROM_FN(gg_ggorby)

struct BurnDriver BurnDrvgg_ggorby = {
	"gg_ggorby", "gg_factoryp", NULL, NULL, "1991",
	"Ganbare Gorby! (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ggorbyRomInfo, gg_ggorbyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Garfield - Caught in the Act (Euro, USA)

static struct BurnRomInfo gg_garfieldRomDesc[] = {
	{ "mpr-18201-s.ic2",	0x100000, 0xcd53f3af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_garfield)
STD_ROM_FN(gg_garfield)

struct BurnDriver BurnDrvgg_garfield = {
	"gg_garfield", NULL, NULL, NULL, "1995",
	"Garfield - Caught in the Act (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_garfieldRomInfo, gg_garfieldRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Garou Densetsu Special (Jpn)

static struct BurnRomInfo gg_garouspRomDesc[] = {
	{ "mpr-17432.ic1",	0x80000, 0x9afb6f33, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_garousp)
STD_ROM_FN(gg_garousp)

struct BurnDriver BurnDrvgg_garousp = {
	"gg_garousp", "gg_fatfursp", NULL, NULL, "1994",
	"Garou Densetsu Special (Jpn)\0", NULL, "Takara", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_garouspRomInfo, gg_garouspRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gear Stadium (Jpn)

static struct BurnRomInfo gg_gearstadRomDesc[] = {
	{ "gear stadium (japan).bin",	0x20000, 0x0e300223, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gearstad)
STD_ROM_FN(gg_gearstad)

struct BurnDriver BurnDrvgg_gearstad = {
	"gg_gearstad", "gg_batterup", NULL, NULL, "1991",
	"Gear Stadium (Jpn)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gearstadRomInfo, gg_gearstadRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gear Stadium Heiseiban (Jpn)

static struct BurnRomInfo gg_gearstahRomDesc[] = {
	{ "gear stadium heiseiban (japan).bin",	0x20000, 0xa0530664, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gearstah)
STD_ROM_FN(gg_gearstah)

struct BurnDriver BurnDrvgg_gearstah = {
	"gg_gearstah", NULL, NULL, NULL, "1995",
	"Gear Stadium Heiseiban (Jpn)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gearstahRomInfo, gg_gearstahRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gear Works (USA)

static struct BurnRomInfo gg_gearworkRomDesc[] = {
	{ "gear works (usa).bin",	0x20000, 0xe9a2efb4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gearwork)
STD_ROM_FN(gg_gearwork)

struct BurnDriver BurnDrvgg_gearwork = {
	"gg_gearwork", NULL, NULL, NULL, "1994",
	"Gear Works (USA)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gearworkRomInfo, gg_gearworkRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// George Foreman's KO Boxing (Euro, USA)

static struct BurnRomInfo gg_georgekoRomDesc[] = {
	{ "george foreman's ko boxing (usa, europe).bin",	0x20000, 0x58b44585, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_georgeko)
STD_ROM_FN(gg_georgeko)

struct BurnDriver BurnDrvgg_georgeko = {
	"gg_georgeko", NULL, NULL, NULL, "1992",
	"George Foreman's KO Boxing (Euro, USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_georgekoRomInfo, gg_georgekoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GG Aleste (Jpn)

static struct BurnRomInfo gg_ggalesteRomDesc[] = {
	{ "gg aleste (japan).bin",	0x40000, 0x1b80a75b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ggaleste)
STD_ROM_FN(gg_ggaleste)

struct BurnDriver BurnDrvgg_ggaleste = {
	"gg_ggaleste", NULL, NULL, NULL, "1991",
	"GG Aleste (Jpn)\0", NULL, "Compile", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ggalesteRomInfo, gg_ggalesteRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GG Doraemon - Noranosuke no Yabou (Jpn)

static struct BurnRomInfo gg_ggdoraRomDesc[] = {
	{ "gg doraemon - noranosuke no yabou (japan).bin",	0x40000, 0x9a8b2c16, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ggdora)
STD_ROM_FN(gg_ggdora)

struct BurnDriver BurnDrvgg_ggdora = {
	"gg_ggdora", NULL, NULL, NULL, "1993",
	"GG Doraemon - Noranosuke no Yabou (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ggdoraRomInfo, gg_ggdoraRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GG Portrait - Pai Chen (Jpn)

static struct BurnRomInfo gg_ggportRomDesc[] = {
	{ "gg portrait - pai chen (japan).bin",	0x80000, 0x695cc120, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ggport)
STD_ROM_FN(gg_ggport)

struct BurnDriver BurnDrvgg_ggport = {
	"gg_ggport", NULL, NULL, NULL, "1996",
	"GG Portrait - Pai Chen (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ggportRomInfo, gg_ggportRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GG Portrait - Yuuki Akira (Jpn)

static struct BurnRomInfo gg_ggport1RomDesc[] = {
	{ "gg portrait - yuuki akira (japan).bin",	0x80000, 0x51159f8f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ggport1)
STD_ROM_FN(gg_ggport1)

struct BurnDriver BurnDrvgg_ggport1 = {
	"gg_ggport1", NULL, NULL, NULL, "1996",
	"GG Portrait - Yuuki Akira (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ggport1RomInfo, gg_ggport1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The GG Shinobi (Jpn)

static struct BurnRomInfo gg_shinobijRomDesc[] = {
	{ "gg shinobi, the (japan).bin",	0x40000, 0x83926bd1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinobij)
STD_ROM_FN(gg_shinobij)

struct BurnDriver BurnDrvgg_shinobij = {
	"gg_shinobij", "gg_shinobi", NULL, NULL, "1991",
	"The GG Shinobi (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinobijRomInfo, gg_shinobijRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The GG Shinobi (Euro, USA)

static struct BurnRomInfo gg_shinobiRomDesc[] = {
	{ "shinobi (usa, europe).bin",	0x40000, 0x30f1c984, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinobi)
STD_ROM_FN(gg_shinobi)

struct BurnDriver BurnDrvgg_shinobi = {
	"gg_shinobi", NULL, NULL, NULL, "1991",
	"The GG Shinobi (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinobiRomInfo, gg_shinobiRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Godzilla - Kaijuu Daishingeki (Jpn)

static struct BurnRomInfo gg_godzillaRomDesc[] = {
	{ "godzilla - kaijuu daishingeki (japan).bin",	0x80000, 0x4cf97801, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_godzilla)
STD_ROM_FN(gg_godzilla)

struct BurnDriver BurnDrvgg_godzilla = {
	"gg_godzilla", NULL, NULL, NULL, "1995",
	"Godzilla - Kaijuu Daishingeki (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_godzillaRomInfo, gg_godzillaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GP Rider (Jpn)

static struct BurnRomInfo gg_gpriderjRomDesc[] = {
	{ "mpr-16589.ic1",	0x80000, 0x7a26ec6a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gpriderj)
STD_ROM_FN(gg_gpriderj)

struct BurnDriver BurnDrvgg_gpriderj = {
	"gg_gpriderj", "gg_gprider", NULL, NULL, "1994",
	"GP Rider (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gpriderjRomInfo, gg_gpriderjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GP Rider (Prototype, 19940104)

static struct BurnRomInfo gg_gpriderpRomDesc[] = {
	{ "gp rider (prototype - jan 04,  1994).bin",	0x80000, 0x12e4f6db, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gpriderp)
STD_ROM_FN(gg_gpriderp)

struct BurnDriver BurnDrvgg_gpriderp = {
	"gg_gpriderp", "gg_gprider", NULL, NULL, "1994",
	"GP Rider (Prototype, 19940104)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gpriderpRomInfo, gg_gpriderpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// GP Rider (Euro, USA)

static struct BurnRomInfo gg_gpriderRomDesc[] = {
	{ "mpr-16367-s.ic1",	0x80000, 0x876e9b72, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gprider)
STD_ROM_FN(gg_gprider)

struct BurnDriver BurnDrvgg_gprider = {
	"gg_gprider", NULL, NULL, NULL, "1994",
	"GP Rider (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gpriderRomInfo, gg_gpriderRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Greendog (Euro, USA)

static struct BurnRomInfo gg_greendogRomDesc[] = {
	{ "greendog (usa, europe).bin",	0x40000, 0xf27925b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_greendog)
STD_ROM_FN(gg_greendog)

struct BurnDriver BurnDrvgg_greendog = {
	"gg_greendog", NULL, NULL, NULL, "1993",
	"Greendog (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_greendogRomInfo, gg_greendogRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Griffin (Jpn)

static struct BurnRomInfo gg_griffinRomDesc[] = {
	{ "griffin (japan).bin",	0x20000, 0xa93e8b0f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_griffin)
STD_ROM_FN(gg_griffin)

struct BurnDriver BurnDrvgg_griffin = {
	"gg_griffin", NULL, NULL, NULL, "1991",
	"Griffin (Jpn)\0", NULL, "Riot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_griffinRomInfo, gg_griffinRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Gunstar Heroes (Jpn)

static struct BurnRomInfo gg_gunstarRomDesc[] = {
	{ "mpr-17847-f.ic1",	0x80000, 0xc3c52767, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_gunstar)
STD_ROM_FN(gg_gunstar)

struct BurnDriver BurnDrvgg_gunstar = {
	"gg_gunstar", NULL, NULL, NULL, "1995",
	"Gunstar Heroes (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_gunstarRomInfo, gg_gunstarRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Halley Wars (Jpn)

static struct BurnRomInfo gg_halleywjRomDesc[] = {
	{ "halley wars (japan).bin",	0x20000, 0xdef5a5d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_halleywj)
STD_ROM_FN(gg_halleywj)

struct BurnDriver BurnDrvgg_halleywj = {
	"gg_halleywj", "gg_halleyw", NULL, NULL, "1991",
	"Halley Wars (Jpn)\0", NULL, "Taito", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_halleywjRomInfo, gg_halleywjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Halley Wars (Euro, USA, Bra)

static struct BurnRomInfo gg_halleywRomDesc[] = {
	{ "halley wars (usa, europe).bin",	0x20000, 0x7e9dea46, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_halleyw)
STD_ROM_FN(gg_halleyw)

struct BurnDriver BurnDrvgg_halleyw = {
	"gg_halleyw", NULL, NULL, NULL, "1991",
	"Halley Wars (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_halleywRomInfo, gg_halleywRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Head Buster (Jpn)

static struct BurnRomInfo gg_headbustRomDesc[] = {
	{ "head buster (japan).bin",	0x20000, 0x7e689995, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_headbust)
STD_ROM_FN(gg_headbust)

struct BurnDriver BurnDrvgg_headbust = {
	"gg_headbust", NULL, NULL, NULL, "1991",
	"Head Buster (Jpn)\0", NULL, "Masaya", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_headbustRomInfo, gg_headbustRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Heavy Weight Champ (Jpn)

static struct BurnRomInfo gg_heavywRomDesc[] = {
	{ "heavy weight champ (japan).bin",	0x20000, 0xbeed9150, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_heavyw)
STD_ROM_FN(gg_heavyw)

struct BurnDriver BurnDrvgg_heavyw = {
	"gg_heavyw", NULL, NULL, NULL, "1991",
	"Heavy Weight Champ (Jpn)\0", NULL, "SIMS", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_heavywRomInfo, gg_heavywRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Home Alone (Euro, USA)

static struct BurnRomInfo gg_homeaRomDesc[] = {
	{ "home alone (usa, europe).bin",	0x40000, 0xdde29f74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_homea)
STD_ROM_FN(gg_homea)

struct BurnDriver BurnDrvgg_homea = {
	"gg_homea", NULL, NULL, NULL, "1992",
	"Home Alone (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_homeaRomInfo, gg_homeaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Honoo no Toukyuuji - Dodge Danpei (Jpn)

static struct BurnRomInfo gg_ddanpeiRomDesc[] = {
	{ "honoo no toukyuuji - dodge danpei (japan).bin",	0x20000, 0xdfa805a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ddanpei)
STD_ROM_FN(gg_ddanpei)

struct BurnDriver BurnDrvgg_ddanpei = {
	"gg_ddanpei", NULL, NULL, NULL, "1992",
	"Honoo no Toukyuuji - Dodge Danpei (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ddanpeiRomInfo, gg_ddanpeiRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hook (Euro)

static struct BurnRomInfo gg_hookRomDesc[] = {
	{ "hook (eu).bin",	0x40000, 0xa5aa5ba1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_hook)
STD_ROM_FN(gg_hook)

struct BurnDriver BurnDrvgg_hook = {
	"gg_hook", NULL, NULL, NULL, "1992",
	"Hook (Euro)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_hookRomInfo, gg_hookRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hook (USA)

static struct BurnRomInfo gg_hookuRomDesc[] = {
	{ "hook (usa).bin",	0x40000, 0xf53ced2e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_hooku)
STD_ROM_FN(gg_hooku)

struct BurnDriver BurnDrvgg_hooku = {
	"gg_hooku", "gg_hook", NULL, NULL, "1992",
	"Hook (USA)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_hookuRomInfo, gg_hookuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hurricanes (Euro)

static struct BurnRomInfo gg_hurricanRomDesc[] = {
	{ "mpr-17442-f.ic1",	0x80000, 0x0a25eec5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_hurrican)
STD_ROM_FN(gg_hurrican)

struct BurnDriver BurnDrvgg_hurrican = {
	"gg_hurrican", NULL, NULL, NULL, "1994",
	"Hurricanes (Euro)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	GGGetZipName, gg_hurricanRomInfo, gg_hurricanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hyokkori Hyoutanjima - Hyoutanjima no Daikoukai (Jpn)

static struct BurnRomInfo gg_hyokkohjRomDesc[] = {
	{ "mpr-14690-f.ic1",	0x20000, 0x42389270, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_hyokkohj)
STD_ROM_FN(gg_hyokkohj)

struct BurnDriver BurnDrvgg_hyokkohj = {
	"gg_hyokkohj", NULL, NULL, NULL, "1992",
	"Hyokkori Hyoutanjima - Hyoutanjima no Daikoukai (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_hyokkohjRomInfo, gg_hyokkohjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Hyper Pro Yakyuu '92 (Jpn)

static struct BurnRomInfo gg_hypyak92RomDesc[] = {
	{ "mpr-14759.ic1",	0x20000, 0x056cae74, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_hypyak92)
STD_ROM_FN(gg_hypyak92)

struct BurnDriver BurnDrvgg_hypyak92 = {
	"gg_hypyak92", NULL, NULL, NULL, "1992",
	"Hyper Pro Yakyuu '92 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_hypyak92RomInfo, gg_hypyak92RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Puzzlle and Action Ichidant~R GG (Jpn)

static struct BurnRomInfo gg_ichirggRomDesc[] = {
	{ "ichidant~r gg (japan).bin",	0x80000, 0x9f64c2bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ichirgg)
STD_ROM_FN(gg_ichirgg)

struct BurnDriver BurnDrvgg_ichirgg = {
	"gg_ichirgg", NULL, NULL, NULL, "1994",
	"Puzzlle and Action Ichidant~R GG (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ichirggRomInfo, gg_ichirggRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// In the Wake of Vampire (Jpn)

static struct BurnRomInfo gg_wakevampRomDesc[] = {
	{ "mpr-15156.ic1",	0x40000, 0xdab0f265, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wakevamp)
STD_ROM_FN(gg_wakevamp)

struct BurnDriver BurnDrvgg_wakevamp = {
	"gg_wakevamp", "gg_mastdark", NULL, NULL, "1992",
	"In the Wake of Vampire (Jpn)\0", NULL, "SIMS", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wakevampRomInfo, gg_wakevampRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Incredible Crash Dummies (World)

static struct BurnRomInfo gg_crashdumRomDesc[] = {
	{ "incredible crash dummies, the (usa, europe).bin",	0x40000, 0x087fc247, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_crashdum)
STD_ROM_FN(gg_crashdum)

struct BurnDriver BurnDrvgg_crashdum = {
	"gg_crashdum", NULL, NULL, NULL, "1992",
	"The Incredible Crash Dummies (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_crashdumRomInfo, gg_crashdumRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Incredible Hulk (Euro, USA)

static struct BurnRomInfo gg_hulkRomDesc[] = {
	{ "incredible hulk, the (usa, europe).bin",	0x80000, 0xd7055f88, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_hulk)
STD_ROM_FN(gg_hulk)

struct BurnDriver BurnDrvgg_hulk = {
	"gg_hulk", NULL, NULL, NULL, "1994",
	"The Incredible Hulk (Euro, USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_hulkRomInfo, gg_hulkRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Indiana Jones and the Last Crusade (Euro, USA)

static struct BurnRomInfo gg_indycrusRomDesc[] = {
	{ "indiana jones and the last crusade (usa, europe).bin",	0x40000, 0xb875226b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_indycrus)
STD_ROM_FN(gg_indycrus)

struct BurnDriver BurnDrvgg_indycrus = {
	"gg_indycrus", NULL, NULL, NULL, "1992",
	"Indiana Jones and the Last Crusade (Euro, USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_indycrusRomInfo, gg_indycrusRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Iron Man X-O Manowar in Heavy Metal (USA, Alt)

static struct BurnRomInfo gg_ironman1RomDesc[] = {
	{ "iron man x-o manowar in heavy metal (u) [a1].bin",	0x80000, 0x847ae5ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ironman1)
STD_ROM_FN(gg_ironman1)

struct BurnDriver BurnDrvgg_ironman1 = {
	"gg_ironman1", "gg_ironman", NULL, NULL, "1996",
	"Iron Man X-O Manowar in Heavy Metal (USA, Alt)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ironman1RomInfo, gg_ironman1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Iron Man X-O Manowar in Heavy Metal (Euro, USA)

static struct BurnRomInfo gg_ironmanRomDesc[] = {
	{ "iron man x-o manowar in heavy metal (usa, europe).bin",	0x80000, 0x8927b69b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ironman)
STD_ROM_FN(gg_ironman)

struct BurnDriver BurnDrvgg_ironman = {
	"gg_ironman", NULL, NULL, NULL, "1996",
	"Iron Man X-O Manowar in Heavy Metal (Euro, USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ironmanRomInfo, gg_ironmanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Itchy and Scratchy Game (Euro, USA)

static struct BurnRomInfo gg_itchyRomDesc[] = {
	{ "itchy and scratchy game, the (usa, europe).bin",	0x40000, 0x44e7e2da, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_itchy)
STD_ROM_FN(gg_itchy)

struct BurnDriver BurnDrvgg_itchy = {
	"gg_itchy", NULL, NULL, NULL, "1995",
	"The Itchy and Scratchy Game (Euro, USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_itchyRomInfo, gg_itchyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// J.League GG Pro Striker '94 (Jpn)

static struct BurnRomInfo gg_jleagu94RomDesc[] = {
	{ "mpr-16821.ic1",	0x80000, 0xa12a28a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jleagu94)
STD_ROM_FN(gg_jleagu94)

struct BurnDriver BurnDrvgg_jleagu94 = {
	"gg_jleagu94", NULL, NULL, NULL, "1994",
	"J.League GG Pro Striker '94 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jleagu94RomInfo, gg_jleagu94RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// J.League Soccer - Dream Eleven (Jpn)

static struct BurnRomInfo gg_jleagueRomDesc[] = {
	{ "j.league soccer - dream eleven (japan).bin",	0x80000, 0xabddf0eb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jleague)
STD_ROM_FN(gg_jleague)

struct BurnDriver BurnDrvgg_jleague = {
	"gg_jleague", NULL, NULL, NULL, "1995",
	"J.League Soccer - Dream Eleven (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jleagueRomInfo, gg_jleagueRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James Bond 007 - The Duel (Euro)

static struct BurnRomInfo gg_jb007RomDesc[] = {
	{ "james bond 007 - the duel (europe).bin",	0x40000, 0x881a4524, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jb007)
STD_ROM_FN(gg_jb007)

struct BurnDriver BurnDrvgg_jb007 = {
	"gg_jb007", NULL, NULL, NULL, "1993",
	"James Bond 007 - The Duel (Euro)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jb007RomInfo, gg_jb007RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James Pond 3 - Operation Starfish (Euro)

static struct BurnRomInfo gg_jpond3RomDesc[] = {
	{ "james pond 3 - operation starfi5h (europe).bin",	0x80000, 0x68bb7f71, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jpond3)
STD_ROM_FN(gg_jpond3)

struct BurnDriver BurnDrvgg_jpond3 = {
	"gg_jpond3", NULL, NULL, NULL, "1994",
	"James Pond 3 - Operation Starfish (Euro)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	GGGetZipName, gg_jpond3RomInfo, gg_jpond3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James Pond II - Codename RoboCod (Euro)

static struct BurnRomInfo gg_robocodRomDesc[] = {
	{ "james pond ii - codename robocod (eu).bin",	0x80000, 0xc2f7928b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_robocod)
STD_ROM_FN(gg_robocod)

struct BurnDriver BurnDrvgg_robocod = {
	"gg_robocod", NULL, NULL, NULL, "1993",
	"James Pond II - Codename RoboCod (Euro)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_robocodRomInfo, gg_robocodRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// James Pond II - Codename RoboCod (USA)

static struct BurnRomInfo gg_robocoduRomDesc[] = {
	{ "james pond ii - codename robocod (usa).bin",	0x80000, 0x9fb5c155, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_robocodu)
STD_ROM_FN(gg_robocodu)

struct BurnDriver BurnDrvgg_robocodu = {
	"gg_robocodu", "gg_robocod", NULL, NULL, "1993",
	"James Pond II - Codename RoboCod (USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_robocoduRomInfo, gg_robocoduRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jang Pung II (Kor, SMS Mode)

static struct BurnRomInfo gg_jangpun2RomDesc[] = {
	{ "jang pung ii (k) [s].bin",	0x40000, 0x76c5bdfb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jangpun2)
STD_ROM_FN(gg_jangpun2)

struct BurnDriver BurnDrvgg_jangpun2 = {
	"gg_jangpun2", NULL, NULL, NULL, "1993",
	"Jang Pung II (Kor, SMS Mode)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_jangpun2RomInfo, gg_jangpun2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jeopardy! (USA)

static struct BurnRomInfo gg_jeopardyRomDesc[] = {
	{ "jeopardy! (usa).bin",	0x40000, 0xd7820c21, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jeopardy)
STD_ROM_FN(gg_jeopardy)

struct BurnDriver BurnDrvgg_jeopardy = {
	"gg_jeopardy", NULL, NULL, NULL, "1993",
	"Jeopardy! (USA)\0", NULL, "GameTek", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jeopardyRomInfo, gg_jeopardyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jeopardy! - Sports Edition (Euro, USA)

static struct BurnRomInfo gg_jeopardsRomDesc[] = {
	{ "jeopardy! - sports edition (usa, europe).bin",	0x40000, 0x2dd850b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jeopards)
STD_ROM_FN(gg_jeopards)

struct BurnDriver BurnDrvgg_jeopards = {
	"gg_jeopards", NULL, NULL, NULL, "1994",
	"Jeopardy! - Sports Edition (Euro, USA)\0", NULL, "GameTek", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jeopardsRomInfo, gg_jeopardsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Joe Montana's Football (Jpn)

static struct BurnRomInfo gg_joemontjRomDesc[] = {
	{ "mpr-14319.ic1",	0x40000, 0x4a98678b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_joemontj)
STD_ROM_FN(gg_joemontj)

struct BurnDriver BurnDrvgg_joemontj = {
	"gg_joemontj", "gg_joemont", NULL, NULL, "1992",
	"Joe Montana's Football (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_joemontjRomInfo, gg_joemontjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Joe Montana's Football (Euro, USA)

static struct BurnRomInfo gg_joemontRomDesc[] = {
	{ "joe montana's football (usa, europe).bin",	0x40000, 0x2e01ba6c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_joemont)
STD_ROM_FN(gg_joemont)

struct BurnDriver BurnDrvgg_joemont = {
	"gg_joemont", NULL, NULL, NULL, "1991",
	"Joe Montana's Football (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_joemontRomInfo, gg_joemontRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Journey from Darkness - Strider Returns (Euro, USA)

static struct BurnRomInfo gg_striderrRomDesc[] = {
	{ "journey from darkness - strider returns (usa, europe).bin",	0x40000, 0x1ebfa5ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_striderr)
STD_ROM_FN(gg_striderr)

struct BurnDriver BurnDrvgg_striderr = {
	"gg_striderr", NULL, NULL, NULL, "1992",
	"Journey from Darkness - Strider Returns (Euro, USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_striderrRomInfo, gg_striderrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Judge Dredd (Euro, USA)

static struct BurnRomInfo gg_jdreddRomDesc[] = {
	{ "judge dredd (usa, europe).bin",	0x80000, 0x04d23fc4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jdredd)
STD_ROM_FN(gg_jdredd)

struct BurnDriver BurnDrvgg_jdredd = {
	"gg_jdredd", NULL, NULL, NULL, "1995",
	"Judge Dredd (Euro, USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jdreddRomInfo, gg_jdreddRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Junction (Jpn)

static struct BurnRomInfo gg_junctionjRomDesc[] = {
	{ "junction (japan).bin",	0x20000, 0xa8ef36a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_junctionj)
STD_ROM_FN(gg_junctionj)

struct BurnDriver BurnDrvgg_junctionj = {
	"gg_junctionj", "gg_junction", NULL, NULL, "1991",
	"Junction (Jpn)\0", NULL, "Micronet", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_junctionjRomInfo, gg_junctionjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Junction (USA)

static struct BurnRomInfo gg_junctionRomDesc[] = {
	{ "junction (us).bin",	0x20000, 0x32abaa02, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_junction)
STD_ROM_FN(gg_junction)

struct BurnDriver BurnDrvgg_junction = {
	"gg_junction", NULL, NULL, NULL, "1991",
	"Junction (USA)\0", NULL, "Micronet", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_junctionRomInfo, gg_junctionRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Jungle Book (Euro)

static struct BurnRomInfo gg_jungleRomDesc[] = {
	{ "jungle book, the (europe).bin",	0x40000, 0x90100884, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jungle)
STD_ROM_FN(gg_jungle)

struct BurnDriver BurnDrvgg_jungle = {
	"gg_jungle", NULL, NULL, NULL, "1994",
	"The Jungle Book (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jungleRomInfo, gg_jungleRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Jungle Book (USA)

static struct BurnRomInfo gg_jungleuRomDesc[] = {
	{ "jungle book, the (usa).bin",	0x40000, 0x30c09f31, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jungleu)
STD_ROM_FN(gg_jungleu)

struct BurnDriver BurnDrvgg_jungleu = {
	"gg_jungleu", "gg_jungle", NULL, NULL, "1994",
	"The Jungle Book (USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jungleuRomInfo, gg_jungleuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jungle Strike (USA)

static struct BurnRomInfo gg_jstrikeRomDesc[] = {
	{ "jungle strike (usa).bin",	0x80000, 0xd51642b0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jstrike)
STD_ROM_FN(gg_jstrike)

struct BurnDriver BurnDrvgg_jstrike = {
	"gg_jstrike", NULL, NULL, NULL, "1995",
	"Jungle Strike (USA)\0", NULL, "Black Pearl Software", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jstrikeRomInfo, gg_jstrikeRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jurassic Park (Jpn)

static struct BurnRomInfo gg_jparkjRomDesc[] = {
	{ "jurassic park (japan).bin",	0x80000, 0x2f536ae3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jparkj)
STD_ROM_FN(gg_jparkj)

struct BurnDriver BurnDrvgg_jparkj = {
	"gg_jparkj", "gg_jpark", NULL, NULL, "1993",
	"Jurassic Park (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jparkjRomInfo, gg_jparkjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Jurassic Park (Euro, USA)

static struct BurnRomInfo gg_jparkRomDesc[] = {
	{ "mpr-15779.ic1",	0x80000, 0xbd6f2321, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_jpark)
STD_ROM_FN(gg_jpark)

struct BurnDriver BurnDrvgg_jpark = {
	"gg_jpark", NULL, NULL, NULL, "1993",
	"Jurassic Park (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_jparkRomInfo, gg_jparkRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kaitou Saint Tale (Jpn)

static struct BurnRomInfo gg_sainttalRomDesc[] = {
	{ "kaitou saint tale (japan).bin",	0x80000, 0x937fd52b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sainttal)
STD_ROM_FN(gg_sainttal)

struct BurnDriver BurnDrvgg_sainttal = {
	"gg_sainttal", NULL, NULL, NULL, "1996",
	"Kaitou Saint Tale (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sainttalRomInfo, gg_sainttalRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kawasaki Superbike Challenge (Euro, USA)

static struct BurnRomInfo gg_kawasakiRomDesc[] = {
	{ "kawasaki superbike challenge (usa, europe).bin",	0x40000, 0x23f9150a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_kawasaki)
STD_ROM_FN(gg_kawasaki)

struct BurnDriver BurnDrvgg_kawasaki = {
	"gg_kawasaki", NULL, NULL, NULL, "1995",
	"Kawasaki Superbike Challenge (Euro, USA)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_kawasakiRomInfo, gg_kawasakiRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kenyuu Densetsu Yaiba (Jpn)

static struct BurnRomInfo gg_yaibaRomDesc[] = {
	{ "kenyuu densetsu yaiba (japan).bin",	0x80000, 0xd9ce3f4c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_yaiba)
STD_ROM_FN(gg_yaiba)

struct BurnDriver BurnDrvgg_yaiba = {
	"gg_yaiba", NULL, NULL, NULL, "1994",
	"Kenyuu Densetsu Yaiba (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_yaibaRomInfo, gg_yaibaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kick and Rush (Jpn)

static struct BurnRomInfo gg_kickrushRomDesc[] = {
	{ "kick and rush (japan).bin",	0x40000, 0xfd14ce00, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_kickrush)
STD_ROM_FN(gg_kickrush)

struct BurnDriver BurnDrvgg_kickrush = {
	"gg_kickrush", "gg_tengenwc", NULL, NULL, "1993",
	"Kick and Rush (Jpn)\0", NULL, "SIMS", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_kickrushRomInfo, gg_kickrushRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kinetic Connection (Jpn)

static struct BurnRomInfo gg_kineticcRomDesc[] = {
	{ "mpr-13610.ic1",	0x20000, 0x4af7f2aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_kineticc)
STD_ROM_FN(gg_kineticc)

struct BurnDriver BurnDrvgg_kineticc = {
	"gg_kineticc", NULL, NULL, NULL, "1991",
	"Kinetic Connection (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_kineticcRomInfo, gg_kineticcRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kishin Douji Zenki (Jpn)

static struct BurnRomInfo gg_zenkiRomDesc[] = {
	{ "mpr-18210.ic1",	0x80000, 0x7d622bdd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_zenki)
STD_ROM_FN(gg_zenki)

struct BurnDriver BurnDrvgg_zenki = {
	"gg_zenki", NULL, NULL, NULL, "1995",
	"Kishin Douji Zenki (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_zenkiRomInfo, gg_zenkiRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Klax (Euro, USA)

static struct BurnRomInfo gg_klaxRomDesc[] = {
	{ "mpr-15076-f.ic1",	0x20000, 0x9b40fc8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_klax)
STD_ROM_FN(gg_klax)

struct BurnDriver BurnDrvgg_klax = {
	"gg_klax", NULL, NULL, NULL, "1992",
	"Klax (Euro, USA)\0", NULL, "Tengen", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_klaxRomInfo, gg_klaxRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Krusty's Fun House (Euro, USA)

static struct BurnRomInfo gg_krustyfhRomDesc[] = {
	{ "mpr-15461-f.ic1",	0x40000, 0xd01e784f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_krustyfh)
STD_ROM_FN(gg_krustyfh)

struct BurnDriver BurnDrvgg_krustyfh = {
	"gg_krustyfh", NULL, NULL, NULL, "1992",
	"Krusty's Fun House (Euro, USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_krustyfhRomInfo, gg_krustyfhRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kuni-chan no Game Tengoku (Jpn)

static struct BurnRomInfo gg_kunichanRomDesc[] = {
	{ "kuni-chan no game tengoku (japan).bin",	0x40000, 0x398f2358, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_kunichan)
STD_ROM_FN(gg_kunichan)

struct BurnDriver BurnDrvgg_kunichan = {
	"gg_kunichan", NULL, NULL, NULL, "1991",
	"Kuni-chan no Game Tengoku (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_kunichanRomInfo, gg_kunichanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Kuni-chan no Game Tengoku Part 2 (Jpn)

static struct BurnRomInfo gg_kunichn2RomDesc[] = {
	{ "kuni-chan no game tengoku part 2 (japan).bin",	0x40000, 0xf3774c65, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_kunichn2)
STD_ROM_FN(gg_kunichn2)

struct BurnDriver BurnDrvgg_kunichn2 = {
	"gg_kunichn2", NULL, NULL, NULL, "1992",
	"Kuni-chan no Game Tengoku Part 2 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_kunichn2RomInfo, gg_kunichn2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Land of Illusion Starring Mickey Mouse (Euro, USA)

static struct BurnRomInfo gg_landillRomDesc[] = {
	{ "mpr-15486.ic1",	0x80000, 0x52dbf3e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_landill)
STD_ROM_FN(gg_landill)

struct BurnDriver BurnDrvgg_landill = {
	"gg_landill", NULL, NULL, NULL, "1993",
	"Land of Illusion Starring Mickey Mouse (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_landillRomInfo, gg_landillRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Last Action Hero (USA)

static struct BurnRomInfo gg_lastactRomDesc[] = {
	{ "last action hero (usa).bin",	0x40000, 0x2d367c43, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lastact)
STD_ROM_FN(gg_lastact)

struct BurnDriver BurnDrvgg_lastact = {
	"gg_lastact", NULL, NULL, NULL, "1992",
	"Last Action Hero (USA)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lastactRomInfo, gg_lastactRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Euro, USA)

static struct BurnRomInfo gg_legndillRomDesc[] = {
	{ "legend of illusion starring mickey mouse (usa, europe).bin",	0x80000, 0xce5ad8b7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndill)
STD_ROM_FN(gg_legndill)

struct BurnDriver BurnDrvgg_legndill = {
	"gg_legndill", NULL, NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndillRomInfo, gg_legndillRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Prototype, 19941011)

static struct BurnRomInfo gg_legndillp3RomDesc[] = {
	{ "legend of illusion starring mickey mouse (prototype - oct 11,  1994).bin",	0x80000, 0xf9d4cee9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndillp3)
STD_ROM_FN(gg_legndillp3)

struct BurnDriver BurnDrvgg_legndillp3 = {
	"gg_legndillp3", "gg_legndill", NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Prototype, 19941011)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndillp3RomInfo, gg_legndillp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Prototype, 19941014)

static struct BurnRomInfo gg_legndillp2RomDesc[] = {
	{ "legend of illusion starring mickey mouse (prototype - oct 14,  1994).bin",	0x80000, 0x670ede92, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndillp2)
STD_ROM_FN(gg_legndillp2)

struct BurnDriver BurnDrvgg_legndillp2 = {
	"gg_legndillp2", "gg_legndill", NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Prototype, 19941014)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndillp2RomInfo, gg_legndillp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Prototype, 19941017)

static struct BurnRomInfo gg_legndillp1RomDesc[] = {
	{ "legend of illusion starring mickey mouse (prototype - oct 17,  1994).bin",	0x80000, 0xde8545af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndillp1)
STD_ROM_FN(gg_legndillp1)

struct BurnDriver BurnDrvgg_legndillp1 = {
	"gg_legndillp1", "gg_legndill", NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Prototype, 19941017)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndillp1RomInfo, gg_legndillp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Prototype, 19940922)

static struct BurnRomInfo gg_legndillp6RomDesc[] = {
	{ "legend of illusion starring mickey mouse (prototype - sep 22,  1994).bin",	0x80000, 0x1cc16ad9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndillp6)
STD_ROM_FN(gg_legndillp6)

struct BurnDriver BurnDrvgg_legndillp6 = {
	"gg_legndillp6", "gg_legndill", NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Prototype, 19940922)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndillp6RomInfo, gg_legndillp6RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Prototype, 19940930-SEL)

static struct BurnRomInfo gg_legndillp5RomDesc[] = {
	{ "legend of illusion starring mickey mouse (prototype - sep 30,  1994 - sel).bin",	0x80000, 0xa0565a5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndillp5)
STD_ROM_FN(gg_legndillp5)

struct BurnDriver BurnDrvgg_legndillp5 = {
	"gg_legndillp5", "gg_legndill", NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Prototype, 19940930-SEL)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndillp5RomInfo, gg_legndillp5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Legend of Illusion Starring Mickey Mouse (Prototype, 19940930)

static struct BurnRomInfo gg_legndillp4RomDesc[] = {
	{ "legend of illusion starring mickey mouse (prototype - sep 30,  1994).bin",	0x80000, 0x63020617, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndillp4)
STD_ROM_FN(gg_legndillp4)

struct BurnDriver BurnDrvgg_legndillp4 = {
	"gg_legndillp4", "gg_legndill", NULL, NULL, "1994",
	"Legend of Illusion Starring Mickey Mouse (Prototype, 19940930)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndillp4RomInfo, gg_legndillp4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lemmings (World, Prototype)

static struct BurnRomInfo gg_lemmingspRomDesc[] = {
	{ "lemmings (world) (beta).bin",	0x40000, 0x51548f61, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lemmingsp)
STD_ROM_FN(gg_lemmingsp)

struct BurnDriver BurnDrvgg_lemmingsp = {
	"gg_lemmingsp", "gg_lemmings", NULL, NULL, "1992",
	"Lemmings (World, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lemmingspRomInfo, gg_lemmingspRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lemmings (World)

static struct BurnRomInfo gg_lemmingsRomDesc[] = {
	{ "lemmings (world).bin",	0x40000, 0x0fde7baa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lemmings)
STD_ROM_FN(gg_lemmings)

struct BurnDriver BurnDrvgg_lemmings = {
	"gg_lemmings", NULL, NULL, NULL, "1992",
	"Lemmings (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lemmingsRomInfo, gg_lemmingsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lemmings 2 - The Tribes (Euro, Prototype)

static struct BurnRomInfo gg_lemming2RomDesc[] = {
	{ "lemmings 2 - the tribes [proto].bin",	0x80000, 0xfbc807e1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lemming2)
STD_ROM_FN(gg_lemming2)

struct BurnDriver BurnDrvgg_lemming2 = {
	"gg_lemming2", NULL, NULL, NULL, "1994",
	"Lemmings 2 - The Tribes (Euro, Prototype)\0", NULL, "Psygnosis", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lemming2RomInfo, gg_lemming2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lion King (Euro)

static struct BurnRomInfo gg_lionkingRomDesc[] = {
	{ "mpr-17271.ic1",	0x80000, 0x0cd9c20b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionking)
STD_ROM_FN(gg_lionking)

struct BurnDriver BurnDrvgg_lionking = {
	"gg_lionking", NULL, NULL, NULL, "1994",
	"The Lion King (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingRomInfo, gg_lionkingRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lion King (Jpn)

static struct BurnRomInfo gg_lionkingjRomDesc[] = {
	{ "lion king, the (disney's) (jp).bin",	0x80000, 0xc78b8637, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingj)
STD_ROM_FN(gg_lionkingj)

struct BurnDriver BurnDrvgg_lionkingj = {
	"gg_lionkingj", "gg_lionking", NULL, NULL, "1995",
	"The Lion King (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingjRomInfo, gg_lionkingjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lion King (USA)

static struct BurnRomInfo gg_lionkinguRomDesc[] = {
	{ "mpr-17074.ic1",	0x80000, 0x9808d7b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingu)
STD_ROM_FN(gg_lionkingu)

struct BurnDriver BurnDrvgg_lionkingu = {
	"gg_lionkingu", "gg_lionking", NULL, NULL, "1994",
	"The Lion King (USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkinguRomInfo, gg_lionkinguRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's The Lion King (Prototype, 19940803)

static struct BurnRomInfo gg_lionkingp9RomDesc[] = {
	{ "lion king, the (prototype - aug 03,  1994).bin",	0x7d909, 0xaacb7387, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp9)
STD_ROM_FN(gg_lionkingp9)

struct BurnDriver BurnDrvgg_lionkingp9 = {
	"gg_lionkingp9", "gg_lionking", NULL, NULL, "1994",
	"Disney's The Lion King (Prototype, 19940803)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp9RomInfo, gg_lionkingp9RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's The Lion King (Prototype, 19940811)

static struct BurnRomInfo gg_lionkingp8RomDesc[] = {
	{ "lion king, the (prototype - aug 11,  1994).bin",	0x7d992, 0x214c13dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp8)
STD_ROM_FN(gg_lionkingp8)

struct BurnDriver BurnDrvgg_lionkingp8 = {
	"gg_lionkingp8", "gg_lionking", NULL, NULL, "1994",
	"Disney's The Lion King (Prototype, 19940811)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp8RomInfo, gg_lionkingp8RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's The Lion King (Prototype, 19940812)

static struct BurnRomInfo gg_lionkingp7RomDesc[] = {
	{ "lion king, the (prototype - aug 12,  1994).bin",	0x7d9ae, 0x0e80efe0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp7)
STD_ROM_FN(gg_lionkingp7)

struct BurnDriver BurnDrvgg_lionkingp7 = {
	"gg_lionkingp7", "gg_lionking", NULL, NULL, "1994",
	"Disney's The Lion King (Prototype, 19940812)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp7RomInfo, gg_lionkingp7RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's The Lion King (Prototype, 19940813)

static struct BurnRomInfo gg_lionkingp6RomDesc[] = {
	{ "lion king, the (prototype - aug 13,  1994).bin",	0x7d9b2, 0x8250c11e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp6)
STD_ROM_FN(gg_lionkingp6)

struct BurnDriver BurnDrvgg_lionkingp6 = {
	"gg_lionkingp6", "gg_lionking", NULL, NULL, "1994",
	"Disney's The Lion King (Prototype, 19940813)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp6RomInfo, gg_lionkingp6RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's The Lion King (Prototype, 19940814)

static struct BurnRomInfo gg_lionkingp5RomDesc[] = {
	{ "lion king, the (prototype - aug 14,  1994).bin",	0x7d9b6, 0xeb10a2d4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp5)
STD_ROM_FN(gg_lionkingp5)

struct BurnDriver BurnDrvgg_lionkingp5 = {
	"gg_lionkingp5", "gg_lionking", NULL, NULL, "1994",
	"Disney's The Lion King (Prototype, 19940814)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp5RomInfo, gg_lionkingp5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Disney's The Lion King (Prototype, 19940816)

static struct BurnRomInfo gg_lionkingp4RomDesc[] = {
	{ "lion king, the (prototype - aug 16,  1994).bin",	0x7d9b6, 0xc6af9643, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp4)
STD_ROM_FN(gg_lionkingp4)

struct BurnDriver BurnDrvgg_lionkingp4 = {
	"gg_lionkingp4", "gg_lionking", NULL, NULL, "1994",
	"Disney's The Lion King (Prototype, 19940816)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp4RomInfo, gg_lionkingp4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lion King (Prototype, 19940817-2)

static struct BurnRomInfo gg_lionkingp3RomDesc[] = {
	{ "lion king, the (prototype - aug 17,  1994 - 2).bin",	0x7da3a, 0xb65030d0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp3)
STD_ROM_FN(gg_lionkingp3)

struct BurnDriver BurnDrvgg_lionkingp3 = {
	"gg_lionkingp3", "gg_lionking", NULL, NULL, "1994",
	"The Lion King (Prototype, 19940817-2)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp3RomInfo, gg_lionkingp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lion King (Prototype, 19940817)

static struct BurnRomInfo gg_lionkingp2RomDesc[] = {
	{ "lion king, the (prototype - aug 17,  1994).bin",	0x7d9ea, 0x4214c474, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp2)
STD_ROM_FN(gg_lionkingp2)

struct BurnDriver BurnDrvgg_lionkingp2 = {
	"gg_lionkingp2", "gg_lionking", NULL, NULL, "1994",
	"The Lion King (Prototype, 19940817)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp2RomInfo, gg_lionkingp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lion King (Prototype, 19940820)

static struct BurnRomInfo gg_lionkingp1RomDesc[] = {
	{ "lion king, the (prototype - aug 20,  1994).bin",	0x7fe56, 0x3d0b31a2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lionkingp1)
STD_ROM_FN(gg_lionkingp1)

struct BurnDriver BurnDrvgg_lionkingp1 = {
	"gg_lionkingp1", "gg_lionking", NULL, NULL, "1994",
	"The Lion King (Prototype, 19940820)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lionkingp1RomInfo, gg_lionkingp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lost World - Jurassic Park (USA)

static struct BurnRomInfo gg_lostwrldRomDesc[] = {
	{ "mpr-19811-s.ic2",	0x100000, 0x8d1597f5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lostwrld)
STD_ROM_FN(gg_lostwrld)

struct BurnDriver BurnDrvgg_lostwrld = {
	"gg_lostwrld", NULL, NULL, NULL, "1997",
	"The Lost World - Jurassic Park (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lostwrldRomInfo, gg_lostwrldRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Lucky Dime Caper (Euro, USA)

static struct BurnRomInfo gg_luckydimRomDesc[] = {
	{ "mpr-14461 j09.ic1",	0x40000, 0x07a7815a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_luckydim)
STD_ROM_FN(gg_luckydim)

struct BurnDriver BurnDrvgg_luckydim = {
	"gg_luckydim", NULL, NULL, NULL, "1991",
	"The Lucky Dime Caper (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_luckydimRomInfo, gg_luckydimRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Lunar - Sanposuru Gakuen (Jpn)

static struct BurnRomInfo gg_lunarRomDesc[] = {
	{ "lunar - sanposuru gakuen (japan).bin",	0x80000, 0x58459edd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lunar)
STD_ROM_FN(gg_lunar)

struct BurnDriver BurnDrvgg_lunar = {
	"gg_lunar", NULL, NULL, NULL, "1996",
	"Lunar - Sanposuru Gakuen (Jpn)\0", NULL, "Game Arts", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lunarRomInfo, gg_lunarRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Madden NFL '95 (USA)

static struct BurnRomInfo gg_madden95RomDesc[] = {
	{ "madden nfl '95 (usa).bin",	0x80000, 0x75c71ebf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_madden95)
STD_ROM_FN(gg_madden95)

struct BurnDriver BurnDrvgg_madden95 = {
	"gg_madden95", NULL, NULL, NULL, "1995",
	"Madden NFL '95 (USA)\0", NULL, "Electronic Arts", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_madden95RomInfo, gg_madden95RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Madden NFL '96 (Euro, USA)

static struct BurnRomInfo gg_madden96RomDesc[] = {
	{ "madden nfl '96 (usa, europe).bin",	0x80000, 0x9f59d302, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_madden96)
STD_ROM_FN(gg_madden96)

struct BurnDriver BurnDrvgg_madden96 = {
	"gg_madden96", NULL, NULL, NULL, "1995",
	"Madden NFL '96 (Euro, USA)\0", NULL, "Black Pearl", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_madden96RomInfo, gg_madden96RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Madou Monogatari A - Dokidoki Vacation (Jpn)

static struct BurnRomInfo gg_madoumnaRomDesc[] = {
	{ "madou monogatari a - dokidoki vacation (japan).bin",	0x80000, 0x7ec95282, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_madoumna)
STD_ROM_FN(gg_madoumna)

struct BurnDriver BurnDrvgg_madoumna = {
	"gg_madoumna", NULL, NULL, NULL, "1995",
	"Madou Monogatari A - Dokidoki Vacation (Jpn)\0", NULL, "Compile", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_madoumnaRomInfo, gg_madoumnaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Madou Monogatari I - 3-Tsu no Madoukyuu (Jpn)

static struct BurnRomInfo gg_madoumonRomDesc[] = {
	{ "madou monogatari i - 3-tsu no madoukyuu (japan).bin",	0x80000, 0x00c34d94, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_madoumon)
STD_ROM_FN(gg_madoumon)

struct BurnDriver BurnDrvgg_madoumon = {
	"gg_madoumon", NULL, NULL, NULL, "1993",
	"Madou Monogatari I - 3-Tsu no Madoukyuu (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_madoumonRomInfo, gg_madoumonRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Madou Monogatari II - Arle 16-Sai (Jpn)

static struct BurnRomInfo gg_madoumn2RomDesc[] = {
	{ "mpr-16616.ic1",	0x80000, 0x12eb2287, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_madoumn2)
STD_ROM_FN(gg_madoumn2)

struct BurnDriver BurnDrvgg_madoumn2 = {
	"gg_madoumn2", NULL, NULL, NULL, "1994",
	"Madou Monogatari II - Arle 16-Sai (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_madoumn2RomInfo, gg_madoumn2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Madou Monogatari III - Kyuukyoku Joou-sama (Jpn, v1)

static struct BurnRomInfo gg_madoumn3RomDesc[] = {
	{ "madou monogatari iii - kyuukyoku joou-sama (japan) (v1.1).bin",	0x80000, 0x568f4825, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_madoumn3)
STD_ROM_FN(gg_madoumn3)

struct BurnDriver BurnDrvgg_madoumn3 = {
	"gg_madoumn3", NULL, NULL, NULL, "1994",
	"Madou Monogatari III - Kyuukyoku Joou-sama (Jpn, v1)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_madoumn3RomInfo, gg_madoumn3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Madou Monogatari III - Kyuukyoku Joou-sama (Jpn, v0)

static struct BurnRomInfo gg_madoumn3aRomDesc[] = {
	{ "madou monogatari iii - kyuukyoku joou-sama (japan).bin",	0x80000, 0x0a634d79, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_madoumn3a)
STD_ROM_FN(gg_madoumn3a)

struct BurnDriver BurnDrvgg_madoumn3a = {
	"gg_madoumn3a", "gg_madoumn3", NULL, NULL, "1994",
	"Madou Monogatari III - Kyuukyoku Joou-sama (Jpn, v0)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_madoumn3aRomInfo, gg_madoumn3aRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Magic Knight Rayearth (Jpn)

static struct BurnRomInfo gg_rayearthRomDesc[] = {
	{ "mpr-17115.ic1",	0x80000, 0x8f82a6b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_rayearth)
STD_ROM_FN(gg_rayearth)

struct BurnDriver BurnDrvgg_rayearth = {
	"gg_rayearth", NULL, NULL, NULL, "1994",
	"Magic Knight Rayearth (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_rayearthRomInfo, gg_rayearthRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Magic Knight Rayearth 2 - Making of Magic Knight (Jpn)

static struct BurnRomInfo gg_rayeart2RomDesc[] = {
	{ "mpr-18183-s.ic1",	0x80000, 0x1c2c2b04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_rayeart2)
STD_ROM_FN(gg_rayeart2)

struct BurnDriver BurnDrvgg_rayeart2 = {
	"gg_rayeart2", NULL, NULL, NULL, "1995",
	"Magic Knight Rayearth 2 - Making of Magic Knight (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_rayeart2RomInfo, gg_rayeart2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Magical Puzzle Popils (World)

static struct BurnRomInfo gg_magiclppRomDesc[] = {
	{ "magical puzzle popils (world) (en,ja).bin",	0x20000, 0xcf6d7bc5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_magiclpp)
STD_ROM_FN(gg_magiclpp)

struct BurnDriver BurnDrvgg_magiclpp = {
	"gg_magiclpp", NULL, NULL, NULL, "1991",
	"Magical Puzzle Popils (World)\0", NULL, "Tengen", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_magiclppRomInfo, gg_magiclppRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Magical Taruruuto-kun (Jpn)

static struct BurnRomInfo gg_mtaruruRomDesc[] = {
	{ "magical taruruuto-kun (japan).bin",	0x20000, 0x6e1cc23c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mtaruru)
STD_ROM_FN(gg_mtaruru)

struct BurnDriver BurnDrvgg_mtaruru = {
	"gg_mtaruru", NULL, NULL, NULL, "1991",
	"Magical Taruruuto-kun (Jpn)\0", NULL, "Tsukuda Original", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mtaruruRomInfo, gg_mtaruruRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Majors Pro Baseball (USA)

static struct BurnRomInfo gg_majorsRomDesc[] = {
	{ "majors pro baseball, the (usa).bin",	0x40000, 0x36ebcd6d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_majors)
STD_ROM_FN(gg_majors)

struct BurnDriver BurnDrvgg_majors = {
	"gg_majors", NULL, NULL, NULL, "1992",
	"The Majors Pro Baseball (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_majorsRomInfo, gg_majorsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mappy (Jpn)

static struct BurnRomInfo gg_mappyRomDesc[] = {
	{ "mappy (japan).bin",	0x20000, 0x01d2dd2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mappy)
STD_ROM_FN(gg_mappy)

struct BurnDriver BurnDrvgg_mappy = {
	"gg_mappy", NULL, NULL, NULL, "1991",
	"Mappy (Jpn)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mappyRomInfo, gg_mappyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Marble Madness (Euro, USA)

static struct BurnRomInfo gg_marbleRomDesc[] = {
	{ "marble madness (usa, europe).bin",	0x40000, 0x9559e339, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_marble)
STD_ROM_FN(gg_marble)

struct BurnDriver BurnDrvgg_marble = {
	"gg_marble", NULL, NULL, NULL, "1992",
	"Marble Madness (Euro, USA)\0", NULL, "Tengen", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_marbleRomInfo, gg_marbleRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Marko's Magic Football (Euro)

static struct BurnRomInfo gg_markoRomDesc[] = {
	{ "marko's magic football (europe) (en,fr,de,es).bin",	0x80000, 0x22c418bf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_marko)
STD_ROM_FN(gg_marko)

struct BurnDriver BurnDrvgg_marko = {
	"gg_marko", NULL, NULL, NULL, "1993",
	"Marko's Magic Football (Euro)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_markoRomInfo, gg_markoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Master of Darkness (Euro)

static struct BurnRomInfo gg_mastdarkRomDesc[] = {
	{ "master of darkness (eu).bin",	0x40000, 0x07d0eb42, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mastdark)
STD_ROM_FN(gg_mastdark)

struct BurnDriver BurnDrvgg_mastdark = {
	"gg_mastdark", NULL, NULL, NULL, "1993",
	"Master of Darkness (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mastdarkRomInfo, gg_mastdarkRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mega Man (Euro, USA)

static struct BurnRomInfo gg_megamanRomDesc[] = {
	{ "mega man (usa, europe).bin",	0x80000, 0x1ace93af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_megaman)
STD_ROM_FN(gg_megaman)

struct BurnDriver BurnDrvgg_megaman = {
	"gg_megaman", NULL, NULL, NULL, "1995",
	"Mega Man (Euro, USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_megamanRomInfo, gg_megamanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Megami Tensei Gaiden - Last Bible (Jpn)

static struct BurnRomInfo gg_lastbiblRomDesc[] = {
	{ "megami tensei gaiden - last bible (japan).bin",	0x80000, 0x2e4ec17b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lastbibl)
STD_ROM_FN(gg_lastbibl)

struct BurnDriver BurnDrvgg_lastbibl = {
	"gg_lastbibl", NULL, NULL, NULL, "1994",
	"Megami Tensei Gaiden - Last Bible (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lastbiblRomInfo, gg_lastbiblRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Megami Tensei Gaiden - Last Bible S (Jpn)

static struct BurnRomInfo gg_lastbibsRomDesc[] = {
	{ "megami tensei gaiden - last bible s (japan).bin",	0x80000, 0x4ec30806, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_lastbibs)
STD_ROM_FN(gg_lastbibs)

struct BurnDriver BurnDrvgg_lastbibs = {
	"gg_lastbibs", NULL, NULL, NULL, "1995",
	"Megami Tensei Gaiden - Last Bible S (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_lastbibsRomInfo, gg_lastbibsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mick and Mack as the Global Gladiators (Euro, USA)

static struct BurnRomInfo gg_mickmackRomDesc[] = {
	{ "mick and mack as the global gladiators (usa, europe).bin",	0x40000, 0xd2b6021e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mickmack)
STD_ROM_FN(gg_mickmack)

struct BurnDriver BurnDrvgg_mickmack = {
	"gg_mickmack", NULL, NULL, NULL, "1992",
	"Mick and Mack as the Global Gladiators (Euro, USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mickmackRomInfo, gg_mickmackRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mickey Mouse Densetsu no Oukoku - Legend of Illusion (Jpn)

static struct BurnRomInfo gg_legndilljRomDesc[] = {
	{ "mickey mouse densetsu no oukoku - legend of illusion (japan).bin",	0x80000, 0xfe12a92f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_legndillj)
STD_ROM_FN(gg_legndillj)

struct BurnDriver BurnDrvgg_legndillj = {
	"gg_legndillj", "gg_legndill", NULL, NULL, "1995",
	"Mickey Mouse Densetsu no Oukoku - Legend of Illusion (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_legndilljRomInfo, gg_legndilljRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mickey Mouse no Castle Illusion (Jpn, SMS Mode)

static struct BurnRomInfo gg_castlilljRomDesc[] = {
	{ "mickey mouse no castle illusion (japan).bin",	0x40000, 0x9942b69b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_castlillj)
STD_ROM_FN(gg_castlillj)

struct BurnDriver BurnDrvgg_castlillj = {
	"gg_castlillj", "gg_castlill", NULL, NULL, "1991",
	"Mickey Mouse no Castle Illusion (Jpn, SMS Mode)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE | HARDWARE_SMS_JAPANESE, GBF_MISC, 0,
	GGGetZipName, gg_castlilljRomInfo, gg_castlilljRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mickey Mouse no Mahou no Crystal (Jpn)

static struct BurnRomInfo gg_landilljRomDesc[] = {
	{ "mickey mouse no mahou no crystal (japan).bin",	0x80000, 0x0117c3df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_landillj)
STD_ROM_FN(gg_landillj)

struct BurnDriver BurnDrvgg_landillj = {
	"gg_landillj", "gg_landill", NULL, NULL, "1993",
	"Mickey Mouse no Mahou no Crystal (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_landilljRomInfo, gg_landilljRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mickey's Ultimate Challenge (Euro, USA)

static struct BurnRomInfo gg_mickeyucRomDesc[] = {
	{ "mickey's ultimate challenge (usa, europe).bin",	0x40000, 0xeccf7a4f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mickeyuc)
STD_ROM_FN(gg_mickeyuc)

struct BurnDriver BurnDrvgg_mickeyuc = {
	"gg_mickeyuc", NULL, NULL, NULL, "1994",
	"Mickey's Ultimate Challenge (Euro, USA)\0", NULL, "Hi Tech Expressions", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mickeyucRomInfo, gg_mickeyucRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Micro Machines (Euro)

static struct BurnRomInfo gg_micromacRomDesc[] = {
	{ "micro machines (europe).bin",	0x40000, 0xf7c524f6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_micromac)
STD_ROM_FN(gg_micromac)

struct BurnDriver BurnDrvgg_micromac = {
	"gg_micromac", NULL, NULL, NULL, "1993",
	"Micro Machines (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	GGGetZipName, gg_micromacRomInfo, gg_micromacRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Micro Machines 2 - Turbo Tournament (Euro)

static struct BurnRomInfo gg_micromc2RomDesc[] = {
	{ "micro machines 2 - turbo tournament (europe).bin",	0x80000, 0xdbe8895c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_micromc2)
STD_ROM_FN(gg_micromc2)

struct BurnDriver BurnDrvgg_micromc2 = {
	"gg_micromc2", NULL, NULL, NULL, "1995",
	"Micro Machines 2 - Turbo Tournament (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_micromc2RomInfo, gg_micromc2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mighty Morphin Power Rangers (Prototype, 19940716)

static struct BurnRomInfo gg_mmprpRomDesc[] = {
	{ "mighty morphin power rangers (prototype - jul 16,  1994).bin",	0x80000, 0x2887f04a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mmprp)
STD_ROM_FN(gg_mmprp)

struct BurnDriver BurnDrvgg_mmprp = {
	"gg_mmprp", "gg_mmpr", NULL, NULL, "1994",
	"Mighty Morphin Power Rangers (Prototype, 19940716)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mmprpRomInfo, gg_mmprpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mighty Morphin Power Rangers (Euro, USA)

static struct BurnRomInfo gg_mmprRomDesc[] = {
	{ "mighty morphin power rangers (usa, europe).bin",	0x80000, 0x9289dfcc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mmpr)
STD_ROM_FN(gg_mmpr)

struct BurnDriver BurnDrvgg_mmpr = {
	"gg_mmpr", NULL, NULL, NULL, "1994",
	"Mighty Morphin Power Rangers (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mmprRomInfo, gg_mmprRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mighty Morphin Power Rangers - The Movie (Prototype, 19950530)

static struct BurnRomInfo gg_mmprtmpRomDesc[] = {
	{ "mighty morphin power rangers - the movie (prototype - may 30,  1995).bin",	0x80000, 0x8c665203, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mmprtmp)
STD_ROM_FN(gg_mmprtmp)

struct BurnDriver BurnDrvgg_mmprtmp = {
	"gg_mmprtmp", "gg_mmprtm", NULL, NULL, "1995",
	"Mighty Morphin Power Rangers - The Movie (Prototype, 19950530)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mmprtmpRomInfo, gg_mmprtmpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mighty Morphin Power Rangers - The Movie (Euro, USA)

static struct BurnRomInfo gg_mmprtmRomDesc[] = {
	{ "mighty morphin power rangers - the movie (usa, europe).bin",	0x80000, 0xb47c19e5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mmprtm)
STD_ROM_FN(gg_mmprtm)

struct BurnDriver BurnDrvgg_mmprtm = {
	"gg_mmprtm", NULL, NULL, NULL, "1995",
	"Mighty Morphin Power Rangers - The Movie (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mmprtmRomInfo, gg_mmprtmRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// MLBPA Baseball (USA)

static struct BurnRomInfo gg_mlbpaRomDesc[] = {
	{ "mlbpa baseball (usa).bin",	0x80000, 0x1ecf07b4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mlbpa)
STD_ROM_FN(gg_mlbpa)

struct BurnDriver BurnDrvgg_mlbpa = {
	"gg_mlbpa", NULL, NULL, NULL, "1995",
	"MLBPA Baseball (USA)\0", NULL, "Electronic Arts", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mlbpaRomInfo, gg_mlbpaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Moldorian - Hikari to Yami no Sister (Jpn)

static struct BurnRomInfo gg_moldoranRomDesc[] = {
	{ "moldorian - hikari to yami no sister (japan).bin",	0x80000, 0x4d5d15fb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_moldoran)
STD_ROM_FN(gg_moldoran)

struct BurnDriver BurnDrvgg_moldoran = {
	"gg_moldoran", NULL, NULL, NULL, "1994",
	"Moldorian - Hikari to Yami no Sister (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_moldoranRomInfo, gg_moldoranRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Monster Truck Wars (Euro, USA)

static struct BurnRomInfo gg_monstwarRomDesc[] = {
	{ "mpr-17420-s.ic1",	0x40000, 0x453c5cec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_monstwar)
STD_ROM_FN(gg_monstwar)

struct BurnDriver BurnDrvgg_monstwar = {
	"gg_monstwar", NULL, NULL, NULL, "1994",
	"Monster Truck Wars (Euro, USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_monstwarRomInfo, gg_monstwarRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Monster World II - Dragon no Wana (Jpn)

static struct BurnRomInfo gg_mworld2RomDesc[] = {
	{ "monster world ii - dragon no wana (japan).bin",	0x40000, 0xea89e0e7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mworld2)
STD_ROM_FN(gg_mworld2)

struct BurnDriver BurnDrvgg_mworld2 = {
	"gg_mworld2", "gg_wboydtrp", NULL, NULL, "1992",
	"Monster World II - Dragon no Wana (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mworld2RomInfo, gg_mworld2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mortal Kombat (Jpn, v3.3)

static struct BurnRomInfo gg_mkjRomDesc[] = {
	{ "mortal kombat (japan) (v3.3).bin",	0x80000, 0xdbff0461, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mkj)
STD_ROM_FN(gg_mkj)

struct BurnDriver BurnDrvgg_mkj = {
	"gg_mkj", "gg_mk", NULL, NULL, "1993",
	"Mortal Kombat (Jpn, v3.3)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mkjRomInfo, gg_mkjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mortal Kombat (Euro, USA, v2.6)

static struct BurnRomInfo gg_mkRomDesc[] = {
	{ "mpr-15771.ic1",	0x80000, 0x07494f2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mk)
STD_ROM_FN(gg_mk)

struct BurnDriver BurnDrvgg_mk = {
	"gg_mk", NULL, NULL, NULL, "1993",
	"Mortal Kombat (Euro, USA, v2.6)\0", NULL, "Arena", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mkRomInfo, gg_mkRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mortal Kombat 3 (Euro)

static struct BurnRomInfo gg_mk3RomDesc[] = {
	{ "mortal kombat 3 (europe).bin",	0x80000, 0xc2be62bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mk3)
STD_ROM_FN(gg_mk3)

struct BurnDriver BurnDrvgg_mk3 = {
	"gg_mk3", NULL, NULL, NULL, "1995",
	"Mortal Kombat 3 (Euro)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mk3RomInfo, gg_mk3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Mortal Kombat II (World)

static struct BurnRomInfo gg_mk2RomDesc[] = {
	{ "mortal kombat ii (world).bin",	0x80000, 0x4b304e0f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mk2)
STD_ROM_FN(gg_mk2)

struct BurnDriver BurnDrvgg_mk2 = {
	"gg_mk2", NULL, NULL, NULL, "1994",
	"Mortal Kombat II (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mk2RomInfo, gg_mk2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ms. Pac-Man (USA)

static struct BurnRomInfo gg_mspacmanRomDesc[] = {
	{ "ms. pac-man (usa).bin",	0x20000, 0x5ee88bd5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_mspacman)
STD_ROM_FN(gg_mspacman)

struct BurnDriver BurnDrvgg_mspacman = {
	"gg_mspacman", NULL, NULL, NULL, "199?",
	"Ms. Pac-Man (USA)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_mspacmanRomInfo, gg_mspacmanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nazo Puyo (Jpn, v1)

static struct BurnRomInfo gg_nazopuyoRomDesc[] = {
	{ "nazo puyo (japan)(v1.1).bin",	0x20000, 0xd8d11f8d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nazopuyo)
STD_ROM_FN(gg_nazopuyo)

struct BurnDriver BurnDrvgg_nazopuyo = {
	"gg_nazopuyo", NULL, NULL, NULL, "1993",
	"Nazo Puyo (Jpn, v1)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nazopuyoRomInfo, gg_nazopuyoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nazo Puyo (Jpn, v0)

static struct BurnRomInfo gg_nazopuyo1RomDesc[] = {
	{ "nazo puyo (japan).bin",	0x20000, 0xbcce5fd4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nazopuyo1)
STD_ROM_FN(gg_nazopuyo1)

struct BurnDriver BurnDrvgg_nazopuyo1 = {
	"gg_nazopuyo1", "gg_nazopuyo", NULL, NULL, "1993",
	"Nazo Puyo (Jpn, v0)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nazopuyo1RomInfo, gg_nazopuyo1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nazo Puyo 2 (Jpn)

static struct BurnRomInfo gg_nazpuyo2RomDesc[] = {
	{ "nazo puyo 2 (japan).bin",	0x40000, 0x73939de4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nazpuyo2)
STD_ROM_FN(gg_nazpuyo2)

struct BurnDriver BurnDrvgg_nazpuyo2 = {
	"gg_nazpuyo2", NULL, NULL, NULL, "1993",
	"Nazo Puyo 2 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nazpuyo2RomInfo, gg_nazpuyo2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nazo Puyo Arle no Roux (Jpn, Prototype)

static struct BurnRomInfo gg_nazpyoarpRomDesc[] = {
	{ "nazo puyo arle no roux (japan) (development edition).bin",	0x40000, 0x4c874466, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nazpyoarp)
STD_ROM_FN(gg_nazpyoarp)

struct BurnDriver BurnDrvgg_nazpyoarp = {
	"gg_nazpyoarp", "gg_nazpyoar", NULL, NULL, "1994",
	"Nazo Puyo Arle no Roux (Jpn, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nazpyoarpRomInfo, gg_nazpyoarpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nazo Puyo Arle no Roux (Jpn)

static struct BurnRomInfo gg_nazpyoarRomDesc[] = {
	{ "mpr-16849.ic1",	0x40000, 0x54ab42a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nazpyoar)
STD_ROM_FN(gg_nazpyoar)

struct BurnDriver BurnDrvgg_nazpyoar = {
	"gg_nazpyoar", NULL, NULL, NULL, "1994",
	"Nazo Puyo Arle no Roux (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nazpyoarRomInfo, gg_nazpyoarRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (USA)

static struct BurnRomInfo gg_nbaactRomDesc[] = {
	{ "nba action (usa).bin",	0x40000, 0x19030108, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaact)
STD_ROM_FN(gg_nbaact)

struct BurnDriver BurnDrvgg_nbaact = {
	"gg_nbaact", NULL, NULL, NULL, "1994",
	"NBA Action Starring David Robinson (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactRomInfo, gg_nbaactRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final A,  19940228)

static struct BurnRomInfo gg_nbaactp08RomDesc[] = {
	{ "nba action starring david robinson (final a - feb 28,  1994).bin",	0x40000, 0xd15aa940, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp08)
STD_ROM_FN(gg_nbaactp08)

struct BurnDriver BurnDrvgg_nbaactp08 = {
	"gg_nbaactp08", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final A,  19940228)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp08RomInfo, gg_nbaactp08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final B,  19940301)

static struct BurnRomInfo gg_nbaactp07RomDesc[] = {
	{ "nba action starring david robinson (final b - mar 01,  1994).bin",	0x40000, 0x4415c0bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp07)
STD_ROM_FN(gg_nbaactp07)

struct BurnDriver BurnDrvgg_nbaactp07 = {
	"gg_nbaactp07", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final B,  19940301)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp07RomInfo, gg_nbaactp07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final C,  19940302)

static struct BurnRomInfo gg_nbaactp06RomDesc[] = {
	{ "nba action starring david robinson (final c - mar 02,  1994).bin",	0x40000, 0x5254577a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp06)
STD_ROM_FN(gg_nbaactp06)

struct BurnDriver BurnDrvgg_nbaactp06 = {
	"gg_nbaactp06", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final C,  19940302)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp06RomInfo, gg_nbaactp06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final D,  19940303)

static struct BurnRomInfo gg_nbaactp05RomDesc[] = {
	{ "nba action starring david robinson (final d - mar 03,  1994).bin",	0x40000, 0x75a5bc5d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp05)
STD_ROM_FN(gg_nbaactp05)

struct BurnDriver BurnDrvgg_nbaactp05 = {
	"gg_nbaactp05", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final D,  19940303)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp05RomInfo, gg_nbaactp05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final E,  19940304)

static struct BurnRomInfo gg_nbaactp04RomDesc[] = {
	{ "nba action starring david robinson (final e - mar 04,  1994).bin",	0x40000, 0x80c0586c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp04)
STD_ROM_FN(gg_nbaactp04)

struct BurnDriver BurnDrvgg_nbaactp04 = {
	"gg_nbaactp04", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final E,  19940304)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp04RomInfo, gg_nbaactp04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final F,  19940304)

static struct BurnRomInfo gg_nbaactp03RomDesc[] = {
	{ "nba action starring david robinson (final f - mar 04,  1994).bin",	0x40000, 0xa82049df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp03)
STD_ROM_FN(gg_nbaactp03)

struct BurnDriver BurnDrvgg_nbaactp03 = {
	"gg_nbaactp03", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final F,  19940304)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp03RomInfo, gg_nbaactp03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final G,  19940305)

static struct BurnRomInfo gg_nbaactp02RomDesc[] = {
	{ "nba action starring david robinson (final g - mar 05,  1994).bin",	0x40000, 0xe197a0bb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp02)
STD_ROM_FN(gg_nbaactp02)

struct BurnDriver BurnDrvgg_nbaactp02 = {
	"gg_nbaactp02", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final G,  19940305)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp02RomInfo, gg_nbaactp02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Final H,  19940308)

static struct BurnRomInfo gg_nbaactp01RomDesc[] = {
	{ "nba action starring david robinson (final h - mar 08,  1994).bin",	0x40000, 0xc9e98a49, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp01)
STD_ROM_FN(gg_nbaactp01)

struct BurnDriver BurnDrvgg_nbaactp01 = {
	"gg_nbaactp01", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Final H,  19940308)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp01RomInfo, gg_nbaactp01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940202)

static struct BurnRomInfo gg_nbaactp23RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 02,  1994).bin",	0x40000, 0xf9d5500b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp23)
STD_ROM_FN(gg_nbaactp23)

struct BurnDriver BurnDrvgg_nbaactp23 = {
	"gg_nbaactp23", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940202)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp23RomInfo, gg_nbaactp23RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940203)

static struct BurnRomInfo gg_nbaactp22RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 03,  1994).bin",	0x40000, 0xe2341dce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp22)
STD_ROM_FN(gg_nbaactp22)

struct BurnDriver BurnDrvgg_nbaactp22 = {
	"gg_nbaactp22", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940203)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp22RomInfo, gg_nbaactp22RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940206)

static struct BurnRomInfo gg_nbaactp21RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 06,  1994).bin",	0x40000, 0x07649147, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp21)
STD_ROM_FN(gg_nbaactp21)

struct BurnDriver BurnDrvgg_nbaactp21 = {
	"gg_nbaactp21", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940206)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp21RomInfo, gg_nbaactp21RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940208)

static struct BurnRomInfo gg_nbaactp20RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 08,  1994).bin",	0x40000, 0x5e089fbc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp20)
STD_ROM_FN(gg_nbaactp20)

struct BurnDriver BurnDrvgg_nbaactp20 = {
	"gg_nbaactp20", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940208)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp20RomInfo, gg_nbaactp20RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940212)

static struct BurnRomInfo gg_nbaactp19RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 12,  1994).bin",	0x40000, 0x0c0f6b4c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp19)
STD_ROM_FN(gg_nbaactp19)

struct BurnDriver BurnDrvgg_nbaactp19 = {
	"gg_nbaactp19", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940212)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp19RomInfo, gg_nbaactp19RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19941215)

static struct BurnRomInfo gg_nbaactp18RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 15,  1994).bin",	0x40000, 0xe73b02a4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp18)
STD_ROM_FN(gg_nbaactp18)

struct BurnDriver BurnDrvgg_nbaactp18 = {
	"gg_nbaactp18", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19941215)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp18RomInfo, gg_nbaactp18RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940217)

static struct BurnRomInfo gg_nbaactp17RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 17,  1994).bin",	0x40000, 0xc4b3328c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp17)
STD_ROM_FN(gg_nbaactp17)

struct BurnDriver BurnDrvgg_nbaactp17 = {
	"gg_nbaactp17", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940217)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp17RomInfo, gg_nbaactp17RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940220)

static struct BurnRomInfo gg_nbaactp16RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 20,  1994).bin",	0x40000, 0x91e3026c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp16)
STD_ROM_FN(gg_nbaactp16)

struct BurnDriver BurnDrvgg_nbaactp16 = {
	"gg_nbaactp16", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940220)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp16RomInfo, gg_nbaactp16RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940221)

static struct BurnRomInfo gg_nbaactp15RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 21,  1994).bin",	0x40000, 0x9265f8c3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp15)
STD_ROM_FN(gg_nbaactp15)

struct BurnDriver BurnDrvgg_nbaactp15 = {
	"gg_nbaactp15", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940221)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp15RomInfo, gg_nbaactp15RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940224)

static struct BurnRomInfo gg_nbaactp14RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 24,  1994).bin",	0x40000, 0x21a79f63, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp14)
STD_ROM_FN(gg_nbaactp14)

struct BurnDriver BurnDrvgg_nbaactp14 = {
	"gg_nbaactp14", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940224)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp14RomInfo, gg_nbaactp14RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940225-B)

static struct BurnRomInfo gg_nbaactp13RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 25,  1994 - b).bin",	0x40000, 0x188a319e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp13)
STD_ROM_FN(gg_nbaactp13)

struct BurnDriver BurnDrvgg_nbaactp13 = {
	"gg_nbaactp13", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940225-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp13RomInfo, gg_nbaactp13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940225)

static struct BurnRomInfo gg_nbaactp12RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 25,  1994).bin",	0x40000, 0xafc8a69e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp12)
STD_ROM_FN(gg_nbaactp12)

struct BurnDriver BurnDrvgg_nbaactp12 = {
	"gg_nbaactp12", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940225)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp12RomInfo, gg_nbaactp12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940226)

static struct BurnRomInfo gg_nbaactp11RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 26,  1994).bin",	0x40000, 0x6896b65d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp11)
STD_ROM_FN(gg_nbaactp11)

struct BurnDriver BurnDrvgg_nbaactp11 = {
	"gg_nbaactp11", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940226)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp11RomInfo, gg_nbaactp11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940227)

static struct BurnRomInfo gg_nbaactp10RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 27,  1994).bin",	0x40000, 0xab721f11, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp10)
STD_ROM_FN(gg_nbaactp10)

struct BurnDriver BurnDrvgg_nbaactp10 = {
	"gg_nbaactp10", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940227)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp10RomInfo, gg_nbaactp10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940228)

static struct BurnRomInfo gg_nbaactp09RomDesc[] = {
	{ "nba action starring david robinson (prototype - feb 28,  1994).bin",	0x40000, 0x82e882d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp09)
STD_ROM_FN(gg_nbaactp09)

struct BurnDriver BurnDrvgg_nbaactp09 = {
	"gg_nbaactp09", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940228)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp09RomInfo, gg_nbaactp09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940104)

static struct BurnRomInfo gg_nbaactp28RomDesc[] = {
	{ "nba action starring david robinson (prototype - jan 04,  1994).bin",	0x40000, 0xc658b5b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp28)
STD_ROM_FN(gg_nbaactp28)

struct BurnDriver BurnDrvgg_nbaactp28 = {
	"gg_nbaactp28", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940104)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp28RomInfo, gg_nbaactp28RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940106)

static struct BurnRomInfo gg_nbaactp27RomDesc[] = {
	{ "nba action starring david robinson (prototype - jan 06,  1994).bin",	0x40000, 0x41dabd84, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp27)
STD_ROM_FN(gg_nbaactp27)

struct BurnDriver BurnDrvgg_nbaactp27 = {
	"gg_nbaactp27", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940106)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp27RomInfo, gg_nbaactp27RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940110)

static struct BurnRomInfo gg_nbaactp26RomDesc[] = {
	{ "nba action starring david robinson (prototype - jan 10,  1994).bin",	0x40000, 0xa34b2b4c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp26)
STD_ROM_FN(gg_nbaactp26)

struct BurnDriver BurnDrvgg_nbaactp26 = {
	"gg_nbaactp26", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940110)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp26RomInfo, gg_nbaactp26RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940124)

static struct BurnRomInfo gg_nbaactp25RomDesc[] = {
	{ "nba action starring david robinson (prototype - jan 24,  1994).bin",	0x40000, 0x9824f77d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp25)
STD_ROM_FN(gg_nbaactp25)

struct BurnDriver BurnDrvgg_nbaactp25 = {
	"gg_nbaactp25", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940124)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp25RomInfo, gg_nbaactp25RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19940129)

static struct BurnRomInfo gg_nbaactp24RomDesc[] = {
	{ "nba action starring david robinson (prototype - jan 29,  1994).bin",	0x40000, 0x0d6bf79d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp24)
STD_ROM_FN(gg_nbaactp24)

struct BurnDriver BurnDrvgg_nbaactp24 = {
	"gg_nbaactp24", "gg_nbaact", NULL, NULL, "1994",
	"NBA Action Starring David Robinson (Prototype, 19940129)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp24RomInfo, gg_nbaactp24RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19931201)

static struct BurnRomInfo gg_nbaactp29RomDesc[] = {
	{ "nba action starring david robinson (prototype - dec 01,  1993).bin",	0x40000, 0x8c089da5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp29)
STD_ROM_FN(gg_nbaactp29)

struct BurnDriver BurnDrvgg_nbaactp29 = {
	"gg_nbaactp29", "gg_nbaact", NULL, NULL, "1993",
	"NBA Action Starring David Robinson (Prototype, 19931201)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp29RomInfo, gg_nbaactp29RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Action Starring David Robinson (Prototype, 19931128)

static struct BurnRomInfo gg_nbaactp30RomDesc[] = {
	{ "nba action starring david robinson (prototype - nov 28,  1993).bin",	0x40000, 0x31084021, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbaactp30)
STD_ROM_FN(gg_nbaactp30)

struct BurnDriver BurnDrvgg_nbaactp30 = {
	"gg_nbaactp30", "gg_nbaact", NULL, NULL, "1993",
	"NBA Action Starring David Robinson (Prototype, 19931128)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbaactp30RomInfo, gg_nbaactp30RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Jam (Jpn)

static struct BurnRomInfo gg_nbajamjRomDesc[] = {
	{ "nba jam (japan).bin",	0x80000, 0xa49e9033, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbajamj)
STD_ROM_FN(gg_nbajamj)

struct BurnDriver BurnDrvgg_nbajamj = {
	"gg_nbajamj", "gg_nbajam", NULL, NULL, "1994",
	"NBA Jam (Jpn)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbajamjRomInfo, gg_nbajamjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Jam (USA, v1.1)

static struct BurnRomInfo gg_nbajamRomDesc[] = {
	{ "nba jam (usa) (v1.1).bin",	0x80000, 0x820fa4ab, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbajam)
STD_ROM_FN(gg_nbajam)

struct BurnDriver BurnDrvgg_nbajam = {
	"gg_nbajam", NULL, NULL, NULL, "1993",
	"NBA Jam (USA, v1.1)\0", NULL, "Arena", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbajamRomInfo, gg_nbajamRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Jam (Euro, USA)

static struct BurnRomInfo gg_nbajam1RomDesc[] = {
	{ "mpr-16421-s.ic1",	0x80000, 0x8f17597e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbajam1)
STD_ROM_FN(gg_nbajam1)

struct BurnDriver BurnDrvgg_nbajam1 = {
	"gg_nbajam1", "gg_nbajam", NULL, NULL, "1993",
	"NBA Jam (Euro, USA)\0", NULL, "Arena", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbajam1RomInfo, gg_nbajam1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NBA Jam Tournament Edition (World)

static struct BurnRomInfo gg_nbajamteRomDesc[] = {
	{ "mpr-17625.ic1",	0x80000, 0x86c32e5b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nbajamte)
STD_ROM_FN(gg_nbajamte)

struct BurnDriver BurnDrvgg_nbajamte = {
	"gg_nbajamte", NULL, NULL, NULL, "1994",
	"NBA Jam Tournament Edition (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nbajamteRomInfo, gg_nbajamteRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pet Club Neko Daisuki! (Jpn)

static struct BurnRomInfo gg_nekodaisRomDesc[] = {
	{ "neko daisuki! (japan).bin",	0x80000, 0x3679be80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nekodais)
STD_ROM_FN(gg_nekodais)

struct BurnDriver BurnDrvgg_nekodais = {
	"gg_nekodais", NULL, NULL, NULL, "1996",
	"Pet Club Neko Daisuki! (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nekodaisRomInfo, gg_nekodaisRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (USA)

static struct BurnRomInfo gg_nfl95RomDesc[] = {
	{ "nfl '95 (usa).bin",	0x80000, 0xdc5b0407, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95)
STD_ROM_FN(gg_nfl95)

struct BurnDriver BurnDrvgg_nfl95 = {
	"gg_nfl95", NULL, NULL, NULL, "1994",
	"NFL '95 (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95RomInfo, gg_nfl95RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940808)

static struct BurnRomInfo gg_nfl95p13RomDesc[] = {
	{ "nfl '95 (prototype - aug 08,  1994).bin",	0x80000, 0x8cd96d95, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p13)
STD_ROM_FN(gg_nfl95p13)

struct BurnDriver BurnDrvgg_nfl95p13 = {
	"gg_nfl95p13", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940808)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p13RomInfo, gg_nfl95p13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940825-A)

static struct BurnRomInfo gg_nfl95p12RomDesc[] = {
	{ "nfl '95 (prototype - aug 25,  1994 - a).bin",	0x80000, 0x7f769052, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p12)
STD_ROM_FN(gg_nfl95p12)

struct BurnDriver BurnDrvgg_nfl95p12 = {
	"gg_nfl95p12", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940825-A)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p12RomInfo, gg_nfl95p12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940825)

static struct BurnRomInfo gg_nfl95p11RomDesc[] = {
	{ "nfl '95 (prototype - aug 25,  1994).bin",	0x80000, 0xfe140eaa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p11)
STD_ROM_FN(gg_nfl95p11)

struct BurnDriver BurnDrvgg_nfl95p11 = {
	"gg_nfl95p11", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940825)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p11RomInfo, gg_nfl95p11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940826)

static struct BurnRomInfo gg_nfl95p10RomDesc[] = {
	{ "nfl '95 (prototype - aug 26,  1994).bin",	0x80000, 0xf77138dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p10)
STD_ROM_FN(gg_nfl95p10)

struct BurnDriver BurnDrvgg_nfl95p10 = {
	"gg_nfl95p10", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940826)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p10RomInfo, gg_nfl95p10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940831)

static struct BurnRomInfo gg_nfl95p09RomDesc[] = {
	{ "nfl '95 (prototype - aug 31,  1994).bin",	0x80000, 0xa3b046e7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p09)
STD_ROM_FN(gg_nfl95p09)

struct BurnDriver BurnDrvgg_nfl95p09 = {
	"gg_nfl95p09", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940831)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p09RomInfo, gg_nfl95p09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940902)

static struct BurnRomInfo gg_nfl95p08RomDesc[] = {
	{ "nfl '95 (prototype - sep 02,  1994).bin",	0x80000, 0x3978387b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p08)
STD_ROM_FN(gg_nfl95p08)

struct BurnDriver BurnDrvgg_nfl95p08 = {
	"gg_nfl95p08", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940902)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p08RomInfo, gg_nfl95p08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940908)

static struct BurnRomInfo gg_nfl95p07RomDesc[] = {
	{ "nfl '95 (prototype - sep 08,  1994).bin",	0x80000, 0xff70a26f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p07)
STD_ROM_FN(gg_nfl95p07)

struct BurnDriver BurnDrvgg_nfl95p07 = {
	"gg_nfl95p07", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940908)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p07RomInfo, gg_nfl95p07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940910)

static struct BurnRomInfo gg_nfl95p06RomDesc[] = {
	{ "nfl '95 (prototype - sep 10,  1994).bin",	0x80000, 0x132db489, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p06)
STD_ROM_FN(gg_nfl95p06)

struct BurnDriver BurnDrvgg_nfl95p06 = {
	"gg_nfl95p06", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940910)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p06RomInfo, gg_nfl95p06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940911-B)

static struct BurnRomInfo gg_nfl95p05RomDesc[] = {
	{ "nfl '95 (prototype - sep 11,  1994 - b).bin",	0x80000, 0xc57c6afd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p05)
STD_ROM_FN(gg_nfl95p05)

struct BurnDriver BurnDrvgg_nfl95p05 = {
	"gg_nfl95p05", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940911-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p05RomInfo, gg_nfl95p05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940911-C)

static struct BurnRomInfo gg_nfl95p04RomDesc[] = {
	{ "nfl '95 (prototype - sep 11,  1994 - c).bin",	0x80000, 0x088e4b6d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p04)
STD_ROM_FN(gg_nfl95p04)

struct BurnDriver BurnDrvgg_nfl95p04 = {
	"gg_nfl95p04", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940911-C)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p04RomInfo, gg_nfl95p04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940912)

static struct BurnRomInfo gg_nfl95p03RomDesc[] = {
	{ "nfl '95 (prototype - sep 12,  1994).bin",	0x80000, 0xb38f48b6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p03)
STD_ROM_FN(gg_nfl95p03)

struct BurnDriver BurnDrvgg_nfl95p03 = {
	"gg_nfl95p03", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940912)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p03RomInfo, gg_nfl95p03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940914-F)

static struct BurnRomInfo gg_nfl95p02RomDesc[] = {
	{ "nfl '95 (prototype - sep 14,  1994 - f).bin",	0x80000, 0x2828cb66, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p02)
STD_ROM_FN(gg_nfl95p02)

struct BurnDriver BurnDrvgg_nfl95p02 = {
	"gg_nfl95p02", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940914-F)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p02RomInfo, gg_nfl95p02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL '95 (Prototype, 19940914)

static struct BurnRomInfo gg_nfl95p01RomDesc[] = {
	{ "nfl '95 (prototype - sep 14,  1994).bin",	0x80000, 0x474a162b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nfl95p01)
STD_ROM_FN(gg_nfl95p01)

struct BurnDriver BurnDrvgg_nfl95p01 = {
	"gg_nfl95p01", "gg_nfl95", NULL, NULL, "1994",
	"NFL '95 (Prototype, 19940914)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nfl95p01RomInfo, gg_nfl95p01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL Quarterback Club '96 (Euro, USA)

static struct BurnRomInfo gg_nflqb96RomDesc[] = {
	{ "nfl quarterback club '96 (usa, europe).bin",	0x80000, 0xc348e53a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nflqb96)
STD_ROM_FN(gg_nflqb96)

struct BurnDriver BurnDrvgg_nflqb96 = {
	"gg_nflqb96", NULL, NULL, NULL, "1995",
	"NFL Quarterback Club '96 (Euro, USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nflqb96RomInfo, gg_nflqb96RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NFL Quarterback Club (World)

static struct BurnRomInfo gg_nflqbRomDesc[] = {
	{ "nfl quarterback club (world).bin",	0x80000, 0x61785ed5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nflqb)
STD_ROM_FN(gg_nflqb)

struct BurnDriver BurnDrvgg_nflqb = {
	"gg_nflqb", NULL, NULL, NULL, "1994",
	"NFL Quarterback Club (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nflqbRomInfo, gg_nflqbRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (USA)

static struct BurnRomInfo gg_nhlasRomDesc[] = {
	{ "nhl all-star hockey (usa).bin",	0x80000, 0x4680c7aa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlas)
STD_ROM_FN(gg_nhlas)

struct BurnDriver BurnDrvgg_nhlas = {
	"gg_nhlas", NULL, NULL, NULL, "1995",
	"NHL All-Star Hockey (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasRomInfo, gg_nhlasRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19941215)

static struct BurnRomInfo gg_nhlasp17RomDesc[] = {
	{ "nhl all-star hockey (prototype - dec 15,  1994).bin",	0x80000, 0xe583d999, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp17)
STD_ROM_FN(gg_nhlasp17)

struct BurnDriver BurnDrvgg_nhlasp17 = {
	"gg_nhlasp17", "gg_nhlas", NULL, NULL, "1994",
	"NHL All-Star Hockey (Prototype, 19941215)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp17RomInfo, gg_nhlasp17RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19941222)

static struct BurnRomInfo gg_nhlasp16RomDesc[] = {
	{ "nhl all-star hockey (prototype - dec 22,  1994).bin",	0x80000, 0xdfba792a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp16)
STD_ROM_FN(gg_nhlasp16)

struct BurnDriver BurnDrvgg_nhlasp16 = {
	"gg_nhlasp16", "gg_nhlas", NULL, NULL, "1994",
	"NHL All-Star Hockey (Prototype, 19941222)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp16RomInfo, gg_nhlasp16RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19941229)

static struct BurnRomInfo gg_nhlasp15RomDesc[] = {
	{ "nhl all-star hockey (prototype - dec 29,  1994).bin",	0x80000, 0xe14d071b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp15)
STD_ROM_FN(gg_nhlasp15)

struct BurnDriver BurnDrvgg_nhlasp15 = {
	"gg_nhlasp15", "gg_nhlas", NULL, NULL, "1994",
	"NHL All-Star Hockey (Prototype, 19941229)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp15RomInfo, gg_nhlasp15RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19941231)

static struct BurnRomInfo gg_nhlasp14RomDesc[] = {
	{ "nhl all-star hockey (prototype - dec 31,  1994).bin",	0x80000, 0x8a2b6323, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp14)
STD_ROM_FN(gg_nhlasp14)

struct BurnDriver BurnDrvgg_nhlasp14 = {
	"gg_nhlasp14", "gg_nhlas", NULL, NULL, "1994",
	"NHL All-Star Hockey (Prototype, 19941231)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp14RomInfo, gg_nhlasp14RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950102)

static struct BurnRomInfo gg_nhlasp13RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 02,  1995).bin",	0x80000, 0x2551f22c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp13)
STD_ROM_FN(gg_nhlasp13)

struct BurnDriver BurnDrvgg_nhlasp13 = {
	"gg_nhlasp13", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950102)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp13RomInfo, gg_nhlasp13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950103)

static struct BurnRomInfo gg_nhlasp12RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 03,  1995).bin",	0x80000, 0xece5633b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp12)
STD_ROM_FN(gg_nhlasp12)

struct BurnDriver BurnDrvgg_nhlasp12 = {
	"gg_nhlasp12", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950103)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp12RomInfo, gg_nhlasp12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950107)

static struct BurnRomInfo gg_nhlasp11RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 07,  1995).bin",	0x80000, 0x82504476, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp11)
STD_ROM_FN(gg_nhlasp11)

struct BurnDriver BurnDrvgg_nhlasp11 = {
	"gg_nhlasp11", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950107)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp11RomInfo, gg_nhlasp11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950111)

static struct BurnRomInfo gg_nhlasp10RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 11,  1995).bin",	0x80000, 0x49f8b9e4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp10)
STD_ROM_FN(gg_nhlasp10)

struct BurnDriver BurnDrvgg_nhlasp10 = {
	"gg_nhlasp10", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950111)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp10RomInfo, gg_nhlasp10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950113)

static struct BurnRomInfo gg_nhlasp09RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 13,  1995).bin",	0x80000, 0x87d8fe13, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp09)
STD_ROM_FN(gg_nhlasp09)

struct BurnDriver BurnDrvgg_nhlasp09 = {
	"gg_nhlasp09", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950113)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp09RomInfo, gg_nhlasp09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950116)

static struct BurnRomInfo gg_nhlasp08RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 16,  1995).bin",	0x80000, 0x50dcf5b6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp08)
STD_ROM_FN(gg_nhlasp08)

struct BurnDriver BurnDrvgg_nhlasp08 = {
	"gg_nhlasp08", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950116)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp08RomInfo, gg_nhlasp08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950120)

static struct BurnRomInfo gg_nhlasp07RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 20 ,1995).bin",	0x80000, 0x499b06c2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp07)
STD_ROM_FN(gg_nhlasp07)

struct BurnDriver BurnDrvgg_nhlasp07 = {
	"gg_nhlasp07", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950120)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp07RomInfo, gg_nhlasp07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950121)

static struct BurnRomInfo gg_nhlasp06RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 21,  1995).bin",	0x80000, 0x16af7995, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp06)
STD_ROM_FN(gg_nhlasp06)

struct BurnDriver BurnDrvgg_nhlasp06 = {
	"gg_nhlasp06", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950121)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp06RomInfo, gg_nhlasp06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950124-A)

static struct BurnRomInfo gg_nhlasp05RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 24,  1995 - a).bin",	0x80000, 0x0b1c38f4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp05)
STD_ROM_FN(gg_nhlasp05)

struct BurnDriver BurnDrvgg_nhlasp05 = {
	"gg_nhlasp05", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950124-A)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp05RomInfo, gg_nhlasp05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950124-B)

static struct BurnRomInfo gg_nhlasp04RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 24,  1995 - b).bin",	0x80000, 0x0a4a6830, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp04)
STD_ROM_FN(gg_nhlasp04)

struct BurnDriver BurnDrvgg_nhlasp04 = {
	"gg_nhlasp04", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950124-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp04RomInfo, gg_nhlasp04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950124-C)

static struct BurnRomInfo gg_nhlasp03RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 24,  1995 - c).bin",	0x80000, 0x54f5efb2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp03)
STD_ROM_FN(gg_nhlasp03)

struct BurnDriver BurnDrvgg_nhlasp03 = {
	"gg_nhlasp03", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950124-C)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp03RomInfo, gg_nhlasp03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950124)

static struct BurnRomInfo gg_nhlasp02RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 24,  1995).bin",	0x80000, 0x74be3c4c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp02)
STD_ROM_FN(gg_nhlasp02)

struct BurnDriver BurnDrvgg_nhlasp02 = {
	"gg_nhlasp02", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950124)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp02RomInfo, gg_nhlasp02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL All-Star Hockey (Prototype, 19950125)

static struct BurnRomInfo gg_nhlasp01RomDesc[] = {
	{ "nhl all-star hockey (prototype - jan 25,  1995).bin",	0x80000, 0xa587b13e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhlasp01)
STD_ROM_FN(gg_nhlasp01)

struct BurnDriver BurnDrvgg_nhlasp01 = {
	"gg_nhlasp01", "gg_nhlas", NULL, NULL, "1995",
	"NHL All-Star Hockey (Prototype, 19950125)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlasp01RomInfo, gg_nhlasp01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// NHL Hockey (Euro, USA)

static struct BurnRomInfo gg_nhlRomDesc[] = {
	{ "mpr-17850-s.ic1",	0x80000, 0x658713a5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nhl)
STD_ROM_FN(gg_nhl)

struct BurnDriver BurnDrvgg_nhl = {
	"gg_nhl", NULL, NULL, NULL, "1995",
	"NHL Hockey (Euro, USA)\0", NULL, "Electronic Arts", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nhlRomInfo, gg_nhlRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ninja Gaiden (Jpn)

static struct BurnRomInfo gg_ngaidenjRomDesc[] = {
	{ "ninja gaiden (japan).bin",	0x20000, 0x20ef017a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ngaidenj)
STD_ROM_FN(gg_ngaidenj)

struct BurnDriver BurnDrvgg_ngaidenj = {
	"gg_ngaidenj", "gg_ngaiden", NULL, NULL, "1991",
	"Ninja Gaiden (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ngaidenjRomInfo, gg_ngaidenjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ninja Gaiden (Euro, USA)

static struct BurnRomInfo gg_ngaidenRomDesc[] = {
	{ "ninja gaiden (usa, europe).bin",	0x20000, 0xc578756b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ngaiden)
STD_ROM_FN(gg_ngaiden)

struct BurnDriver BurnDrvgg_ngaiden = {
	"gg_ngaiden", NULL, NULL, NULL, "1991",
	"Ninja Gaiden (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ngaidenRomInfo, gg_ngaidenRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ninku (Jpn)

static struct BurnRomInfo gg_ninkuRomDesc[] = {
	{ "mpr-18000.ic1",	0x80000, 0xc3056e15, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ninku)
STD_ROM_FN(gg_ninku)

struct BurnDriver BurnDrvgg_ninku = {
	"gg_ninku", NULL, NULL, NULL, "1995",
	"Ninku (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ninkuRomInfo, gg_ninkuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ninku 2 - Tenkuuryuu e no Michi (Jpn)

static struct BurnRomInfo gg_ninku2RomDesc[] = {
	{ "mpr-18695.ic1",	0x80000, 0x06247dd2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ninku2)
STD_ROM_FN(gg_ninku2)

struct BurnDriver BurnDrvgg_ninku2 = {
	"gg_ninku2", NULL, NULL, NULL, "1995",
	"Ninku 2 - Tenkuuryuu e no Michi (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ninku2RomInfo, gg_ninku2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ninku Gaiden - Hiroyuki Daikatsugeki (Jpn)

static struct BurnRomInfo gg_ninkugRomDesc[] = {
	{ "mpr-18313.ic1",	0x80000, 0x9140f239, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ninkug)
STD_ROM_FN(gg_ninkug)

struct BurnDriver BurnDrvgg_ninkug = {
	"gg_ninkug", NULL, NULL, NULL, "1995",
	"Ninku Gaiden - Hiroyuki Daikatsugeki (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ninkugRomInfo, gg_ninkugRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Nomo's World Series Baseball (Jpn)

static struct BurnRomInfo gg_nomowsbRomDesc[] = {
	{ "mpr-18562.ic1",	0x80000, 0x4ed45bda, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_nomowsb)
STD_ROM_FN(gg_nomowsb)

struct BurnDriver BurnDrvgg_nomowsb = {
	"gg_nomowsb", NULL, NULL, NULL, "1995",
	"Nomo's World Series Baseball (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_nomowsbRomInfo, gg_nomowsbRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Olympic Gold (Euro, v1, SMS Mode)

static struct BurnRomInfo gg_olympgldRomDesc[] = {
	{ "mpr-14752-f.ic1",	0x40000, 0x1d93246e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_olympgld)
STD_ROM_FN(gg_olympgld)

struct BurnDriver BurnDrvgg_olympgld = {
	"gg_olympgld", NULL, NULL, NULL, "1992",
	"Olympic Gold (Euro, v1, SMS Mode)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_olympgldRomInfo, gg_olympgldRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Olympic Gold (Jpn, USA, v0, SMS Mode)

static struct BurnRomInfo gg_olympgldaRomDesc[] = {
	{ "olympic gold (japan, usa) (en,fr,de,es,it,nl,pt,sv).bin",	0x40000, 0xa2f9c7af, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_olympglda)
STD_ROM_FN(gg_olympglda)

struct BurnDriver BurnDrvgg_olympglda = {
	"gg_olympglda", "gg_olympgld", NULL, NULL, "1992",
	"Olympic Gold (Jpn, USA, v0, SMS Mode)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_olympgldaRomInfo, gg_olympgldaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Ottifants (Euro)

static struct BurnRomInfo gg_ottifantRomDesc[] = {
	{ "ottifants, the (europe) (en,fr,de,es,it).bin",	0x40000, 0x1e673168, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ottifant)
STD_ROM_FN(gg_ottifant)

struct BurnDriver BurnDrvgg_ottifant = {
	"gg_ottifant", NULL, NULL, NULL, "1992",
	"The Ottifants (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ottifantRomInfo, gg_ottifantRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Out Run (Jpn)

static struct BurnRomInfo gg_outrunjRomDesc[] = {
	{ "out run (japan).bin",	0x20000, 0xd58cb27c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_outrunj)
STD_ROM_FN(gg_outrunj)

struct BurnDriver BurnDrvgg_outrunj = {
	"gg_outrunj", "gg_outrun", NULL, NULL, "1991",
	"Out Run (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_outrunjRomInfo, gg_outrunjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Out Run (Euro)

static struct BurnRomInfo gg_outrunRomDesc[] = {
	{ "out run.bin",	0x20000, 0x9014b322, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_outrun)
STD_ROM_FN(gg_outrun)

struct BurnDriver BurnDrvgg_outrun = {
	"gg_outrun", NULL, NULL, NULL, "1991",
	"Out Run (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_outrunRomInfo, gg_outrunRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Out Run Europa (Euro, Bra, SMS Mode)

static struct BurnRomInfo gg_outrneurRomDesc[] = {
	{ "out run europa (euro).bin",	0x40000, 0x01eab89d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_outrneur)
STD_ROM_FN(gg_outrneur)

struct BurnDriver BurnDrvgg_outrneur = {
	"gg_outrneur", NULL, NULL, NULL, "1992",
	"Out Run Europa (Euro, Bra, SMS Mode)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_outrneurRomInfo, gg_outrneurRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Out Run Europa (USA, SMS Mode)

static struct BurnRomInfo gg_outrneuruRomDesc[] = {
	{ "out run europa (usa).bin",	0x40000, 0xf037ec00, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_outrneuru)
STD_ROM_FN(gg_outrneuru)

struct BurnDriver BurnDrvgg_outrneuru = {
	"gg_outrneuru", "gg_outrneur", NULL, NULL, "1992",
	"Out Run Europa (USA, SMS Mode)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_outrneuruRomInfo, gg_outrneuruRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pac-Attack (Euro, USA)

static struct BurnRomInfo gg_pacattakRomDesc[] = {
	{ "pac-attack (usa, europe).bin",	0x20000, 0x9273ee2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pacattak)
STD_ROM_FN(gg_pacattak)

struct BurnDriver BurnDrvgg_pacattak = {
	"gg_pacattak", NULL, NULL, NULL, "1994",
	"Pac-Attack (Euro, USA)\0", NULL, "Namco", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pacattakRomInfo, gg_pacattakRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pac-In-Time (Prototype)

static struct BurnRomInfo gg_pacintimRomDesc[] = {
	{ "pac-in-time (unknown) (proto).bin",	0x40000, 0x64c28e20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pacintim)
STD_ROM_FN(gg_pacintim)

struct BurnDriver BurnDrvgg_pacintim = {
	"gg_pacintim", NULL, NULL, NULL, "199?",
	"Pac-In-Time (Prototype)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pacintimRomInfo, gg_pacintimRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pac-Man (Jpn)

static struct BurnRomInfo gg_pacmanjRomDesc[] = {
	{ "mpr-13660.ic1",	0x20000, 0xa16c5e58, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pacmanj)
STD_ROM_FN(gg_pacmanj)

struct BurnDriver BurnDrvgg_pacmanj = {
	"gg_pacmanj", "gg_pacman", NULL, NULL, "1991",
	"Pac-Man (Jpn)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pacmanjRomInfo, gg_pacmanjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pac-Man (USA)

static struct BurnRomInfo gg_pacmanRomDesc[] = {
	{ "pac-man (usa).bin",	0x20000, 0xb318dd37, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pacman)
STD_ROM_FN(gg_pacman)

struct BurnDriver BurnDrvgg_pacman = {
	"gg_pacman", NULL, NULL, NULL, "1991",
	"Pac-Man (USA)\0", NULL, "Namco", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pacmanRomInfo, gg_pacmanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Panzer Dragoon Mini (Jpn)

static struct BurnRomInfo gg_panzerRomDesc[] = {
	{ "panzer dragoon mini (japan).bin",	0x80000, 0xe9783cea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_panzer)
STD_ROM_FN(gg_panzer)

struct BurnDriver BurnDrvgg_panzer = {
	"gg_panzer", NULL, NULL, NULL, "1996",
	"Panzer Dragoon Mini (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_panzerRomInfo, gg_panzerRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Paperboy (Euro, USA)

static struct BurnRomInfo gg_paperboyRomDesc[] = {
	{ "paperboy (usa, europe).bin",	0x20000, 0xf54b6803, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_paperboy)
STD_ROM_FN(gg_paperboy)

struct BurnDriver BurnDrvgg_paperboy = {
	"gg_paperboy", NULL, NULL, NULL, "1991",
	"Paperboy (Euro, USA)\0", NULL, "Tengen", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_paperboyRomInfo, gg_paperboyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Paperboy II (Euro, USA)

static struct BurnRomInfo gg_paperbo2RomDesc[] = {
	{ "paperboy ii (usa, europe).bin",	0x40000, 0x8b2c454b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_paperbo2)
STD_ROM_FN(gg_paperbo2)

struct BurnDriver BurnDrvgg_paperbo2 = {
	"gg_paperbo2", NULL, NULL, NULL, "1992",
	"Paperboy II (Euro, USA)\0", NULL, "Tengen", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_paperbo2RomInfo, gg_paperbo2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pengo (Jpn)

static struct BurnRomInfo gg_pengojRomDesc[] = {
	{ "pengo (japan).bin",	0x08000, 0xce863dba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pengoj)
STD_ROM_FN(gg_pengoj)

struct BurnDriver BurnDrvgg_pengoj = {
	"gg_pengoj", "gg_pengo", NULL, NULL, "1990",
	"Pengo (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pengojRomInfo, gg_pengojRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pengo (Euro, USA)

static struct BurnRomInfo gg_pengoRomDesc[] = {
	{ "mpr-13804-s.ic1",	0x08000, 0x0da23cc1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pengo)
STD_ROM_FN(gg_pengo)

struct BurnDriver BurnDrvgg_pengo = {
	"gg_pengo", NULL, NULL, NULL, "1991",
	"Pengo (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pengoRomInfo, gg_pengoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pet Club Inu Daisuki! (Jpn)

static struct BurnRomInfo gg_inudaisRomDesc[] = {
	{ "pet club inu daisuki! (japan).bin",	0x80000, 0xb42d8430, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_inudais)
STD_ROM_FN(gg_inudais)

struct BurnDriver BurnDrvgg_inudais = {
	"gg_inudais", NULL, NULL, NULL, "1996",
	"Pet Club Inu Daisuki! (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_inudaisRomInfo, gg_inudaisRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pete Sampras Tennis (Euro)

static struct BurnRomInfo gg_samprasRomDesc[] = {
	{ "pete sampras tennis (europe).bin",	0x40000, 0xc1756bee, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sampras)
STD_ROM_FN(gg_sampras)

struct BurnDriver BurnDrvgg_sampras = {
	"gg_sampras", NULL, NULL, NULL, "1994",
	"Pete Sampras Tennis (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_samprasRomInfo, gg_samprasRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// PGA Tour 96 (Euro, USA)

static struct BurnRomInfo gg_pga96RomDesc[] = {
	{ "pga tour 96 (usa, europe).bin",	0x80000, 0x542b6d8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pga96)
STD_ROM_FN(gg_pga96)

struct BurnDriver BurnDrvgg_pga96 = {
	"gg_pga96", NULL, NULL, NULL, "1996",
	"PGA Tour 96 (Euro, USA)\0", NULL, "Black Pearl", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pga96RomInfo, gg_pga96RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// PGA Tour Golf (USA, v1.1)

static struct BurnRomInfo gg_pgatourRomDesc[] = {
	{ "pga tour golf (usa) (v1.1).bin",	0x40000, 0x1c77d996, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pgatour)
STD_ROM_FN(gg_pgatour)

struct BurnDriver BurnDrvgg_pgatour = {
	"gg_pgatour", NULL, NULL, NULL, "1994",
	"PGA Tour Golf (USA, v1.1)\0", NULL, "Electronic Arts?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pgatourRomInfo, gg_pgatourRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// PGA Tour Golf (Euro, USA)

static struct BurnRomInfo gg_pgatourtRomDesc[] = {
	{ "mpr-16044.ic1",	0x40000, 0x9700bb65, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pgatourt)
STD_ROM_FN(gg_pgatourt)

struct BurnDriver BurnDrvgg_pgatourt = {
	"gg_pgatourt", NULL, NULL, NULL, "1991",
	"PGA Tour Golf (Euro, USA)\0", NULL, "Tengen", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pgatourtRomInfo, gg_pgatourtRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// PGA Tour Golf II (Euro, USA)

static struct BurnRomInfo gg_pgatour2RomDesc[] = {
	{ "mpr-17319-s.ic1",	0x80000, 0x4a8ac851, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pgatour2)
STD_ROM_FN(gg_pgatour2)

struct BurnDriver BurnDrvgg_pgatour2 = {
	"gg_pgatour2", NULL, NULL, NULL, "1992",
	"PGA Tour Golf II (Euro, USA)\0", NULL, "Time Warner Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pgatour2RomInfo, gg_pgatour2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star Adventure (Jpn)

static struct BurnRomInfo gg_pstaradvRomDesc[] = {
	{ "phantasy star adventure (japan).bin",	0x20000, 0x1a51579d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pstaradv)
STD_ROM_FN(gg_pstaradv)

struct BurnDriver BurnDrvgg_pstaradv = {
	"gg_pstaradv", NULL, NULL, NULL, "1992",
	"Phantasy Star Adventure (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pstaradvRomInfo, gg_pstaradvRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantasy Star Gaiden (Jpn)

static struct BurnRomInfo gg_pstargdnRomDesc[] = {
	{ "phantasy star gaiden (japan).bin",	0x40000, 0xa942514a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pstargdn)
STD_ROM_FN(gg_pstargdn)

struct BurnDriver BurnDrvgg_pstargdn = {
	"gg_pstargdn", NULL, NULL, NULL, "1992",
	"Phantasy Star Gaiden (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pstargdnRomInfo, gg_pstargdnRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Phantom 2040 (Euro, USA)

static struct BurnRomInfo gg_phantomRomDesc[] = {
	{ "phantom 2040 (usa, europe).bin",	0x80000, 0x281b1c3a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_phantom)
STD_ROM_FN(gg_phantom)

struct BurnDriver BurnDrvgg_phantom = {
	"gg_phantom", NULL, NULL, NULL, "1995",
	"Phantom 2040 (Euro, USA)\0", NULL, "Viacom New Media", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_phantomRomInfo, gg_phantomRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pinball Dreams (USA)

static struct BurnRomInfo gg_pbdreamsRomDesc[] = {
	{ "pinball dreams (usa).bin",	0x20000, 0x635c483a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pbdreams)
STD_ROM_FN(gg_pbdreams)

struct BurnDriver BurnDrvgg_pbdreams = {
	"gg_pbdreams", NULL, NULL, NULL, "1993",
	"Pinball Dreams (USA)\0", NULL, "GameTek", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pbdreamsRomInfo, gg_pbdreamsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pocket Jansou (Jpn)

static struct BurnRomInfo gg_pocketjRomDesc[] = {
	{ "pocket jansou (japan).bin",	0x20000, 0xcc90c723, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pocketj)
STD_ROM_FN(gg_pocketj)

struct BurnDriver BurnDrvgg_pocketj = {
	"gg_pocketj", NULL, NULL, NULL, "1992",
	"Pocket Jansou (Jpn)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pocketjRomInfo, gg_pocketjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Poker Face Paul's Blackjack (USA)

static struct BurnRomInfo gg_pokerfbjRomDesc[] = {
	{ "poker faced paul's blackjack (usa).bin",	0x20000, 0x89d34067, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pokerfbj)
STD_ROM_FN(gg_pokerfbj)

struct BurnDriver BurnDrvgg_pokerfbj = {
	"gg_pokerfbj", NULL, NULL, NULL, "1994",
	"Poker Face Paul's Blackjack (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pokerfbjRomInfo, gg_pokerfbjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Poker Faced Paul's Gin (USA)

static struct BurnRomInfo gg_pokerfgRomDesc[] = {
	{ "poker faced paul's gin (usa).bin",	0x20000, 0xafd61b89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pokerfg)
STD_ROM_FN(gg_pokerfg)

struct BurnDriver BurnDrvgg_pokerfg = {
	"gg_pokerfg", NULL, NULL, NULL, "1994",
	"Poker Faced Paul's Gin (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pokerfgRomInfo, gg_pokerfgRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Poker Face Paul's Poker (USA)

static struct BurnRomInfo gg_pokerfpRomDesc[] = {
	{ "poker faced paul's poker (usa).bin",	0x20000, 0xe783daf8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pokerfp)
STD_ROM_FN(gg_pokerfp)

struct BurnDriver BurnDrvgg_pokerfp = {
	"gg_pokerfp", NULL, NULL, NULL, "1994",
	"Poker Face Paul's Poker (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pokerfpRomInfo, gg_pokerfpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Poker Face Paul's Solitaire (USA)

static struct BurnRomInfo gg_pokerfsRomDesc[] = {
	{ "poker faced paul's solitaire (usa).bin",	0x20000, 0x0e9b0c0a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pokerfs)
STD_ROM_FN(gg_pokerfs)

struct BurnDriver BurnDrvgg_pokerfs = {
	"gg_pokerfs", NULL, NULL, NULL, "1994",
	"Poker Face Paul's Solitaire (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pokerfsRomInfo, gg_pokerfsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pop Breaker (Jpn)

static struct BurnRomInfo gg_popbreakRomDesc[] = {
	{ "pop breaker (japan).bin",	0x20000, 0x71deba5a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_popbreak)
STD_ROM_FN(gg_popbreak)

struct BurnDriver BurnDrvgg_popbreak = {
	"gg_popbreak", NULL, NULL, NULL, "1991",
	"Pop Breaker (Jpn)\0", NULL, "Micro Cabin", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_JAPANESE, GBF_MISC, 0,
	GGGetZipName, gg_popbreakRomInfo, gg_popbreakRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Popeye no Beach Volleyball (Jpn)

static struct BurnRomInfo gg_popeyeRomDesc[] = {
	{ "popeye no beach volleyball (japan).bin",	0x80000, 0x3ef66810, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_popeye)
STD_ROM_FN(gg_popeye)

struct BurnDriver BurnDrvgg_popeye = {
	"gg_popeye", NULL, NULL, NULL, "1994",
	"Popeye no Beach Volleyball (Jpn)\0", NULL, "Technos Japan", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_popeyeRomInfo, gg_popeyeRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Power Drive (Euro)

static struct BurnRomInfo gg_pdriveRomDesc[] = {
	{ "power drive (europe).bin",	0x80000, 0xac369ae3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pdrive)
STD_ROM_FN(gg_pdrive)

struct BurnDriver BurnDrvgg_pdrive = {
	"gg_pdrive", NULL, NULL, NULL, "1994",
	"Power Drive (Euro)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pdriveRomInfo, gg_pdriveRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Power Strike II (Euro) ~ GG Aleste II (Jpn)

static struct BurnRomInfo gg_pstrike2RomDesc[] = {
	{ "power strike ii (japan, europe).bin",	0x40000, 0x09de1528, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pstrike2)
STD_ROM_FN(gg_pstrike2)

struct BurnDriver BurnDrvgg_pstrike2 = {
	"gg_pstrike2", NULL, NULL, NULL, "1993",
	"Power Strike II (Euro) ~ GG Aleste II (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pstrike2RomInfo, gg_pstrike2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Predator 2 (Euro, USA, SMS Mode)

static struct BurnRomInfo gg_predatr2RomDesc[] = {
	{ "predator 2 (usa, europe).bin",	0x40000, 0xe5f789b9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_predatr2)
STD_ROM_FN(gg_predatr2)

struct BurnDriver BurnDrvgg_predatr2 = {
	"gg_predatr2", NULL, NULL, NULL, "1992",
	"Predator 2 (Euro, USA, SMS Mode)\0", NULL, "Arena", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_predatr2RomInfo, gg_predatr2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Primal Rage (Euro, USA)

static struct BurnRomInfo gg_primalRomDesc[] = {
	{ "mpr-18181.ic1",	0x80000, 0x2a34b5c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_primal)
STD_ROM_FN(gg_primal)

struct BurnDriver BurnDrvgg_primal = {
	"gg_primal", NULL, NULL, NULL, "1994",
	"Primal Rage (Euro, USA)\0", NULL, "Time Warner Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_primalRomInfo, gg_primalRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Prince of Persia (Euro, SMS Mode)

static struct BurnRomInfo gg_ppersiaRomDesc[] = {
	{ "prince of persia (usa, europe) (beta).bin",	0x40000, 0x45f058d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ppersia)
STD_ROM_FN(gg_ppersia)

struct BurnDriver BurnDrvgg_ppersia = {
	"gg_ppersia", NULL, NULL, NULL, "1992",
	"Prince of Persia (Euro, SMS Mode)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_ppersiaRomInfo, gg_ppersiaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Prince of Persia (USA, SMS Mode)

static struct BurnRomInfo gg_ppersiauRomDesc[] = {
	{ "prince of persia (usa, europe).bin",	0x40000, 0x311d2863, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ppersiau)
STD_ROM_FN(gg_ppersiau)

struct BurnDriver BurnDrvgg_ppersiau = {
	"gg_ppersiau", "gg_ppersia", NULL, NULL, "1992",
	"Prince of Persia (USA, SMS Mode)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_ppersiauRomInfo, gg_ppersiauRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Pro Yakyuu '91 (Jpn)

static struct BurnRomInfo gg_proyak91RomDesc[] = {
	{ "pro yakyuu '91, the (japan).bin",	0x20000, 0x6d3a10d3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_proyak91)
STD_ROM_FN(gg_proyak91)

struct BurnDriver BurnDrvgg_proyak91 = {
	"gg_proyak91", NULL, NULL, NULL, "1991",
	"The Pro Yakyuu '91 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_proyak91RomInfo, gg_proyak91RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pro Yakyuu GG League '94 (Jpn)

static struct BurnRomInfo gg_proyak94RomDesc[] = {
	{ "pro yakyuu gg league '94 (japan).bin",	0x80000, 0xa1a19135, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_proyak94)
STD_ROM_FN(gg_proyak94)

struct BurnDriver BurnDrvgg_proyak94 = {
	"gg_proyak94", NULL, NULL, NULL, "1994",
	"Pro Yakyuu GG League '94 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_proyak94RomInfo, gg_proyak94RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Pro Yakyuu GG League (Jpn)

static struct BurnRomInfo gg_proyakggRomDesc[] = {
	{ "mpr-15564.ic1",	0x40000, 0x2da8e943, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_proyakgg)
STD_ROM_FN(gg_proyakgg)

struct BurnDriver BurnDrvgg_proyakgg = {
	"gg_proyakgg", NULL, NULL, NULL, "1993",
	"Pro Yakyuu GG League (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_proyakggRomInfo, gg_proyakggRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Psychic World (Jpn)

static struct BurnRomInfo gg_psychicwjRomDesc[] = {
	{ "psychic world (japan).bin",	0x20000, 0xafcc7828, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_psychicwj)
STD_ROM_FN(gg_psychicwj)

struct BurnDriver BurnDrvgg_psychicwj = {
	"gg_psychicwj", "gg_psychicw", NULL, NULL, "1991",
	"Psychic World (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_psychicwjRomInfo, gg_psychicwjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Psychic World (Euro, USA, Bra)

static struct BurnRomInfo gg_psychicwRomDesc[] = {
	{ "psychic world (usa, europe) (v1.1).bin",	0x20000, 0x73779b22, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_psychicw)
STD_ROM_FN(gg_psychicw)

struct BurnDriver BurnDrvgg_psychicw = {
	"gg_psychicw", NULL, NULL, NULL, "1991",
	"Psychic World (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_psychicwRomInfo, gg_psychicwRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Putt and Putter (Jpn)

static struct BurnRomInfo gg_puttputtjRomDesc[] = {
	{ "putt and putter (japan).bin",	0x20000, 0x407ac070, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_puttputtj)
STD_ROM_FN(gg_puttputtj)

struct BurnDriver BurnDrvgg_puttputtj = {
	"gg_puttputtj", "gg_puttputt", NULL, NULL, "1991",
	"Putt and Putter (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_puttputtjRomInfo, gg_puttputtjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Putt and Putter (Euro, USA, Bra)

static struct BurnRomInfo gg_puttputtRomDesc[] = {
	{ "putt and putter (usa, europe).bin",	0x20000, 0xecc301dd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_puttputt)
STD_ROM_FN(gg_puttputt)

struct BurnDriver BurnDrvgg_puttputt = {
	"gg_puttputt", NULL, NULL, NULL, "1991",
	"Putt and Putter (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_puttputtRomInfo, gg_puttputtRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Puyo Puyo (Jpn)

static struct BurnRomInfo gg_puyopuyoRomDesc[] = {
	{ "mpr-15464.ic1",	0x40000, 0xd173a06f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_puyopuyo)
STD_ROM_FN(gg_puyopuyo)

struct BurnDriver BurnDrvgg_puyopuyo = {
	"gg_puyopuyo", NULL, NULL, NULL, "1993",
	"Puyo Puyo (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_puyopuyoRomInfo, gg_puyopuyoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Puyo Puyo 2 (Jpn)

static struct BurnRomInfo gg_puyopuy2RomDesc[] = {
	{ "mpr-17537.ic1",	0x80000, 0x3ab2393b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_puyopuy2)
STD_ROM_FN(gg_puyopuy2)

struct BurnDriver BurnDrvgg_puyopuy2 = {
	"gg_puyopuy2", NULL, NULL, NULL, "1994",
	"Puyo Puyo 2 (Jpn)\0", NULL, "Compile", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_puyopuy2RomInfo, gg_puyopuy2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Puzzle and Action Tant~R (Jpn)

static struct BurnRomInfo gg_tantrRomDesc[] = {
	{ "mpr-16607.ic1",	0x80000, 0x09151743, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tantr)
STD_ROM_FN(gg_tantr)

struct BurnDriver BurnDrvgg_tantr = {
	"gg_tantr", NULL, NULL, NULL, "1994",
	"Puzzle and Action Tant~R (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tantrRomInfo, gg_tantrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Puzzle Bobble (Jpn)

static struct BurnRomInfo gg_pbobbleRomDesc[] = {
	{ "puzzle bobble (japan).bin",	0x40000, 0x8e54ee04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_pbobble)
STD_ROM_FN(gg_pbobble)

struct BurnDriver BurnDrvgg_pbobble = {
	"gg_pbobble", "gg_bam", NULL, NULL, "1996",
	"Puzzle Bobble (Jpn)\0", NULL, "Taito", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_pbobbleRomInfo, gg_pbobbleRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Quest for the Shaven Yak Starring Ren Hoëk and Stimpy (Euro, USA)

static struct BurnRomInfo gg_shavnyakRomDesc[] = {
	{ "quest for the shaven yak starring ren hoek and stimpy (usa, europe).bin",	0x80000, 0x6c451ee1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shavnyak)
STD_ROM_FN(gg_shavnyak)

struct BurnDriver BurnDrvgg_shavnyak = {
	"gg_shavnyak", NULL, NULL, NULL, "1993",
	"Quest for the Shaven Yak Starring Ren Hoek and Stimpy (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shavnyakRomInfo, gg_shavnyakRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Quiz Gear Fight!! (Jpn)

static struct BurnRomInfo gg_quizgearRomDesc[] = {
	{ "mpr-17816.ic1",	0x80000, 0x736cdb76, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_quizgear)
STD_ROM_FN(gg_quizgear)

struct BurnDriver BurnDrvgg_quizgear = {
	"gg_quizgear", NULL, NULL, NULL, "1995",
	"The Quiz Gear Fight!! (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_quizgearRomInfo, gg_quizgearRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// R.B.I. Baseball '94 (USA)

static struct BurnRomInfo gg_rbibb94RomDesc[] = {
	{ "mpr-17310-s.ic1",	0x80000, 0x6dc3295e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_rbibb94)
STD_ROM_FN(gg_rbibb94)

struct BurnDriver BurnDrvgg_rbibb94 = {
	"gg_rbibb94", NULL, NULL, NULL, "1994",
	"R.B.I. Baseball '94 (USA)\0", NULL, "Time Warner Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_rbibb94RomInfo, gg_rbibb94RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// R.C. Grand Prix (USA, SMS Mode)

static struct BurnRomInfo gg_rcgpRomDesc[] = {
	{ "r.c. grand prix (usa, europe).bin",	0x40000, 0x56201996, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_rcgp)
STD_ROM_FN(gg_rcgp)

struct BurnDriver BurnDrvgg_rcgp = {
	"gg_rcgp", NULL, NULL, NULL, "1992",
	"R.C. Grand Prix (USA, SMS Mode)\0", NULL, "Absolute Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_rcgpRomInfo, gg_rcgpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rastan Saga (Jpn, SMS Mode)

static struct BurnRomInfo gg_rastanRomDesc[] = {
	{ "rastan saga (japan).bin",	0x40000, 0x9c76fb3a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_rastan)
STD_ROM_FN(gg_rastan)

struct BurnDriver BurnDrvgg_rastan = {
	"gg_rastan", NULL, NULL, NULL, "1991",
	"Rastan Saga (Jpn, SMS Mode)\0", NULL, "Taito", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_rastanRomInfo, gg_rastanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Revenge of Drancon (USA, Bra)

static struct BurnRomInfo gg_revengedRomDesc[] = {
	{ "revenge of drancon (usa).bin",	0x20000, 0x03e9c607, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_revenged)
STD_ROM_FN(gg_revenged)

struct BurnDriver BurnDrvgg_revenged = {
	"gg_revenged", "gg_wboy", NULL, NULL, "1991",
	"Revenge of Drancon (USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_revengedRomInfo, gg_revengedRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Riddick Bowe Boxing (Jpn)

static struct BurnRomInfo gg_riddickjRomDesc[] = {
	{ "riddick bowe boxing (japan).bin",	0x40000, 0xa45fffb7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_riddickj)
STD_ROM_FN(gg_riddickj)

struct BurnDriver BurnDrvgg_riddickj = {
	"gg_riddickj", "gg_riddick", NULL, NULL, "1994",
	"Riddick Bowe Boxing (Jpn)\0", NULL, "Micronet", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_riddickjRomInfo, gg_riddickjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Riddick Bowe Boxing (USA)

static struct BurnRomInfo gg_riddickRomDesc[] = {
	{ "riddick bowe boxing (usa).bin",	0x40000, 0x38d8ec56, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_riddick)
STD_ROM_FN(gg_riddick)

struct BurnDriver BurnDrvgg_riddick = {
	"gg_riddick", NULL, NULL, NULL, "1993",
	"Riddick Bowe Boxing (USA)\0", NULL, "Extreme Entertainment Group", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_riddickRomInfo, gg_riddickRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Rise of the Robots (Euro, USA)

static struct BurnRomInfo gg_riseroboRomDesc[] = {
	{ "rise of the robots (usa, europe).bin",	0x80000, 0x100b77b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_riserobo)
STD_ROM_FN(gg_riserobo)

struct BurnDriver BurnDrvgg_riserobo = {
	"gg_riserobo", NULL, NULL, NULL, "1994",
	"Rise of the Robots (Euro, USA)\0", NULL, "Time Warner Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_riseroboRomInfo, gg_riseroboRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ristar - The Shooting Star (World)

static struct BurnRomInfo gg_ristarRomDesc[] = {
	{ "ristar - the shooting star (world).bin",	0x80000, 0xefe65b3b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ristar)
STD_ROM_FN(gg_ristar)

struct BurnDriver BurnDrvgg_ristar = {
	"gg_ristar", NULL, NULL, NULL, "1995",
	"Ristar - The Shooting Star (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ristarRomInfo, gg_ristarRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ristar - The Shooting Star (Prototype, 19941101)

static struct BurnRomInfo gg_ristarp2RomDesc[] = {
	{ "ristar (prototype - nov 01,  1994).bin",	0x80000, 0x44fa6ae6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ristarp2)
STD_ROM_FN(gg_ristarp2)

struct BurnDriver BurnDrvgg_ristarp2 = {
	"gg_ristarp2", "gg_ristar", NULL, NULL, "1994",
	"Ristar - The Shooting Star (Prototype, 19941101)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ristarp2RomInfo, gg_ristarp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ristar - The Shooting Star (Prototype, 19941102)

static struct BurnRomInfo gg_ristarp1RomDesc[] = {
	{ "ristar (prototype - nov 02,  1994).bin",	0x80000, 0x302d2b4b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ristarp1)
STD_ROM_FN(gg_ristarp1)

struct BurnDriver BurnDrvgg_ristarp1 = {
	"gg_ristarp1", "gg_ristar", NULL, NULL, "1994",
	"Ristar - The Shooting Star (Prototype, 19941102)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ristarp1RomInfo, gg_ristarp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ristar - The Shooting Star (Prototype, 19941019)

static struct BurnRomInfo gg_ristarp4RomDesc[] = {
	{ "ristar (prototype - oct 19,  1994).bin",	0x80000, 0x672d38d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ristarp4)
STD_ROM_FN(gg_ristarp4)

struct BurnDriver BurnDrvgg_ristarp4 = {
	"gg_ristarp4", "gg_ristar", NULL, NULL, "1994",
	"Ristar - The Shooting Star (Prototype, 19941019)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ristarp4RomInfo, gg_ristarp4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ristar - The Shooting Star (Prototype, 19941026)

static struct BurnRomInfo gg_ristarp3RomDesc[] = {
	{ "ristar (prototype - oct 26,  1994).bin",	0x80000, 0xa302413c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ristarp3)
STD_ROM_FN(gg_ristarp3)

struct BurnDriver BurnDrvgg_ristarp3 = {
	"gg_ristarp3", "gg_ristar", NULL, NULL, "1994",
	"Ristar - The Shooting Star (Prototype, 19941026)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ristarp3RomInfo, gg_ristarp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ristar - The Shooting Star (Prototype, 19940909)

static struct BurnRomInfo gg_ristarp5RomDesc[] = {
	{ "ristar (prototype - sep 09,  1994).bin",	0x80000, 0xa1c214bd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ristarp5)
STD_ROM_FN(gg_ristarp5)

struct BurnDriver BurnDrvgg_ristarp5 = {
	"gg_ristarp5", "gg_ristar", NULL, NULL, "1994",
	"Ristar - The Shooting Star (Prototype, 19940909)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ristarp5RomInfo, gg_ristarp5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Road Rash (Euro)

static struct BurnRomInfo gg_roadrashRomDesc[] = {
	{ "road rash (eu).bin",	0x80000, 0x176505d4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_roadrash)
STD_ROM_FN(gg_roadrash)

struct BurnDriver BurnDrvgg_roadrash = {
	"gg_roadrash", NULL, NULL, NULL, "1993",
	"Road Rash (Euro)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_roadrashRomInfo, gg_roadrashRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Road Rash (USA)

static struct BurnRomInfo gg_roadrashuRomDesc[] = {
	{ "road rash (usa).bin",	0x80000, 0x96045f76, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_roadrashu)
STD_ROM_FN(gg_roadrashu)

struct BurnDriver BurnDrvgg_roadrashu = {
	"gg_roadrashu", "gg_roadrash", NULL, NULL, "1993",
	"Road Rash (USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_roadrashuRomInfo, gg_roadrashuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// RoboCop 3 (World)

static struct BurnRomInfo gg_robocop3RomDesc[] = {
	{ "robocop 3 (usa, europe).bin",	0x40000, 0x069a0704, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_robocop3)
STD_ROM_FN(gg_robocop3)

struct BurnDriver BurnDrvgg_robocop3 = {
	"gg_robocop3", NULL, NULL, NULL, "1993",
	"RoboCop 3 (World)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_robocop3RomInfo, gg_robocop3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// RoboCop versus The Terminator (Euro, USA)

static struct BurnRomInfo gg_robotermRomDesc[] = {
	{ "mpr-16049.ic1",	0x80000, 0x4ab7fa4e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_roboterm)
STD_ROM_FN(gg_roboterm)

struct BurnDriver BurnDrvgg_roboterm = {
	"gg_roboterm", NULL, NULL, NULL, "1993",
	"RoboCop versus The Terminator (Euro, USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_robotermRomInfo, gg_robotermRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Royal Stone - Hirakareshi Toki no Tobira (Jpn)

static struct BurnRomInfo gg_royalstnRomDesc[] = {
	{ "royal stone - hirakareshi toki no tobira (japan).bin",	0x80000, 0x445d7cd2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_royalstn)
STD_ROM_FN(gg_royalstn)

struct BurnDriver BurnDrvgg_royalstn = {
	"gg_royalstn", NULL, NULL, NULL, "1995",
	"Royal Stone - Hirakareshi Toki no Tobira (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_royalstnRomInfo, gg_royalstnRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ryuukyuu (Jpn)

static struct BurnRomInfo gg_ryukyuRomDesc[] = {
	{ "mpr-13884.ic1",	0x20000, 0x95efd52b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ryukyu)
STD_ROM_FN(gg_ryukyu)

struct BurnDriver BurnDrvgg_ryukyu = {
	"gg_ryukyu", "gg_solitarp", NULL, NULL, "1991",
	"Ryuukyuu (Jpn)\0", NULL, "Face", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ryukyuRomInfo, gg_ryukyuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// S.S. Lucifer - Man Overboard! (Euro)

static struct BurnRomInfo gg_sslucifrRomDesc[] = {
	{ "s.s. lucifer - man overboard! (europe).bin",	0x40000, 0xd9a7f170, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sslucifr)
STD_ROM_FN(gg_sslucifr)

struct BurnDriver BurnDrvgg_sslucifr = {
	"gg_sslucifr", NULL, NULL, NULL, "1994",
	"S.S. Lucifer - Man Overboard! (Euro)\0", NULL, "Codemasters", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES, GBF_MISC, 0,
	GGGetZipName, gg_sslucifrRomInfo, gg_sslucifrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Samurai Shodown (USA)

static struct BurnRomInfo gg_samshoRomDesc[] = {
	{ "samurai shodown (usa).bin",	0x80000, 0x98171deb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_samsho)
STD_ROM_FN(gg_samsho)

struct BurnDriver BurnDrvgg_samsho = {
	"gg_samsho", NULL, NULL, NULL, "1994",
	"Samurai Shodown (USA)\0", NULL, "Takara", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_samshoRomInfo, gg_samshoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Samurai Spirits (Jpn)

static struct BurnRomInfo gg_samspirRomDesc[] = {
	{ "samurai spirits (japan).bin",	0x80000, 0x93fd73dc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_samspir)
STD_ROM_FN(gg_samspir)

struct BurnDriver BurnDrvgg_samspir = {
	"gg_samspir", "gg_samsho", NULL, NULL, "1994",
	"Samurai Spirits (Jpn)\0", NULL, "Takara", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_samspirRomInfo, gg_samspirRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Scratch Golf (USA)

static struct BurnRomInfo gg_scratchgRomDesc[] = {
	{ "scratch golf (us).bin",	0x40000, 0xc10df4ce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_scratchg)
STD_ROM_FN(gg_scratchg)

struct BurnDriver BurnDrvgg_scratchg = {
	"gg_scratchg", NULL, NULL, NULL, "1994",
	"Scratch Golf (USA)\0", NULL, "Vic Tokai", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_scratchgRomInfo, gg_scratchgRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Scratch Golf (Jpn)

static struct BurnRomInfo gg_scratchgjRomDesc[] = {
	{ "mpr-16454.ic1",	0x40000, 0xec0f2c72, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_scratchgj)
STD_ROM_FN(gg_scratchgj)

struct BurnDriver BurnDrvgg_scratchgj = {
	"gg_scratchgj", "gg_scratchg", NULL, NULL, "1994",
	"Scratch Golf (Jpn)\0", NULL, "Vic Tokai", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_scratchgjRomInfo, gg_scratchgjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// SD Gundam - Winner's History (Jpn)

static struct BurnRomInfo gg_sdgundamRomDesc[] = {
	{ "mpr-17848.ic1",	0x80000, 0x5e2b39b8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sdgundam)
STD_ROM_FN(gg_sdgundam)

struct BurnDriver BurnDrvgg_sdgundam = {
	"gg_sdgundam", NULL, NULL, NULL, "1995",
	"SD Gundam - Winner's History (Jpn)\0", NULL, "Bandai", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sdgundamRomInfo, gg_sdgundamRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sega Game Pack 4 in 1 (Euro)

static struct BurnRomInfo gg_sega4in1RomDesc[] = {
	{ "mpr-15205.ic1",	0x40000, 0x0924d2ec, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sega4in1)
STD_ROM_FN(gg_sega4in1)

struct BurnDriver BurnDrvgg_sega4in1 = {
	"gg_sega4in1", NULL, NULL, NULL, "1991",
	"Sega Game Pack 4 in 1 (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sega4in1RomInfo, gg_sega4in1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sega Game Pack 4 in 1 (Euro, Prototype)

static struct BurnRomInfo gg_sega4in1pRomDesc[] = {
	{ "sega game pack 4 in 1 (proto).bin",	0x40000, 0x758a7123, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sega4in1p)
STD_ROM_FN(gg_sega4in1p)

struct BurnDriver BurnDrvgg_sega4in1p = {
	"gg_sega4in1p", "gg_sega4in1", NULL, NULL, "1991",
	"Sega Game Pack 4 in 1 (Euro, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sega4in1pRomInfo, gg_sega4in1pRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sensible Soccer (Euro)

static struct BurnRomInfo gg_sensibleRomDesc[] = {
	{ "sensible soccer (europe).bin",	0x20000, 0x5b68da75, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sensible)
STD_ROM_FN(gg_sensible)

struct BurnDriver BurnDrvgg_sensible = {
	"gg_sensible", NULL, NULL, NULL, "1994",
	"Sensible Soccer (Euro)\0", NULL, "Sony Imagesoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sensibleRomInfo, gg_sensibleRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shadam Crusader - Harukanaru Oukoku (Jpn)

static struct BurnRomInfo gg_shadamRomDesc[] = {
	{ "shadam crusader - harukanaru oukoku (japan).bin",	0x80000, 0x09f9ed60, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shadam)
STD_ROM_FN(gg_shadam)

struct BurnDriver BurnDrvgg_shadam = {
	"gg_shadam", "gg_defoasis", NULL, NULL, "1992",
	"Shadam Crusader - Harukanaru Oukoku (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shadamRomInfo, gg_shadamRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shanghai II (Jpn, v0)

static struct BurnRomInfo gg_shangh2aRomDesc[] = {
	{ "shanghai ii (japan).bin",	0x20000, 0x2ae8c75f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shangh2a)
STD_ROM_FN(gg_shangh2a)

struct BurnDriver BurnDrvgg_shangh2a = {
	"gg_shangh2a", "gg_shangh2", NULL, NULL, "1990",
	"Shanghai II (Jpn, v0)\0", NULL, "Sunsoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shangh2aRomInfo, gg_shangh2aRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shanghai II (Jpn, v1)

static struct BurnRomInfo gg_shangh2RomDesc[] = {
	{ "mpr-13494.ic1",	0x20000, 0x81314249, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shangh2)
STD_ROM_FN(gg_shangh2)

struct BurnDriver BurnDrvgg_shangh2 = {
	"gg_shangh2", NULL, NULL, NULL, "1990",
	"Shanghai II (Jpn, v1)\0", NULL, "Sunsoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shangh2RomInfo, gg_shangh2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shaq Fu (Euro, USA)

static struct BurnRomInfo gg_shaqfuRomDesc[] = {
	{ "shaq fu (usa, europe).bin",	0x80000, 0x6fcb8ab0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shaqfu)
STD_ROM_FN(gg_shaqfu)

struct BurnDriver BurnDrvgg_shaqfu = {
	"gg_shaqfu", NULL, NULL, NULL, "1995",
	"Shaq Fu (Euro, USA)\0", NULL, "Electronic Arts", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shaqfuRomInfo, gg_shaqfuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shikinjou (Jpn)

static struct BurnRomInfo gg_shikinjoRomDesc[] = {
	{ "shikinjou (japan).bin",	0x20000, 0x9c5c7f53, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shikinjo)
STD_ROM_FN(gg_shikinjo)

struct BurnDriver BurnDrvgg_shikinjo = {
	"gg_shikinjo", NULL, NULL, NULL, "1991",
	"Shikinjou (Jpn)\0", NULL, "Sunsoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shikinjoRomInfo, gg_shikinjoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force Gaiden - Ensei, Jashin no Kuni e (Jpn)

static struct BurnRomInfo gg_shinfrcgRomDesc[] = {
	{ "shining force gaiden - ensei, jashin no kuni e (japan).bin",	0x80000, 0x4d1f4699, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrcg)
STD_ROM_FN(gg_shinfrcg)

struct BurnDriver BurnDrvgg_shinfrcg = {
	"gg_shinfrcg", NULL, NULL, NULL, "1992",
	"Shining Force Gaiden - Ensei, Jashin no Kuni e (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrcgRomInfo, gg_shinfrcgRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force Gaiden - Final Conflict (Jpn)

static struct BurnRomInfo gg_shinfrgfRomDesc[] = {
	{ "shining force gaiden - final conflict (japan).bin",	0x80000, 0x6019fe5e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrgf)
STD_ROM_FN(gg_shinfrgf)

struct BurnDriver BurnDrvgg_shinfrgf = {
	"gg_shinfrgf", NULL, NULL, NULL, "1995",
	"Shining Force Gaiden - Final Conflict (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrgfRomInfo, gg_shinfrgfRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force II - The Sword of Hajya (Euro, USA)

static struct BurnRomInfo gg_shinfrc2RomDesc[] = {
	{ "shining force ii - the sword of hajya (usa, europe).bin",	0x80000, 0xa6ca6fa9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrc2)
STD_ROM_FN(gg_shinfrc2)

struct BurnDriver BurnDrvgg_shinfrc2 = {
	"gg_shinfrc2", NULL, NULL, NULL, "1994",
	"Shining Force II - The Sword of Hajya (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrc2RomInfo, gg_shinfrc2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force Gaiden II - Jashin no Kakusei (Jpn)

static struct BurnRomInfo gg_shinfrg2RomDesc[] = {
	{ "mpr-15570 j07.u1",	0x80000, 0x30374681, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrg2)
STD_ROM_FN(gg_shinfrg2)

struct BurnDriver BurnDrvgg_shinfrg2 = {
	"gg_shinfrg2", "gg_shinfrc2", NULL, NULL, "1993",
	"Shining Force Gaiden II - Jashin no Kakusei (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrg2RomInfo, gg_shinfrg2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force II - The Sword of Hajya (Prototype, 19940427)

static struct BurnRomInfo gg_shinfrc2p4RomDesc[] = {
	{ "shining force ii (prototype - apr 27,  1994).bin",	0x80000, 0x65e5345c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrc2p4)
STD_ROM_FN(gg_shinfrc2p4)

struct BurnDriver BurnDrvgg_shinfrc2p4 = {
	"gg_shinfrc2p4", "gg_shinfrc2", NULL, NULL, "1994",
	"Shining Force II - The Sword of Hajya (Prototype, 19940427)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrc2p4RomInfo, gg_shinfrc2p4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force II - The Sword of Hajya (Prototype, 19940708)

static struct BurnRomInfo gg_shinfrc2p1RomDesc[] = {
	{ "shining force ii (prototype - jul 08,  1994).bin",	0x80000, 0x80dca91d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrc2p1)
STD_ROM_FN(gg_shinfrc2p1)

struct BurnDriver BurnDrvgg_shinfrc2p1 = {
	"gg_shinfrc2p1", "gg_shinfrc2", NULL, NULL, "1994",
	"Shining Force II - The Sword of Hajya (Prototype, 19940708)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrc2p1RomInfo, gg_shinfrc2p1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force II - The Sword of Hajya (Prototype, 19940627)

static struct BurnRomInfo gg_shinfrc2p2RomDesc[] = {
	{ "shining force ii (prototype - jun 27,  1994).bin",	0x80000, 0x8bfc56c3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrc2p2)
STD_ROM_FN(gg_shinfrc2p2)

struct BurnDriver BurnDrvgg_shinfrc2p2 = {
	"gg_shinfrc2p2", "gg_shinfrc2", NULL, NULL, "1994",
	"Shining Force II - The Sword of Hajya (Prototype, 19940627)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrc2p2RomInfo, gg_shinfrc2p2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Shining Force II - The Sword of Hajya (Prototype, 19940530)

static struct BurnRomInfo gg_shinfrc2p3RomDesc[] = {
	{ "shining force ii (prototype - may 30,  1994).bin",	0x80000, 0xe97ec011, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinfrc2p3)
STD_ROM_FN(gg_shinfrc2p3)

struct BurnDriver BurnDrvgg_shinfrc2p3 = {
	"gg_shinfrc2p3", "gg_shinfrc2", NULL, NULL, "1994",
	"Shining Force II - The Sword of Hajya (Prototype, 19940530)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinfrc2p3RomInfo, gg_shinfrc2p3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The GG Shinobi II (World)

static struct BurnRomInfo gg_shinobi2RomDesc[] = {
	{ "mpr-15154-f.ic1",	0x40000, 0x6201c694, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shinobi2)
STD_ROM_FN(gg_shinobi2)

struct BurnDriver BurnDrvgg_shinobi2 = {
	"gg_shinobi2", NULL, NULL, NULL, "1992",
	"The GG Shinobi II (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_shinobi2RomInfo, gg_shinobi2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Side Pocket (USA)

static struct BurnRomInfo gg_sidepockRomDesc[] = {
	{ "side pocket (usa).bin",	0x40000, 0x6a603eed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sidepock)
STD_ROM_FN(gg_sidepock)

struct BurnDriver BurnDrvgg_sidepock = {
	"gg_sidepock", NULL, NULL, NULL, "1994",
	"Side Pocket (USA)\0", NULL, "Data East", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sidepockRomInfo, gg_sidepockRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Simpsons - Bart vs. The Space Mutants (Euro, USA)

static struct BurnRomInfo gg_bartvssmRomDesc[] = {
	{ "mpr-15229-s.ic1",	0x40000, 0xc0009274, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bartvssm)
STD_ROM_FN(gg_bartvssm)

struct BurnDriver BurnDrvgg_bartvssm = {
	"gg_bartvssm", NULL, NULL, NULL, "1992",
	"The Simpsons - Bart vs. The Space Mutants (Euro, USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bartvssmRomInfo, gg_bartvssmRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Simpsons - Bart vs. The World (World)

static struct BurnRomInfo gg_bartvswdRomDesc[] = {
	{ "mpr-15978.ic1",	0x40000, 0xda7bd5c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_bartvswd)
STD_ROM_FN(gg_bartvswd)

struct BurnDriver BurnDrvgg_bartvswd = {
	"gg_bartvswd", NULL, NULL, NULL, "1993",
	"The Simpsons - Bart vs. The World (World)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_bartvswdRomInfo, gg_bartvswdRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Skweek (Jpn)

static struct BurnRomInfo gg_skweekRomDesc[] = {
	{ "skweek (japan).bin",	0x20000, 0x3d9c92c7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_skweek)
STD_ROM_FN(gg_skweek)

struct BurnDriver BurnDrvgg_skweek = {
	"gg_skweek", "gg_slider", NULL, NULL, "1991",
	"Skweek (Jpn)\0", NULL, "Victor Musical Industries", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_skweekRomInfo, gg_skweekRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Slider (Euro, USA)

static struct BurnRomInfo gg_sliderRomDesc[] = {
	{ "slider (usa, europe).bin",	0x20000, 0x4dc6f555, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_slider)
STD_ROM_FN(gg_slider)

struct BurnDriver BurnDrvgg_slider = {
	"gg_slider", NULL, NULL, NULL, "1991",
	"Slider (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sliderRomInfo, gg_sliderRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Smurfs (Euro)

static struct BurnRomInfo gg_smurfsRomDesc[] = {
	{ "smurfs, the (europe) (en,fr,de,es).bin",	0x40000, 0x354e1cbd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smurfs)
STD_ROM_FN(gg_smurfs)

struct BurnDriver BurnDrvgg_smurfs = {
	"gg_smurfs", NULL, NULL, NULL, "1994",
	"The Smurfs (Euro)\0", NULL, "Infogrames", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smurfsRomInfo, gg_smurfsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Smurfs 2 (Euro)

static struct BurnRomInfo gg_smurfs2RomDesc[] = {
	{ "smurfs travel the world, the.bin",	0x40000, 0x3eb337df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smurfs2)
STD_ROM_FN(gg_smurfs2)

struct BurnDriver BurnDrvgg_smurfs2 = {
	"gg_smurfs2", NULL, NULL, NULL, "1995",
	"The Smurfs 2 (Euro)\0", NULL, "Infogrames", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smurfs2RomInfo, gg_smurfs2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Solitaire Funpak (USA)

static struct BurnRomInfo gg_solitairRomDesc[] = {
	{ "solitaire funpak (usa).bin",	0x40000, 0xf6f24b75, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_solitair)
STD_ROM_FN(gg_solitair)

struct BurnDriver BurnDrvgg_solitair = {
	"gg_solitair", NULL, NULL, NULL, "1994",
	"Solitaire Funpak (USA)\0", NULL, "Interplay", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_solitairRomInfo, gg_solitairRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Solitaire Poker (Euro, USA)

static struct BurnRomInfo gg_solitarpRomDesc[] = {
	{ "solitaire poker (usa, europe).bin",	0x20000, 0x06f2fc46, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_solitarp)
STD_ROM_FN(gg_solitarp)

struct BurnDriver BurnDrvgg_solitarp = {
	"gg_solitarp", NULL, NULL, NULL, "1991",
	"Solitaire Poker (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_solitarpRomInfo, gg_solitarpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic and Tails (Jpn)

static struct BurnRomInfo gg_sonictlsRomDesc[] = {
	{ "sonic and tails (japan).bin",	0x80000, 0x8ac0dade, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonictls)
STD_ROM_FN(gg_sonictls)

struct BurnDriver BurnDrvgg_sonictls = {
	"gg_sonictls", "gg_sonicc", NULL, NULL, "1993",
	"Sonic and Tails (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonictlsRomInfo, gg_sonictlsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic and Tails (Jpn, Jitsuenyou Sample)

static struct BurnRomInfo gg_sonictlssRomDesc[] = {
	{ "sonic and tails [demo] (jp).bin",	0x80000, 0xe0e3fb6a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonictlss)
STD_ROM_FN(gg_sonictlss)

struct BurnDriver BurnDrvgg_sonictlss = {
	"gg_sonictlss", "gg_sonicc", NULL, NULL, "1993",
	"Sonic and Tails (Jpn, Jitsuenyou Sample)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonictlssRomInfo, gg_sonictlssRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic and Tails 2 (Jpn)

static struct BurnRomInfo gg_sonictl2RomDesc[] = {
	{ "sonic and tails 2 (japan).bin",	0x80000, 0x496bce64, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonictl2)
STD_ROM_FN(gg_sonictl2)

struct BurnDriver BurnDrvgg_sonictl2 = {
	"gg_sonictl2", "gg_sonictri", NULL, NULL, "1994",
	"Sonic and Tails 2 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonictl2RomInfo, gg_sonictl2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Euro, USA) ~ G Sonic (Jpn)

static struct BurnRomInfo gg_sonicblsRomDesc[] = {
	{ "mpr-19071-s.ic2",	0x100000, 0x031b9da9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicbls)
STD_ROM_FN(gg_sonicbls)

struct BurnDriver BurnDrvgg_sonicbls = {
	"gg_sonicbls", NULL, NULL, NULL, "1996",
	"Sonic Blast (Euro, USA) ~ G Sonic (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicblsRomInfo, gg_sonicblsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Prototype 611, 19960531, 13.49)

static struct BurnRomInfo gg_sonicblsp6RomDesc[] = {
	{ "sonic blast (prototype 611 - may 31,  1996, 13.49).bin",	0x80000, 0xc09ef45b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicblsp6)
STD_ROM_FN(gg_sonicblsp6)

struct BurnDriver BurnDrvgg_sonicblsp6 = {
	"gg_sonicblsp6", "gg_sonicbls", NULL, NULL, "1996",
	"Sonic Blast (Prototype 611, 19960531, 13.49)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicblsp6RomInfo, gg_sonicblsp6RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Prototype 74, 19960717, 09.11)

static struct BurnRomInfo gg_sonicblsp5RomDesc[] = {
	{ "sonic blast (prototype 74 - jul 17,  1996, 09.11).bin",	0x100000, 0x362b186d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicblsp5)
STD_ROM_FN(gg_sonicblsp5)

struct BurnDriver BurnDrvgg_sonicblsp5 = {
	"gg_sonicblsp5", "gg_sonicbls", NULL, NULL, "1996",
	"Sonic Blast (Prototype 74, 19960717, 09.11)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicblsp5RomInfo, gg_sonicblsp5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Prototype 806, 19960806, 18.37)

static struct BurnRomInfo gg_sonicblsp4RomDesc[] = {
	{ "sonic blast (prototype 806 - aug 06,  1996, 18.37).bin",	0x100000, 0x19dad067, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicblsp4)
STD_ROM_FN(gg_sonicblsp4)

struct BurnDriver BurnDrvgg_sonicblsp4 = {
	"gg_sonicblsp4", "gg_sonicbls", NULL, NULL, "1996",
	"Sonic Blast (Prototype 806, 19960806, 18.37)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicblsp4RomInfo, gg_sonicblsp4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Prototype 821, 19960822, 09.38)

static struct BurnRomInfo gg_sonicblsp3RomDesc[] = {
	{ "sonic blast (prototype 821 - aug 22,  1996, 09.38).bin",	0x100000, 0x5eecb549, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicblsp3)
STD_ROM_FN(gg_sonicblsp3)

struct BurnDriver BurnDrvgg_sonicblsp3 = {
	"gg_sonicblsp3", "gg_sonicbls", NULL, NULL, "1996",
	"Sonic Blast (Prototype 821, 19960822, 09.38)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicblsp3RomInfo, gg_sonicblsp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Prototype 827, 19960828, 11.13)

static struct BurnRomInfo gg_sonicblsp2RomDesc[] = {
	{ "sonic blast (prototype 827 - aug 28,  1996, 11.13).bin",	0x100000, 0xe6847ac0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicblsp2)
STD_ROM_FN(gg_sonicblsp2)

struct BurnDriver BurnDrvgg_sonicblsp2 = {
	"gg_sonicblsp2", "gg_sonicbls", NULL, NULL, "1996",
	"Sonic Blast (Prototype 827, 19960828, 11.13)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicblsp2RomInfo, gg_sonicblsp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Blast (Prototype 94, 19960902, 17.47)

static struct BurnRomInfo gg_sonicblsp1RomDesc[] = {
	{ "sonic blast (prototype 94 - sep 02,  1996, 17.47).bin",	0x100000, 0x2f22a352, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicblsp1)
STD_ROM_FN(gg_sonicblsp1)

struct BurnDriver BurnDrvgg_sonicblsp1 = {
	"gg_sonicblsp1", "gg_sonicbls", NULL, NULL, "1996",
	"Sonic Blast (Prototype 94, 19960902, 17.47)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicblsp1RomInfo, gg_sonicblsp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Chaos (Euro, USA, Bra)

static struct BurnRomInfo gg_soniccRomDesc[] = {
	{ "mpr-15973-f.ic1",	0x80000, 0x663f2abb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicc)
STD_ROM_FN(gg_sonicc)

struct BurnDriver BurnDrvgg_sonicc = {
	"gg_sonicc", NULL, NULL, NULL, "199?",
	"Sonic Chaos (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_soniccRomInfo, gg_soniccRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Drift (Jpn, Demo Sample)

static struct BurnRomInfo gg_sonicdrsRomDesc[] = {
	{ "sonic drift (japan) (sample).bin",	0x80000, 0xbaac1fc0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicdrs)
STD_ROM_FN(gg_sonicdrs)

struct BurnDriver BurnDrvgg_sonicdrs = {
	"gg_sonicdrs", "gg_sonicdr", NULL, NULL, "1994",
	"Sonic Drift (Jpn, Demo Sample)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicdrsRomInfo, gg_sonicdrsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Drift (Jpn)

static struct BurnRomInfo gg_sonicdrRomDesc[] = {
	{ "sonic drift (japan).bin",	0x80000, 0x68f0a776, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicdr)
STD_ROM_FN(gg_sonicdr)

struct BurnDriver BurnDrvgg_sonicdr = {
	"gg_sonicdr", NULL, NULL, NULL, "1994",
	"Sonic Drift (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicdrRomInfo, gg_sonicdrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Drift 2 (Jpn, USA)

static struct BurnRomInfo gg_sonicdr2RomDesc[] = {
	{ "mpr-17623.ic1",	0x80000, 0xd6e8a305, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicdr2)
STD_ROM_FN(gg_sonicdr2)

struct BurnDriver BurnDrvgg_sonicdr2 = {
	"gg_sonicdr2", NULL, NULL, NULL, "1995",
	"Sonic Drift 2 (Jpn, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicdr2RomInfo, gg_sonicdr2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Labyrinth (World)

static struct BurnRomInfo gg_soniclabRomDesc[] = {
	{ "mpr-18314-s.ic1",	0x80000, 0x5550173b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_soniclab)
STD_ROM_FN(gg_soniclab)

struct BurnDriver BurnDrvgg_soniclab = {
	"gg_soniclab", NULL, NULL, NULL, "1995",
	"Sonic Labyrinth (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_soniclabRomInfo, gg_soniclabRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Euro, USA)

static struct BurnRomInfo gg_sspinRomDesc[] = {
	{ "sonic spinball (usa, europe).bin",	0x80000, 0xa9210434, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspin)
STD_ROM_FN(gg_sspin)

struct BurnDriver BurnDrvgg_sspin = {
	"gg_sspin", NULL, NULL, NULL, "1993",
	"Sonic Spinball (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinRomInfo, gg_sspinRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0307, 19940307, 05.24)

static struct BurnRomInfo gg_sspinp20RomDesc[] = {
	{ "sonic spinball (prototype 0307 - mar 07,  1994, 05.24).bin",	0x80000, 0x3283580d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp20)
STD_ROM_FN(gg_sspinp20)

struct BurnDriver BurnDrvgg_sspinp20 = {
	"gg_sspinp20", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0307, 19940307, 05.24)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp20RomInfo, gg_sspinp20RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 031094, 19940310, 17.41)

static struct BurnRomInfo gg_sspinp19RomDesc[] = {
	{ "sonic spinball (prototype 031094 - mar 10,  1994, 17.41).bin",	0x80000, 0x1a736612, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp19)
STD_ROM_FN(gg_sspinp19)

struct BurnDriver BurnDrvgg_sspinp19 = {
	"gg_sspinp19", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 031094, 19940310, 17.41)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp19RomInfo, gg_sspinp19RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 031194, 19940311, 12.09)

static struct BurnRomInfo gg_sspinp18RomDesc[] = {
	{ "sonic spinball (prototype 031194 - mar 11,  1994, 12.09).bin",	0x80000, 0x1aed8f62, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp18)
STD_ROM_FN(gg_sspinp18)

struct BurnDriver BurnDrvgg_sspinp18 = {
	"gg_sspinp18", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 031194, 19940311, 12.09)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp18RomInfo, gg_sspinp18RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 032194, 19940321, 07.12)

static struct BurnRomInfo gg_sspinp17RomDesc[] = {
	{ "sonic spinball (prototype 032194 - mar 21,  1994, 07.12).bin",	0x80000, 0x46657b4e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp17)
STD_ROM_FN(gg_sspinp17)

struct BurnDriver BurnDrvgg_sspinp17 = {
	"gg_sspinp17", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 032194, 19940321, 07.12)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp17RomInfo, gg_sspinp17RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0328, 19940328, 06.54)

static struct BurnRomInfo gg_sspinp16RomDesc[] = {
	{ "sonic spinball (prototype 0328 - mar 28,  1994, 06.54).bin",	0x80000, 0x1233c3e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp16)
STD_ROM_FN(gg_sspinp16)

struct BurnDriver BurnDrvgg_sspinp16 = {
	"gg_sspinp16", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0328, 19940328, 06.54)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp16RomInfo, gg_sspinp16RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0329, 19940329, 07.35)

static struct BurnRomInfo gg_sspinp15RomDesc[] = {
	{ "sonic spinball (prototype 0329 - mar 29,  1994, 07.35).bin",	0x80000, 0xaa7aedb0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp15)
STD_ROM_FN(gg_sspinp15)

struct BurnDriver BurnDrvgg_sspinp15 = {
	"gg_sspinp15", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0329, 19940329, 07.35)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp15RomInfo, gg_sspinp15RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0330, 19940330, 06.54)

static struct BurnRomInfo gg_sspinp14RomDesc[] = {
	{ "sonic spinball (prototype 0330 - mar 30,  1994, 06.54).bin",	0x80000, 0x2ca34ab2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp14)
STD_ROM_FN(gg_sspinp14)

struct BurnDriver BurnDrvgg_sspinp14 = {
	"gg_sspinp14", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0330, 19940330, 06.54)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp14RomInfo, gg_sspinp14RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0405, 19940405, 18.07)

static struct BurnRomInfo gg_sspinp13RomDesc[] = {
	{ "sonic spinball (prototype 0405 - apr 05,  1994, 18.07).bin",	0x80000, 0x09b913d5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp13)
STD_ROM_FN(gg_sspinp13)

struct BurnDriver BurnDrvgg_sspinp13 = {
	"gg_sspinp13", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0405, 19940405, 18.07)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp13RomInfo, gg_sspinp13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0412, 19940412, 07.26)

static struct BurnRomInfo gg_sspinp12RomDesc[] = {
	{ "sonic spinball (prototype 0412 - apr 12,  1994, 07.26).bin",	0x80000, 0xf8a432f6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp12)
STD_ROM_FN(gg_sspinp12)

struct BurnDriver BurnDrvgg_sspinp12 = {
	"gg_sspinp12", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0412, 19940412, 07.26)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp12RomInfo, gg_sspinp12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0413, 19940413, 02.54)

static struct BurnRomInfo gg_sspinp11RomDesc[] = {
	{ "sonic spinball (prototype 0413 - apr 13,  1994, 02.54).bin",	0x80000, 0x1f1abf2c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp11)
STD_ROM_FN(gg_sspinp11)

struct BurnDriver BurnDrvgg_sspinp11 = {
	"gg_sspinp11", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0413, 19940413, 02.54)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp11RomInfo, gg_sspinp11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0415, 19940415, 17.33)

static struct BurnRomInfo gg_sspinp10RomDesc[] = {
	{ "sonic spinball (prototype 0415 - apr 15,  1994, 17.33).bin",	0x80000, 0xe3b5cd1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp10)
STD_ROM_FN(gg_sspinp10)

struct BurnDriver BurnDrvgg_sspinp10 = {
	"gg_sspinp10", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0415, 19940415, 17.33)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp10RomInfo, gg_sspinp10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0421, 19940421, 05.30)

static struct BurnRomInfo gg_sspinp09RomDesc[] = {
	{ "sonic spinball (prototype 0421 - apr 21,  1994, 05.30).bin",	0x80000, 0xbbaecf7c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp09)
STD_ROM_FN(gg_sspinp09)

struct BurnDriver BurnDrvgg_sspinp09 = {
	"gg_sspinp09", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0421, 19940421, 05.30)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp09RomInfo, gg_sspinp09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0427, 19940427, 04.51)

static struct BurnRomInfo gg_sspinp08RomDesc[] = {
	{ "sonic spinball (prototype 0427 - apr 27,  1994, 04.51).bin",	0x5c000, 0x07d32244, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp08)
STD_ROM_FN(gg_sspinp08)

struct BurnDriver BurnDrvgg_sspinp08 = {
	"gg_sspinp08", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0427, 19940427, 04.51)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp08RomInfo, gg_sspinp08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0429, 19940429, 06.01)

static struct BurnRomInfo gg_sspinp07RomDesc[] = {
	{ "sonic spinball (prototype 0429 - apr 29,  1994, 06.01).bin",	0x80000, 0x30a7d062, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp07)
STD_ROM_FN(gg_sspinp07)

struct BurnDriver BurnDrvgg_sspinp07 = {
	"gg_sspinp07", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0429, 19940429, 06.01)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp07RomInfo, gg_sspinp07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0430, 19940429, 21.48)

static struct BurnRomInfo gg_sspinp06RomDesc[] = {
	{ "sonic spinball (prototype 0430 - apr 29,  1994, 21.48).bin",	0x80000, 0x221c4ba4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp06)
STD_ROM_FN(gg_sspinp06)

struct BurnDriver BurnDrvgg_sspinp06 = {
	"gg_sspinp06", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0430, 19940429, 21.48)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp06RomInfo, gg_sspinp06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 503B, 19940503, 19.27)

static struct BurnRomInfo gg_sspinp05RomDesc[] = {
	{ "sonic spinball (prototype 503b - may 03,  1994, 19.27).bin",	0x80000, 0xe2fe976a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp05)
STD_ROM_FN(gg_sspinp05)

struct BurnDriver BurnDrvgg_sspinp05 = {
	"gg_sspinp05", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 503B, 19940503, 19.27)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp05RomInfo, gg_sspinp05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0505, 19940505, 04.47)

static struct BurnRomInfo gg_sspinp04RomDesc[] = {
	{ "sonic spinball (prototype 0505 - may 05,  1994, 04.47).bin",	0x80000, 0xbe76bf30, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp04)
STD_ROM_FN(gg_sspinp04)

struct BurnDriver BurnDrvgg_sspinp04 = {
	"gg_sspinp04", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0505, 19940505, 04.47)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp04RomInfo, gg_sspinp04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0506, 19940506, 13.04)

static struct BurnRomInfo gg_sspinp03RomDesc[] = {
	{ "sonic spinball (prototype 0506 - may 06,  1994, 13.04).bin",	0x80000, 0xa4833d27, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp03)
STD_ROM_FN(gg_sspinp03)

struct BurnDriver BurnDrvgg_sspinp03 = {
	"gg_sspinp03", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0506, 19940506, 13.04)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp03RomInfo, gg_sspinp03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0525, 19940525, 13.24)

static struct BurnRomInfo gg_sspinp02RomDesc[] = {
	{ "sonic spinball (prototype 0525 - may 25,  1994, 13.24).bin",	0x80000, 0xa035fd5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp02)
STD_ROM_FN(gg_sspinp02)

struct BurnDriver BurnDrvgg_sspinp02 = {
	"gg_sspinp02", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0525, 19940525, 13.24)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp02RomInfo, gg_sspinp02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic Spinball (Prototype 0530, 19940530, 23.25)

static struct BurnRomInfo gg_sspinp01RomDesc[] = {
	{ "sonic spinball (prototype 0530 - may 30,  1994, 23.25).bin",	0x80000, 0x6107819d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sspinp01)
STD_ROM_FN(gg_sspinp01)

struct BurnDriver BurnDrvgg_sspinp01 = {
	"gg_sspinp01", "gg_sspin", NULL, NULL, "1994",
	"Sonic Spinball (Prototype 0530, 19940530, 23.25)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sspinp01RomInfo, gg_sspinp01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog (Jpn, USA, v0)

static struct BurnRomInfo gg_sonicjRomDesc[] = {
	{ "sonic the hedgehog (japan).bin",	0x40000, 0x3e31cb8c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicj)
STD_ROM_FN(gg_sonicj)

struct BurnDriver BurnDrvgg_sonicj = {
	"gg_sonicj", "gg_sonic", NULL, NULL, "1991",
	"Sonic The Hedgehog (Jpn, USA, v0)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicjRomInfo, gg_sonicjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog (Euro, Jpn, v1)

static struct BurnRomInfo gg_sonicRomDesc[] = {
	{ "mpr-14459a.ic1",	0x40000, 0xd163356e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonic)
STD_ROM_FN(gg_sonic)

struct BurnDriver BurnDrvgg_sonic = {
	"gg_sonic", NULL, NULL, NULL, "1991",
	"Sonic The Hedgehog (Euro, Jpn, v1)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicRomInfo, gg_sonicRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog (World, Prototype)

static struct BurnRomInfo gg_sonicpRomDesc[] = {
	{ "sonic the hedgehog (world) (proto).bin",	0x40000, 0x816c0a1e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonicp)
STD_ROM_FN(gg_sonicp)

struct BurnDriver BurnDrvgg_sonicp = {
	"gg_sonicp", "gg_sonic", NULL, NULL, "1991",
	"Sonic The Hedgehog (World, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonicpRomInfo, gg_sonicpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog 2 (World)

static struct BurnRomInfo gg_sonic2RomDesc[] = {
	{ "mpr-15169 j02.ic1",	0x80000, 0x95a18ec7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonic2)
STD_ROM_FN(gg_sonic2)

struct BurnDriver BurnDrvgg_sonic2 = {
	"gg_sonic2", NULL, NULL, NULL, "1992",
	"Sonic The Hedgehog 2 (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonic2RomInfo, gg_sonic2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog 2 (Rolling Demo)

static struct BurnRomInfo gg_sonic2dRomDesc[] = {
	{ "sonic_2_auto_demo_prototype_(1991-12-05)_tmr_sega_cb06_ffff.bin",	0x40000, 0x15ad37a5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonic2d)
STD_ROM_FN(gg_sonic2d)

struct BurnDriver BurnDrvgg_sonic2d = {
	"gg_sonic2d", "gg_sonic2", NULL, NULL, "1992",
	"Sonic The Hedgehog 2 (Rolling Demo)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonic2dRomInfo, gg_sonic2dRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog - Triple Trouble (Euro, USA)

static struct BurnRomInfo gg_sonictriRomDesc[] = {
	{ "mpr-17075.ic1",	0x80000, 0xd23a2a93, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonictri)
STD_ROM_FN(gg_sonictri)

struct BurnDriver BurnDrvgg_sonictri = {
	"gg_sonictri", NULL, NULL, NULL, "1994",
	"Sonic The Hedgehog - Triple Trouble (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonictriRomInfo, gg_sonictriRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sonic The Hedgehog - Triple Trouble (Prototype 0808, 19940808, 18.05)

static struct BurnRomInfo gg_sonictripRomDesc[] = {
	{ "sonic the hedgehog - triple trouble (prototype 0808 - aug 08,  1994, 18.05).bin",	0x80000, 0x80eb7cfb, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sonictrip)
STD_ROM_FN(gg_sonictrip)

struct BurnDriver BurnDrvgg_sonictrip = {
	"gg_sonictrip", "gg_sonictri", NULL, NULL, "1994",
	"Sonic The Hedgehog - Triple Trouble (Prototype 0808, 19940808, 18.05)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sonictripRomInfo, gg_sonictripRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Soukoban (Jpn)

static struct BurnRomInfo gg_sokobanRomDesc[] = {
	{ "soukoban (japan).bin",	0x20000, 0x0f3e3840, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sokoban)
STD_ROM_FN(gg_sokoban)

struct BurnDriver BurnDrvgg_sokoban = {
	"gg_sokoban", NULL, NULL, NULL, "1990",
	"Soukoban (Jpn)\0", NULL, "Riverhill Software", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sokobanRomInfo, gg_sokobanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Space Harrier (World)

static struct BurnRomInfo gg_sharrierRomDesc[] = {
	{ "mpr-14346.ic1",	0x20000, 0x600c15b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sharrier)
STD_ROM_FN(gg_sharrier)

struct BurnDriver BurnDrvgg_sharrier = {
	"gg_sharrier", NULL, NULL, NULL, "1991",
	"Space Harrier (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sharrierRomInfo, gg_sharrierRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Cheese Cat-astrophe Starring Speedy Gonzales (Euro)

static struct BurnRomInfo gg_cheeseRomDesc[] = {
	{ "cheese catastrophe (euro).bin",	0x80000, 0x4e78fd36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_cheese)
STD_ROM_FN(gg_cheese)

struct BurnDriver BurnDrvgg_cheese = {
	"gg_cheese", NULL, NULL, NULL, "1995",
	"Cheese Cat-astrophe Starring Speedy Gonzales (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_cheeseRomInfo, gg_cheeseRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spider-Man - Return of the Sinister Six (Euro, USA)

static struct BurnRomInfo gg_spidermnRomDesc[] = {
	{ "spider-man - return of the sinister six (usa, europe).bin",	0x40000, 0xbc240779, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_spidermn)
STD_ROM_FN(gg_spidermn)

struct BurnDriver BurnDrvgg_spidermn = {
	"gg_spidermn", NULL, NULL, NULL, "1993",
	"Spider-Man - Return of the Sinister Six (Euro, USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_spidermnRomInfo, gg_spidermnRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spider-Man and the X-Men in Arcade's Revenge (USA)

static struct BurnRomInfo gg_spidxmenRomDesc[] = {
	{ "spider-man and the x-men in arcade's revenge (usa).bin",	0x80000, 0x742a372b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_spidxmen)
STD_ROM_FN(gg_spidxmen)

struct BurnDriver BurnDrvgg_spidxmen = {
	"gg_spidxmen", NULL, NULL, NULL, "1992",
	"Spider-Man and the X-Men in Arcade's Revenge (USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_spidxmenRomInfo, gg_spidxmenRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spider-Man vs. The Kingpin (Euro, USA)

static struct BurnRomInfo gg_spidkingRomDesc[] = {
	{ "spider-man vs. the kingpin (usa, europe).bin",	0x40000, 0x2651024e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_spidking)
STD_ROM_FN(gg_spidking)

struct BurnDriver BurnDrvgg_spidking = {
	"gg_spidking", NULL, NULL, NULL, "1992",
	"Spider-Man vs. The Kingpin (Euro, USA)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_spidkingRomInfo, gg_spidkingRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Spirou (Euro, Prototype)

static struct BurnRomInfo gg_spirouRomDesc[] = {
	{ "spirou (europe) (proto).bin",	0x80000, 0xab622adc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_spirou)
STD_ROM_FN(gg_spirou)

struct BurnDriver BurnDrvgg_spirou = {
	"gg_spirou", NULL, NULL, NULL, "199?",
	"Spirou (Euro, Prototype)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_spirouRomInfo, gg_spirouRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Illustrated Championship Football and Baseball (Euro, USA)

static struct BurnRomInfo gg_sportillRomDesc[] = {
	{ "sports illustrated championship football and baseball (usa, europe).bin",	0x80000, 0xde25e2d8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sportill)
STD_ROM_FN(gg_sportill)

struct BurnDriver BurnDrvgg_sportill = {
	"gg_sportill", NULL, NULL, NULL, "1995",
	"Sports Illustrated Championship Football and Baseball (Euro, USA)\0", NULL, "Black Pearl", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sportillRomInfo, gg_sportillRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (USA)

static struct BurnRomInfo gg_sporttrvRomDesc[] = {
	{ "sports trivia (usa).bin",	0x20000, 0xa7af7ca9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrv)
STD_ROM_FN(gg_sporttrv)

struct BurnDriver BurnDrvgg_sporttrv = {
	"gg_sporttrv", NULL, NULL, NULL, "1995",
	"Sports Trivia (USA)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvRomInfo, gg_sporttrvRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950308)

static struct BurnRomInfo gg_sporttrvp12RomDesc[] = {
	{ "sports trivia (prototype - mar 08,  1995).bin",	0x20000, 0xc38d41c3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp12)
STD_ROM_FN(gg_sporttrvp12)

struct BurnDriver BurnDrvgg_sporttrvp12 = {
	"gg_sporttrvp12", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950308)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp12RomInfo, gg_sporttrvp12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950309)

static struct BurnRomInfo gg_sporttrvp11RomDesc[] = {
	{ "sports trivia (prototype - mar 09,  1995).bin",	0x20000, 0x34407fe9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp11)
STD_ROM_FN(gg_sporttrvp11)

struct BurnDriver BurnDrvgg_sporttrvp11 = {
	"gg_sporttrvp11", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950309)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp11RomInfo, gg_sporttrvp11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950313)

static struct BurnRomInfo gg_sporttrvp10RomDesc[] = {
	{ "sports trivia (prototype - mar 13,  1995).bin",	0x40000, 0xa8d1ac51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp10)
STD_ROM_FN(gg_sporttrvp10)

struct BurnDriver BurnDrvgg_sporttrvp10 = {
	"gg_sporttrvp10", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950313)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp10RomInfo, gg_sporttrvp10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950316-1MEG)

static struct BurnRomInfo gg_sporttrvp09RomDesc[] = {
	{ "sports trivia (prototype - mar 16,  1995 - 1meg).bin",	0x20000, 0x464f1b51, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp09)
STD_ROM_FN(gg_sporttrvp09)

struct BurnDriver BurnDrvgg_sporttrvp09 = {
	"gg_sporttrvp09", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950316-1MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp09RomInfo, gg_sporttrvp09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950316-2MEG)

static struct BurnRomInfo gg_sporttrvp08RomDesc[] = {
	{ "sports trivia (prototype - mar 16,  1995 - 2meg).bin",	0x40000, 0xcaf87d8b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp08)
STD_ROM_FN(gg_sporttrvp08)

struct BurnDriver BurnDrvgg_sporttrvp08 = {
	"gg_sporttrvp08", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950316-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp08RomInfo, gg_sporttrvp08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950317-2MEG)

static struct BurnRomInfo gg_sporttrvp07RomDesc[] = {
	{ "sports trivia (prototype - mar 17,  1995 - 2meg).bin",	0x40000, 0x43bb6b10, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp07)
STD_ROM_FN(gg_sporttrvp07)

struct BurnDriver BurnDrvgg_sporttrvp07 = {
	"gg_sporttrvp07", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950317-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp07RomInfo, gg_sporttrvp07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950320-2MEG)

static struct BurnRomInfo gg_sporttrvp06RomDesc[] = {
	{ "sports trivia (prototype - mar 20,  1995 - 2meg).bin",	0x40000, 0xe7aff566, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp06)
STD_ROM_FN(gg_sporttrvp06)

struct BurnDriver BurnDrvgg_sporttrvp06 = {
	"gg_sporttrvp06", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950320-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp06RomInfo, gg_sporttrvp06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950320-QTEST)

static struct BurnRomInfo gg_sporttrvp05RomDesc[] = {
	{ "sports trivia (prototype - mar 20,  1995 - qtest).bin",	0x40000, 0xa3e57816, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp05)
STD_ROM_FN(gg_sporttrvp05)

struct BurnDriver BurnDrvgg_sporttrvp05 = {
	"gg_sporttrvp05", "gg_sporttrv", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950320-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp05RomInfo, gg_sporttrvp05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950425-1MEG)

static struct BurnRomInfo gg_sporttrvp04RomDesc[] = {
	{ "sports trivia (prototype - apr 25,  1995 - 1meg).bin",	0x20000, 0x895af58a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp04)
STD_ROM_FN(gg_sporttrvp04)

struct BurnDriver BurnDrvgg_sporttrvp04 = {
	"gg_sporttrvp04", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950425-1MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp04RomInfo, gg_sporttrvp04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950425-QTEST)

static struct BurnRomInfo gg_sporttrvp03RomDesc[] = {
	{ "sports trivia (prototype - apr 25,  1995 - qtest).bin",	0x20000, 0x7524957c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp03)
STD_ROM_FN(gg_sporttrvp03)

struct BurnDriver BurnDrvgg_sporttrvp03 = {
	"gg_sporttrvp03", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950425-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp03RomInfo, gg_sporttrvp03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950427-1MEG)

static struct BurnRomInfo gg_sporttrvp02RomDesc[] = {
	{ "sports trivia (prototype - apr 27,  1995 - 1meg).bin",	0x20000, 0x7898919f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp02)
STD_ROM_FN(gg_sporttrvp02)

struct BurnDriver BurnDrvgg_sporttrvp02 = {
	"gg_sporttrvp02", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950427-1MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp02RomInfo, gg_sporttrvp02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950504-1MEG)

static struct BurnRomInfo gg_sporttrvp01RomDesc[] = {
	{ "sports trivia (prototype - may 04,  1995 - 1meg).bin",	0x20000, 0xbba4cf7b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sporttrvp01)
STD_ROM_FN(gg_sporttrvp01)

struct BurnDriver BurnDrvgg_sporttrvp01 = {
	"gg_sporttrvp01", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950504-1MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sporttrvp01RomInfo, gg_sporttrvp01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia - Championship Edition (Euro, USA)

static struct BurnRomInfo gg_sprtrvceRomDesc[] = {
	{ "sports trivia - championship edition (usa, europe).bin",	0x40000, 0x5b5de94d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvce)
STD_ROM_FN(gg_sprtrvce)

struct BurnDriver BurnDrvgg_sprtrvce = {
	"gg_sprtrvce", NULL, NULL, NULL, "1995",
	"Sports Trivia - Championship Edition (Euro, USA)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvceRomInfo, gg_sprtrvceRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950502-2MEG)

static struct BurnRomInfo gg_sprtrvcep01RomDesc[] = {
	{ "sports trivia (prototype - may 02,  1995 - 2meg).bin",	0x40000, 0x15e7e261, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep01)
STD_ROM_FN(gg_sprtrvcep01)

struct BurnDriver BurnDrvgg_sprtrvcep01 = {
	"gg_sprtrvcep01", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950502-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep01RomInfo, gg_sprtrvcep01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950403-2MEG)

static struct BurnRomInfo gg_sprtrvcep13RomDesc[] = {
	{ "sports trivia (prototype - apr 03,  1995 - 2meg).bin",	0x40000, 0x3e3d6728, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep13)
STD_ROM_FN(gg_sprtrvcep13)

struct BurnDriver BurnDrvgg_sprtrvcep13 = {
	"gg_sprtrvcep13", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950403-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep13RomInfo, gg_sprtrvcep13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950403-QTEST)

static struct BurnRomInfo gg_sprtrvcep12RomDesc[] = {
	{ "sports trivia (prototype - apr 03,  1995 - qtest).bin",	0x40000, 0xc68762e6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep12)
STD_ROM_FN(gg_sprtrvcep12)

struct BurnDriver BurnDrvgg_sprtrvcep12 = {
	"gg_sprtrvcep12", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950403-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep12RomInfo, gg_sprtrvcep12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950404-2MEG)

static struct BurnRomInfo gg_sprtrvcep11RomDesc[] = {
	{ "sports trivia (prototype - apr 04,  1995 - 2meg).bin",	0x40000, 0xd61a366f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep11)
STD_ROM_FN(gg_sprtrvcep11)

struct BurnDriver BurnDrvgg_sprtrvcep11 = {
	"gg_sprtrvcep11", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950404-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep11RomInfo, gg_sprtrvcep11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950405-2MEG-B)

static struct BurnRomInfo gg_sprtrvcep10RomDesc[] = {
	{ "sports trivia (prototype - apr 05,  1995 - 2meg - b).bin",	0x40000, 0x818ec54c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep10)
STD_ROM_FN(gg_sprtrvcep10)

struct BurnDriver BurnDrvgg_sprtrvcep10 = {
	"gg_sprtrvcep10", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950405-2MEG-B)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep10RomInfo, gg_sprtrvcep10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950405-2MEG-C)

static struct BurnRomInfo gg_sprtrvcep09RomDesc[] = {
	{ "sports trivia (prototype - apr 05,  1995 - 2meg - c).bin",	0x40000, 0xf88a55f4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep09)
STD_ROM_FN(gg_sprtrvcep09)

struct BurnDriver BurnDrvgg_sprtrvcep09 = {
	"gg_sprtrvcep09", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950405-2MEG-C)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep09RomInfo, gg_sprtrvcep09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950405-QTEST)

static struct BurnRomInfo gg_sprtrvcep08RomDesc[] = {
	{ "sports trivia (prototype - apr 05,  1995 - qtest).bin",	0x40000, 0x93b98f19, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep08)
STD_ROM_FN(gg_sprtrvcep08)

struct BurnDriver BurnDrvgg_sprtrvcep08 = {
	"gg_sprtrvcep08", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950405-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep08RomInfo, gg_sprtrvcep08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950406-2MEG)

static struct BurnRomInfo gg_sprtrvcep07RomDesc[] = {
	{ "sports trivia (prototype - apr 06,  1995 - 2meg).bin",	0x40000, 0xcab74070, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep07)
STD_ROM_FN(gg_sprtrvcep07)

struct BurnDriver BurnDrvgg_sprtrvcep07 = {
	"gg_sprtrvcep07", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950406-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep07RomInfo, gg_sprtrvcep07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950407-2MEG)

static struct BurnRomInfo gg_sprtrvcep06RomDesc[] = {
	{ "sports trivia (prototype - apr 07,  1995 - 2meg).bin",	0x40000, 0x352e8b8c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep06)
STD_ROM_FN(gg_sprtrvcep06)

struct BurnDriver BurnDrvgg_sprtrvcep06 = {
	"gg_sprtrvcep06", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950407-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep06RomInfo, gg_sprtrvcep06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950407-QTEST)

static struct BurnRomInfo gg_sprtrvcep05RomDesc[] = {
	{ "sports trivia (prototype - apr 07,  1995 - qtest).bin",	0x40000, 0x961ae378, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep05)
STD_ROM_FN(gg_sprtrvcep05)

struct BurnDriver BurnDrvgg_sprtrvcep05 = {
	"gg_sprtrvcep05", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950407-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep05RomInfo, gg_sprtrvcep05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950411-2MEG)

static struct BurnRomInfo gg_sprtrvcep04RomDesc[] = {
	{ "sports trivia (prototype - apr 11,  1995 - 2meg).bin",	0x40000, 0x8bdb184d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep04)
STD_ROM_FN(gg_sprtrvcep04)

struct BurnDriver BurnDrvgg_sprtrvcep04 = {
	"gg_sprtrvcep04", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950411-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep04RomInfo, gg_sprtrvcep04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950412-2MEG)

static struct BurnRomInfo gg_sprtrvcep03RomDesc[] = {
	{ "sports trivia (prototype - apr 12,  1995 - 2meg).bin",	0x40000, 0x1e360a20, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep03)
STD_ROM_FN(gg_sprtrvcep03)

struct BurnDriver BurnDrvgg_sprtrvcep03 = {
	"gg_sprtrvcep03", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950412-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep03RomInfo, gg_sprtrvcep03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950428-2MEG)

static struct BurnRomInfo gg_sprtrvcep02RomDesc[] = {
	{ "sports trivia (prototype - apr 28,  1995 - 2meg).bin",	0x40000, 0x26470d2b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep02)
STD_ROM_FN(gg_sprtrvcep02)

struct BurnDriver BurnDrvgg_sprtrvcep02 = {
	"gg_sprtrvcep02", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950428-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep02RomInfo, gg_sprtrvcep02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950321-2MEG)

static struct BurnRomInfo gg_sprtrvcep24RomDesc[] = {
	{ "sports trivia (prototype - mar 21,  1995 - 2meg).bin",	0x40000, 0x18cf6145, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep24)
STD_ROM_FN(gg_sprtrvcep24)

struct BurnDriver BurnDrvgg_sprtrvcep24 = {
	"gg_sprtrvcep24", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950321-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep24RomInfo, gg_sprtrvcep24RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950323-2MEG)

static struct BurnRomInfo gg_sprtrvcep23RomDesc[] = {
	{ "sports trivia (prototype - mar 23,  1995 - 2meg).bin",	0x40000, 0x27816c09, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep23)
STD_ROM_FN(gg_sprtrvcep23)

struct BurnDriver BurnDrvgg_sprtrvcep23 = {
	"gg_sprtrvcep23", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950323-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep23RomInfo, gg_sprtrvcep23RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950323-ALGOTEST)

static struct BurnRomInfo gg_sprtrvcep22RomDesc[] = {
	{ "sports trivia (prototype - mar 23,  1995 - algotest).bin",	0x40000, 0x9a8ad5ca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep22)
STD_ROM_FN(gg_sprtrvcep22)

struct BurnDriver BurnDrvgg_sprtrvcep22 = {
	"gg_sprtrvcep22", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950323-ALGOTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep22RomInfo, gg_sprtrvcep22RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950323-QTEST)

static struct BurnRomInfo gg_sprtrvcep21RomDesc[] = {
	{ "sports trivia (prototype - mar 23,  1995 - qtest).bin",	0x40000, 0xacb7e580, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep21)
STD_ROM_FN(gg_sprtrvcep21)

struct BurnDriver BurnDrvgg_sprtrvcep21 = {
	"gg_sprtrvcep21", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950323-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep21RomInfo, gg_sprtrvcep21RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950327-2MEG)

static struct BurnRomInfo gg_sprtrvcep20RomDesc[] = {
	{ "sports trivia (prototype - mar 27,  1995 - 2meg).bin",	0x40000, 0x517bd817, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep20)
STD_ROM_FN(gg_sprtrvcep20)

struct BurnDriver BurnDrvgg_sprtrvcep20 = {
	"gg_sprtrvcep20", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950327-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep20RomInfo, gg_sprtrvcep20RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950327-ALGOTEST)

static struct BurnRomInfo gg_sprtrvcep19RomDesc[] = {
	{ "sports trivia (prototype - mar 27,  1995 - algotest).bin",	0x40000, 0x511b1d24, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep19)
STD_ROM_FN(gg_sprtrvcep19)

struct BurnDriver BurnDrvgg_sprtrvcep19 = {
	"gg_sprtrvcep19", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950327-ALGOTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep19RomInfo, gg_sprtrvcep19RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950329-2MEG)

static struct BurnRomInfo gg_sprtrvcep18RomDesc[] = {
	{ "sports trivia (prototype - mar 29,  1995 - 2meg).bin",	0x40000, 0xe906fbd8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep18)
STD_ROM_FN(gg_sprtrvcep18)

struct BurnDriver BurnDrvgg_sprtrvcep18 = {
	"gg_sprtrvcep18", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950329-2MEG)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep18RomInfo, gg_sprtrvcep18RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950329-ALGOTEST)

static struct BurnRomInfo gg_sprtrvcep17RomDesc[] = {
	{ "sports trivia (prototype - mar 29,  1995 - algotest).bin",	0x40000, 0xe80459b3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep17)
STD_ROM_FN(gg_sprtrvcep17)

struct BurnDriver BurnDrvgg_sprtrvcep17 = {
	"gg_sprtrvcep17", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950329-ALGOTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep17RomInfo, gg_sprtrvcep17RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950329-QTEST)

static struct BurnRomInfo gg_sprtrvcep16RomDesc[] = {
	{ "sports trivia (prototype - mar 29,  1995 - qtest).bin",	0x40000, 0x05047357, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep16)
STD_ROM_FN(gg_sprtrvcep16)

struct BurnDriver BurnDrvgg_sprtrvcep16 = {
	"gg_sprtrvcep16", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950329-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep16RomInfo, gg_sprtrvcep16RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950330-2MEG-D)

static struct BurnRomInfo gg_sprtrvcep15RomDesc[] = {
	{ "sports trivia (prototype - mar 30,  1995 - 2meg - d).bin",	0x40000, 0x302b9b36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep15)
STD_ROM_FN(gg_sprtrvcep15)

struct BurnDriver BurnDrvgg_sprtrvcep15 = {
	"gg_sprtrvcep15", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950330-2MEG-D)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep15RomInfo, gg_sprtrvcep15RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sports Trivia (Prototype, 19950330-QTEST)

static struct BurnRomInfo gg_sprtrvcep14RomDesc[] = {
	{ "sports trivia (prototype - mar 30,  1995 - qtest).bin",	0x40000, 0xf3f9a87b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sprtrvcep14)
STD_ROM_FN(gg_sprtrvcep14)

struct BurnDriver BurnDrvgg_sprtrvcep14 = {
	"gg_sprtrvcep14", "gg_sprtrvce", NULL, NULL, "1995",
	"Sports Trivia (Prototype, 19950330-QTEST)\0", NULL, "Sega?", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sprtrvcep14RomInfo, gg_sprtrvcep14RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Star Trek - The Next Generation (USA)

static struct BurnRomInfo gg_stngRomDesc[] = {
	{ "star trek - the next generation - the advanced holodeck tutorial (usa).bin",	0x40000, 0x80156323, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_stng)
STD_ROM_FN(gg_stng)

struct BurnDriver BurnDrvgg_stng = {
	"gg_stng", NULL, NULL, NULL, "1994",
	"Star Trek - The Next Generation (USA)\0", NULL, "Absolute Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_stngRomInfo, gg_stngRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Star Trek Generations - Beyond the Nexus (Euro, USA)

static struct BurnRomInfo gg_stgbeynRomDesc[] = {
	{ "star trek generations - beyond the nexus (usa, europe).bin",	0x40000, 0x087fc5f0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_stgbeyn)
STD_ROM_FN(gg_stgbeyn)

struct BurnDriver BurnDrvgg_stgbeyn = {
	"gg_stgbeyn", NULL, NULL, NULL, "1994",
	"Star Trek Generations - Beyond the Nexus (Euro, USA)\0", NULL, "Absolute Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_stgbeynRomInfo, gg_stgbeynRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Star Wars (Euro)

static struct BurnRomInfo gg_starwarsRomDesc[] = {
	{ "star wars (europe).bin",	0x80000, 0xdb9bc599, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_starwars)
STD_ROM_FN(gg_starwars)

struct BurnDriver BurnDrvgg_starwars = {
	"gg_starwars", NULL, NULL, NULL, "1993",
	"Star Wars (Euro)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_starwarsRomInfo, gg_starwarsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Star Wars (USA)

static struct BurnRomInfo gg_starwarsuRomDesc[] = {
	{ "star wars (usa).bin",	0x80000, 0x0228769c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_starwarsu)
STD_ROM_FN(gg_starwarsu)

struct BurnDriver BurnDrvgg_starwarsu = {
	"gg_starwarsu", "gg_starwars", NULL, NULL, "1993",
	"Star Wars (USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_starwarsuRomInfo, gg_starwarsuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Stargate (World)

static struct BurnRomInfo gg_stargateRomDesc[] = {
	{ "stargate (world).bin",	0x40000, 0xfc7c64e4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_stargate)
STD_ROM_FN(gg_stargate)

struct BurnDriver BurnDrvgg_stargate = {
	"gg_stargate", NULL, NULL, NULL, "1994",
	"Stargate (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_stargateRomInfo, gg_stargateRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Street Hero (USA, Prototype, SMS Mode)

static struct BurnRomInfo gg_sheroRomDesc[] = {
	{ "street hero [proto 0] (us).bin",	0x80000, 0x9fa727a0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_shero)
STD_ROM_FN(gg_shero)

struct BurnDriver BurnDrvgg_shero = {
	"gg_shero", NULL, NULL, NULL, "1993",
	"Street Hero (USA, Prototype, SMS Mode)\0", NULL, "Innovation Tech", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_sheroRomInfo, gg_sheroRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Street Hero (USA, Prototype Alt, SMS Mode)

static struct BurnRomInfo gg_sheroaRomDesc[] = {
	{ "street hero [proto 1] (us).bin",	0x80000, 0xfb481971, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sheroa)
STD_ROM_FN(gg_sheroa)

struct BurnDriver BurnDrvgg_sheroa = {
	"gg_sheroa", "gg_shero", NULL, NULL, "1993",
	"Street Hero (USA, Prototype Alt, SMS Mode)\0", NULL, "Innovation Tech", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_MAPPER_CODIES | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_sheroaRomInfo, gg_sheroaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Streets of Rage (World) ~ Bare Knuckle (Jpn)

static struct BurnRomInfo gg_sorRomDesc[] = {
	{ "streets of rage (world).bin",	0x40000, 0x3d8bcf1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sor)
STD_ROM_FN(gg_sor)

struct BurnDriver BurnDrvgg_sor = {
	"gg_sor", NULL, NULL, NULL, "1992",
	"Streets of Rage (World) ~ Bare Knuckle (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sorRomInfo, gg_sorRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Streets of Rage II (World) ~ Bare Knuckle II - Shitou e no Chingonka (Jpn)

static struct BurnRomInfo gg_sor2RomDesc[] = {
	{ "mpr-15665.ic1",	0x80000, 0x0b618409, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sor2)
STD_ROM_FN(gg_sor2)

struct BurnDriver BurnDrvgg_sor2 = {
	"gg_sor2", NULL, NULL, NULL, "1993",
	"Streets of Rage II (World) ~ Bare Knuckle II - Shitou e no Chingonka (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sor2RomInfo, gg_sor2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Striker (Euro)

static struct BurnRomInfo gg_strikerRomDesc[] = {
	{ "striker (europe) (en,fr,de,es,it).bin",	0x80000, 0xb421c057, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_striker)
STD_ROM_FN(gg_striker)

struct BurnDriver BurnDrvgg_striker = {
	"gg_striker", NULL, NULL, NULL, "1995",
	"Striker (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_strikerRomInfo, gg_strikerRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Battletank (USA)

static struct BurnRomInfo gg_sbtankRomDesc[] = {
	{ "super battletank (usa).bin",	0x40000, 0x73d6745a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sbtank)
STD_ROM_FN(gg_sbtank)

struct BurnDriver BurnDrvgg_sbtank = {
	"gg_sbtank", NULL, NULL, NULL, "1992",
	"Super Battletank (USA)\0", NULL, "Majesco", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sbtankRomInfo, gg_sbtankRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Jpn)

static struct BurnRomInfo gg_supercoljRomDesc[] = {
	{ "mpr-17865.ic1",	0x20000, 0x2a100717, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolj)
STD_ROM_FN(gg_supercolj)

struct BurnDriver BurnDrvgg_supercolj = {
	"gg_supercolj", "gg_supercol", NULL, NULL, "1995",
	"Super Columns (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercoljRomInfo, gg_supercoljRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Euro, USA)

static struct BurnRomInfo gg_supercolRomDesc[] = {
	{ "super columns (usa, europe).bin",	0x20000, 0x8ba43af3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercol)
STD_ROM_FN(gg_supercol)

struct BurnDriver BurnDrvgg_supercol = {
	"gg_supercol", NULL, NULL, NULL, "1995",
	"Super Columns (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolRomInfo, gg_supercolRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Prototype, 19941201)

static struct BurnRomInfo gg_supercolp6RomDesc[] = {
	{ "super columns (prototype - dec 01,  1994).bin",	0x20000, 0x56cf1d4f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolp6)
STD_ROM_FN(gg_supercolp6)

struct BurnDriver BurnDrvgg_supercolp6 = {
	"gg_supercolp6", "gg_supercol", NULL, NULL, "1994",
	"Super Columns (Prototype, 19941201)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolp6RomInfo, gg_supercolp6RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Prototype, 19941215)

static struct BurnRomInfo gg_supercolp5RomDesc[] = {
	{ "super columns (prototype - dec 15,  1994).bin",	0x20000, 0xabe92434, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolp5)
STD_ROM_FN(gg_supercolp5)

struct BurnDriver BurnDrvgg_supercolp5 = {
	"gg_supercolp5", "gg_supercol", NULL, NULL, "1994",
	"Super Columns (Prototype, 19941215)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolp5RomInfo, gg_supercolp5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Prototype, 19941221)

static struct BurnRomInfo gg_supercolp4RomDesc[] = {
	{ "super columns (prototype - dec 21,  1994).bin",	0x20000, 0x81451fc0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolp4)
STD_ROM_FN(gg_supercolp4)

struct BurnDriver BurnDrvgg_supercolp4 = {
	"gg_supercolp4", "gg_supercol", NULL, NULL, "1994",
	"Super Columns (Prototype, 19941221)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolp4RomInfo, gg_supercolp4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Prototype, 19941226)

static struct BurnRomInfo gg_supercolp3RomDesc[] = {
	{ "super columns (prototype - dec 26,  1994).bin",	0x20000, 0x5c4c633d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolp3)
STD_ROM_FN(gg_supercolp3)

struct BurnDriver BurnDrvgg_supercolp3 = {
	"gg_supercolp3", "gg_supercol", NULL, NULL, "1994",
	"Super Columns (Prototype, 19941226)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolp3RomInfo, gg_supercolp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Prototype, 19941228)

static struct BurnRomInfo gg_supercolp2RomDesc[] = {
	{ "super columns (prototype - dec 28,  1994).bin",	0x20000, 0x3d931952, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolp2)
STD_ROM_FN(gg_supercolp2)

struct BurnDriver BurnDrvgg_supercolp2 = {
	"gg_supercolp2", "gg_supercol", NULL, NULL, "1994",
	"Super Columns (Prototype, 19941228)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolp2RomInfo, gg_supercolp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Prototype, 19950106)

static struct BurnRomInfo gg_supercolp1RomDesc[] = {
	{ "super columns (prototype - jan 06,  1995).bin",	0x20000, 0x096f4ff5, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolp1)
STD_ROM_FN(gg_supercolp1)

struct BurnDriver BurnDrvgg_supercolp1 = {
	"gg_supercolp1", "gg_supercol", NULL, NULL, "1995",
	"Super Columns (Prototype, 19950106)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolp1RomInfo, gg_supercolp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Columns (Prototype, 19941111)

static struct BurnRomInfo gg_supercolp7RomDesc[] = {
	{ "super columns (prototype - nov 11,  1994).bin",	0x20000, 0xf28f89d1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supercolp7)
STD_ROM_FN(gg_supercolp7)

struct BurnDriver BurnDrvgg_supercolp7 = {
	"gg_supercolp7", "gg_supercol", NULL, NULL, "1994",
	"Super Columns (Prototype, 19941111)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supercolp7RomInfo, gg_supercolp7RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Golf (USA)

static struct BurnRomInfo gg_supgolfRomDesc[] = {
	{ "super golf (usa).bin",	0x20000, 0xe9570f36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supgolf)
STD_ROM_FN(gg_supgolf)

struct BurnDriver BurnDrvgg_supgolf = {
	"gg_supgolf", NULL, NULL, NULL, "1991",
	"Super Golf (USA)\0", NULL, "Sage's Creations", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supgolfRomInfo, gg_supgolfRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Golf (Jpn)

static struct BurnRomInfo gg_supgolfjRomDesc[] = {
	{ "super golf (japan).bin",	0x20000, 0x528cbbce, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supgolfj)
STD_ROM_FN(gg_supgolfj)

struct BurnDriver BurnDrvgg_supgolfj = {
	"gg_supgolfj", "gg_supgolf", NULL, NULL, "1991",
	"Super Golf (Jpn)\0", NULL, "Sigma Enterprises", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supgolfjRomInfo, gg_supgolfjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Kick Off (Euro, SMS Mode)

static struct BurnRomInfo gg_skickoffRomDesc[] = {
	{ "super kick off (euro).bin",	0x40000, 0x10dbbef4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_skickoff)
STD_ROM_FN(gg_skickoff)

struct BurnDriver BurnDrvgg_skickoff = {
	"gg_skickoff", NULL, NULL, NULL, "1991",
	"Super Kick Off (Euro, SMS Mode)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_skickoffRomInfo, gg_skickoffRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Momotarou Dentetsu III (Jpn)

static struct BurnRomInfo gg_smomo3RomDesc[] = {
	{ "mpr-18440.ic1",	0x80000, 0xb731bb80, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smomo3)
STD_ROM_FN(gg_smomo3)

struct BurnDriver BurnDrvgg_smomo3 = {
	"gg_smomo3", NULL, NULL, NULL, "1995",
	"Super Momotarou Dentetsu III (Jpn)\0", NULL, "Hudson", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smomo3RomInfo, gg_smomo3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Monaco GP (Jpn)

static struct BurnRomInfo gg_smgpjRomDesc[] = {
	{ "super monaco gp (japan).bin",	0x20000, 0x4f686c4a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smgpj)
STD_ROM_FN(gg_smgpj)

struct BurnDriver BurnDrvgg_smgpj = {
	"gg_smgpj", "gg_smgp", NULL, NULL, "1990",
	"Super Monaco GP (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smgpjRomInfo, gg_smgpjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Monaco GP (Euro, USA, Bra)

static struct BurnRomInfo gg_smgpRomDesc[] = {
	{ "mpr-13771.ic1",	0x20000, 0xfcf12547, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smgp)
STD_ROM_FN(gg_smgp)

struct BurnDriver BurnDrvgg_smgp = {
	"gg_smgp", NULL, NULL, NULL, "1991",
	"Super Monaco GP (Euro, USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smgpRomInfo, gg_smgpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Off Road (Euro, USA)

static struct BurnRomInfo gg_superoffRomDesc[] = {
	{ "super off road (usa, europe).bin",	0x20000, 0xe8b42b1f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_superoff)
STD_ROM_FN(gg_superoff)

struct BurnDriver BurnDrvgg_superoff = {
	"gg_superoff", NULL, NULL, NULL, "1992",
	"Super Off Road (Euro, USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_superoffRomInfo, gg_superoffRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Return of the Jedi (Euro, USA)

static struct BurnRomInfo gg_suprjediRomDesc[] = {
	{ "super return of the jedi (usa, europe).bin",	0x80000, 0x4a38b6b6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_suprjedi)
STD_ROM_FN(gg_suprjedi)

struct BurnDriver BurnDrvgg_suprjedi = {
	"gg_suprjedi", NULL, NULL, NULL, "1995",
	"Super Return of the Jedi (Euro, USA)\0", NULL, "Black Pearl", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_suprjediRomInfo, gg_suprjediRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Smash T.V. (World)

static struct BurnRomInfo gg_smashtvRomDesc[] = {
	{ "super smash t.v. (world).bin",	0x40000, 0x1006e4e3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_smashtv)
STD_ROM_FN(gg_smashtv)

struct BurnDriver BurnDrvgg_smashtv = {
	"gg_smashtv", NULL, NULL, NULL, "1992",
	"Smash T.V. (World)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_smashtvRomInfo, gg_smashtvRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Space Invaders (Euro, USA)

static struct BurnRomInfo gg_ssinvRomDesc[] = {
	{ "super space invaders (usa, europe).bin",	0x40000, 0xdfe38e24, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ssinv)
STD_ROM_FN(gg_ssinv)

struct BurnDriver BurnDrvgg_ssinv = {
	"gg_ssinv", NULL, NULL, NULL, "1992",
	"Super Space Invaders (Euro, USA)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ssinvRomInfo, gg_ssinvRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Super Tetris (Kor, SMS Mode?)

static struct BurnRomInfo gg_stetrisRomDesc[] = {
	{ "super tetris (k) [s][!].bin",	0x10000, 0xbd1cc7df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_stetris)
STD_ROM_FN(gg_stetris)

struct BurnDriver BurnDrvgg_stetris = {
	"gg_stetris", NULL, NULL, NULL, "199?",
	"Super Tetris (Kor, SMS Mode?)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE | HARDWARE_SMS_JAPANESE, GBF_MISC, 0,
	GGGetZipName, gg_stetrisRomInfo, gg_stetrisRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Superman - The Man of Steel (Euro)

static struct BurnRomInfo gg_supermanRomDesc[] = {
	{ "mpr-15504-f.ic1",	0x40000, 0x73df5a15, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_superman)
STD_ROM_FN(gg_superman)

struct BurnDriver BurnDrvgg_superman = {
	"gg_superman", NULL, NULL, NULL, "1993",
	"Superman - The Man of Steel (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supermanRomInfo, gg_supermanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Superman - The Man of Steel (Euro, Prototype)

static struct BurnRomInfo gg_supermanpRomDesc[] = {
	{ "superman - the man of steel (proto).bin",	0x40000, 0xaa3f2172, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_supermanp)
STD_ROM_FN(gg_supermanp)

struct BurnDriver BurnDrvgg_supermanp = {
	"gg_supermanp", "gg_superman", NULL, NULL, "1993",
	"Superman - The Man of Steel (Euro, Prototype)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_supermanpRomInfo, gg_supermanpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Surf Ninjas (USA, Bra)

static struct BurnRomInfo gg_surfninjRomDesc[] = {
	{ "surf ninjas (usa).bin",	0x80000, 0x284482a8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_surfninj)
STD_ROM_FN(gg_surfninj)

struct BurnDriver BurnDrvgg_surfninj = {
	"gg_surfninj", NULL, NULL, NULL, "1993",
	"Surf Ninjas (USA, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_surfninjRomInfo, gg_surfninjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Sylvan Tale (Jpn)

static struct BurnRomInfo gg_sylvanRomDesc[] = {
	{ "sylvan tale (japan).bin",	0x80000, 0x45ef2062, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_sylvan)
STD_ROM_FN(gg_sylvan)

struct BurnDriver BurnDrvgg_sylvan = {
	"gg_sylvan", NULL, NULL, NULL, "1995",
	"Sylvan Tale (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_sylvanRomInfo, gg_sylvanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// T2 - The Arcade Game (Euro, USA)

static struct BurnRomInfo gg_t2agRomDesc[] = {
	{ "t2 - the arcade game (usa, europe).bin",	0x80000, 0x9479c83a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_t2ag)
STD_ROM_FN(gg_t2ag)

struct BurnDriver BurnDrvgg_t2ag = {
	"gg_t2ag", NULL, NULL, NULL, "1993",
	"T2 - The Arcade Game (Euro, USA)\0", NULL, "Arena", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_t2agRomInfo, gg_t2agRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// T2 - The Arcade Game (Jpn)

static struct BurnRomInfo gg_t2agjRomDesc[] = {
	{ "t2 - the arcade game (jp).bin",	0x80000, 0x0b1ba87f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_t2agj)
STD_ROM_FN(gg_t2agj)

struct BurnDriver BurnDrvgg_t2agj = {
	"gg_t2agj", "gg_t2ag", NULL, NULL, "1993",
	"T2 - The Arcade Game (Jpn)\0", NULL, "Acclaim", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_t2agjRomInfo, gg_t2agjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tails Adventures (World)

static struct BurnRomInfo gg_tailsadvRomDesc[] = {
	{ "tails adventures (world) (en,ja).bin",	0x80000, 0x5bb6e5d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tailsadv)
STD_ROM_FN(gg_tailsadv)

struct BurnDriver BurnDrvgg_tailsadv = {
	"gg_tailsadv", NULL, NULL, NULL, "1995",
	"Tails Adventures (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tailsadvRomInfo, gg_tailsadvRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tails no Skypatrol (Jpn)

static struct BurnRomInfo gg_tailskypRomDesc[] = {
	{ "tails no skypatrol (japan).bin",	0x40000, 0x88618afa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tailskyp)
STD_ROM_FN(gg_tailskyp)

struct BurnDriver BurnDrvgg_tailskyp = {
	"gg_tailskyp", NULL, NULL, NULL, "1995",
	"Tails no Skypatrol (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tailskypRomInfo, gg_tailskypRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taisen Mahjong HaoPai (Jpn)

static struct BurnRomInfo gg_taisnmjRomDesc[] = {
	{ "taisen mahjong haopai (japan).bin",	0x20000, 0xcf9c607c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_taisnmj)
STD_ROM_FN(gg_taisnmj)

struct BurnDriver BurnDrvgg_taisnmj = {
	"gg_taisnmj", NULL, NULL, NULL, "1990",
	"Taisen Mahjong HaoPai (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_taisnmjRomInfo, gg_taisnmjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taisen Mahjong HaoPai 2 (Jpn)

static struct BurnRomInfo gg_taisnmj2RomDesc[] = {
	{ "taisen mahjong haopai 2 (japan).bin",	0x80000, 0x20527530, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_taisnmj2)
STD_ROM_FN(gg_taisnmj2)

struct BurnDriver BurnDrvgg_taisnmj2 = {
	"gg_taisnmj2", NULL, NULL, NULL, "1993",
	"Taisen Mahjong HaoPai 2 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_taisnmj2RomInfo, gg_taisnmj2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taisen-gata Daisenryaku G (Jpn)

static struct BurnRomInfo gg_daisenrgRomDesc[] = {
	{ "mpr-14246.ic1",	0x40000, 0x7b7717b8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_daisenrg)
STD_ROM_FN(gg_daisenrg)

struct BurnDriver BurnDrvgg_daisenrg = {
	"gg_daisenrg", NULL, NULL, NULL, "1991",
	"Taisen-gata Daisenryaku G (Jpn)\0", NULL, "SystemSoft", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_daisenrgRomInfo, gg_daisenrgRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taito Chase H.Q. (Jpn, SMS Mode)

static struct BurnRomInfo gg_chasehqjRomDesc[] = {
	{ "taito chase h.q. (japan).bin",	0x20000, 0x7bb81e3d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chasehqj)
STD_ROM_FN(gg_chasehqj)

struct BurnDriver BurnDrvgg_chasehqj = {
	"gg_chasehqj", "gg_chasehq", NULL, NULL, "1991",
	"Taito Chase H.Q. (Jpn, SMS Mode)\0", NULL, "Taito", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE | HARDWARE_SMS_JAPANESE, GBF_MISC, 0,
	GGGetZipName, gg_chasehqjRomInfo, gg_chasehqjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taito Chase H.Q. (USA, SMS Mode)

static struct BurnRomInfo gg_chasehqRomDesc[] = {
	{ "chase h.q. [sms-gg].bin",	0x20000, 0xc8381def, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_chasehq)
STD_ROM_FN(gg_chasehq)

struct BurnDriver BurnDrvgg_chasehq = {
	"gg_chasehq", NULL, NULL, NULL, "1991",
	"Taito Chase H.Q. (USA, SMS Mode)\0", NULL, "Taito", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_chasehqRomInfo, gg_chasehqRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tale Spin (Euro, USA)

static struct BurnRomInfo gg_talespinRomDesc[] = {
	{ "tale spin (usa, europe).bin",	0x40000, 0xf1732ffe, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_talespin)
STD_ROM_FN(gg_talespin)

struct BurnDriver BurnDrvgg_talespin = {
	"gg_talespin", NULL, NULL, NULL, "1993",
	"Tale Spin (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_talespinRomInfo, gg_talespinRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tama and Friends Sanchoume Kouen - Tamalympic (Jpn)

static struct BurnRomInfo gg_tamalympRomDesc[] = {
	{ "tama and friends sanchoume kouen - tamalympic (japan).bin",	0x40000, 0xdd1d2ebf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tamalymp)
STD_ROM_FN(gg_tamalymp)

struct BurnDriver BurnDrvgg_tamalymp = {
	"gg_tamalymp", NULL, NULL, NULL, "1995",
	"Tama and Friends Sanchoume Kouen - Tamalympic (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tamalympRomInfo, gg_tamalympRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tarot no Yakata (Jpn)

static struct BurnRomInfo gg_tarotRomDesc[] = {
	{ "tarot no yakata (japan).bin",	0x20000, 0x57834c03, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tarot)
STD_ROM_FN(gg_tarot)

struct BurnDriver BurnDrvgg_tarot = {
	"gg_tarot", NULL, NULL, NULL, "1991",
	"Tarot no Yakata (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tarotRomInfo, gg_tarotRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tarzan - Lord of the Jungle (Euro)

static struct BurnRomInfo gg_tarzanRomDesc[] = {
	{ "tarzan - lord of the jungle (europe).bin",	0x40000, 0xef3afe8b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tarzan)
STD_ROM_FN(gg_tarzan)

struct BurnDriver BurnDrvgg_tarzan = {
	"gg_tarzan", NULL, NULL, NULL, "1994",
	"Tarzan - Lord of the Jungle (Euro)\0", NULL, "GameTek", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	GGGetZipName, gg_tarzanRomInfo, gg_tarzanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tatakae! Pro Yakyuu Twin League (Jpn)

static struct BurnRomInfo gg_proyaktlRomDesc[] = {
	{ "tatakae! pro yakyuu twin league (japan).bin",	0x80000, 0xbec57602, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_proyaktl)
STD_ROM_FN(gg_proyaktl)

struct BurnDriver BurnDrvgg_proyaktl = {
	"gg_proyaktl", NULL, NULL, NULL, "1995",
	"Tatakae! Pro Yakyuu Twin League (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_proyaktlRomInfo, gg_proyaktlRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Euro, USA)

static struct BurnRomInfo gg_tazmarsRomDesc[] = {
	{ "taz in escape from mars (usa, europe).bin",	0x80000, 0xeebad66b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmars)
STD_ROM_FN(gg_tazmars)

struct BurnDriver BurnDrvgg_tazmars = {
	"gg_tazmars", NULL, NULL, NULL, "1994",
	"Taz in Escape from Mars (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsRomInfo, gg_tazmarsRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940810)

static struct BurnRomInfo gg_tazmarsp13RomDesc[] = {
	{ "taz in escape from mars (prototype - aug 10,  1994).bin",	0x80000, 0x92388016, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp13)
STD_ROM_FN(gg_tazmarsp13)

struct BurnDriver BurnDrvgg_tazmarsp13 = {
	"gg_tazmarsp13", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940810)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp13RomInfo, gg_tazmarsp13RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940812)

static struct BurnRomInfo gg_tazmarsp12RomDesc[] = {
	{ "taz in escape from mars (prototype - aug 12,  1994).bin",	0x80000, 0xa43fdfad, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp12)
STD_ROM_FN(gg_tazmarsp12)

struct BurnDriver BurnDrvgg_tazmarsp12 = {
	"gg_tazmarsp12", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940812)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp12RomInfo, gg_tazmarsp12RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940814)

static struct BurnRomInfo gg_tazmarsp11RomDesc[] = {
	{ "taz in escape from mars (prototype - aug 14,  1994).bin",	0x80000, 0x9267d968, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp11)
STD_ROM_FN(gg_tazmarsp11)

struct BurnDriver BurnDrvgg_tazmarsp11 = {
	"gg_tazmarsp11", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940814)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp11RomInfo, gg_tazmarsp11RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940819)

static struct BurnRomInfo gg_tazmarsp10RomDesc[] = {
	{ "taz in escape from mars (prototype - aug 19,  1994).bin",	0x80000, 0x57747640, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp10)
STD_ROM_FN(gg_tazmarsp10)

struct BurnDriver BurnDrvgg_tazmarsp10 = {
	"gg_tazmarsp10", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940819)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp10RomInfo, gg_tazmarsp10RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940823)

static struct BurnRomInfo gg_tazmarsp09RomDesc[] = {
	{ "taz in escape from mars (prototype - aug 23,  1994).bin",	0x80000, 0x6cd0fd78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp09)
STD_ROM_FN(gg_tazmarsp09)

struct BurnDriver BurnDrvgg_tazmarsp09 = {
	"gg_tazmarsp09", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940823)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp09RomInfo, gg_tazmarsp09RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940829-B)

static struct BurnRomInfo gg_tazmarsp08RomDesc[] = {
	{ "taz in escape from mars (prototype - aug 29,  1994 - b).bin",	0x80000, 0x3c03919d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp08)
STD_ROM_FN(gg_tazmarsp08)

struct BurnDriver BurnDrvgg_tazmarsp08 = {
	"gg_tazmarsp08", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940829-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp08RomInfo, gg_tazmarsp08RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940830)

static struct BurnRomInfo gg_tazmarsp07RomDesc[] = {
	{ "taz in escape from mars (prototype - aug 30,  1994).bin",	0x80000, 0x09679b89, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp07)
STD_ROM_FN(gg_tazmarsp07)

struct BurnDriver BurnDrvgg_tazmarsp07 = {
	"gg_tazmarsp07", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940830)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp07RomInfo, gg_tazmarsp07RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940511)

static struct BurnRomInfo gg_tazmarsp14RomDesc[] = {
	{ "taz in escape from mars (prototype - may 11,  1994).bin",	0x40000, 0x030f396a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp14)
STD_ROM_FN(gg_tazmarsp14)

struct BurnDriver BurnDrvgg_tazmarsp14 = {
	"gg_tazmarsp14", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940511)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp14RomInfo, gg_tazmarsp14RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940902)

static struct BurnRomInfo gg_tazmarsp06RomDesc[] = {
	{ "taz in escape from mars (prototype - sep 02,  1994).bin",	0x80000, 0x18913d93, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp06)
STD_ROM_FN(gg_tazmarsp06)

struct BurnDriver BurnDrvgg_tazmarsp06 = {
	"gg_tazmarsp06", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940902)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp06RomInfo, gg_tazmarsp06RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940905-B)

static struct BurnRomInfo gg_tazmarsp05RomDesc[] = {
	{ "taz in escape from mars (prototype - sep 05,  1994 - b).bin",	0x80000, 0xd84a631c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp05)
STD_ROM_FN(gg_tazmarsp05)

struct BurnDriver BurnDrvgg_tazmarsp05 = {
	"gg_tazmarsp05", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940905-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp05RomInfo, gg_tazmarsp05RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940905-C)

static struct BurnRomInfo gg_tazmarsp04RomDesc[] = {
	{ "taz in escape from mars (prototype - sep 05,  1994 - c).bin",	0x80000, 0x61330a58, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp04)
STD_ROM_FN(gg_tazmarsp04)

struct BurnDriver BurnDrvgg_tazmarsp04 = {
	"gg_tazmarsp04", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940905-C)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp04RomInfo, gg_tazmarsp04RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940906-D)

static struct BurnRomInfo gg_tazmarsp03RomDesc[] = {
	{ "taz in escape from mars (prototype - sep 06,  1994 - d).bin",	0x80000, 0x1f040bca, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp03)
STD_ROM_FN(gg_tazmarsp03)

struct BurnDriver BurnDrvgg_tazmarsp03 = {
	"gg_tazmarsp03", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940906-D)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp03RomInfo, gg_tazmarsp03RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940909-B)

static struct BurnRomInfo gg_tazmarsp02RomDesc[] = {
	{ "taz in escape from mars (prototype - sep 09,  1994 - b).bin",	0x80000, 0xc867da05, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp02)
STD_ROM_FN(gg_tazmarsp02)

struct BurnDriver BurnDrvgg_tazmarsp02 = {
	"gg_tazmarsp02", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940909-B)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp02RomInfo, gg_tazmarsp02RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz in Escape from Mars (Prototype, 19940910)

static struct BurnRomInfo gg_tazmarsp01RomDesc[] = {
	{ "taz in escape from mars (prototype - sep 10,  1994).bin",	0x80000, 0xfaf4bec0, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmarsp01)
STD_ROM_FN(gg_tazmarsp01)

struct BurnDriver BurnDrvgg_tazmarsp01 = {
	"gg_tazmarsp01", "gg_tazmars", NULL, NULL, "1994",
	"Taz in Escape from Mars (Prototype, 19940910)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmarsp01RomInfo, gg_tazmarsp01RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Taz-Mania - The Search for the Lost Seabirds (Euro, USA)

static struct BurnRomInfo gg_tazmaniaRomDesc[] = {
	{ "epoxy.u1",	0x40000, 0x36040c24, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tazmania)
STD_ROM_FN(gg_tazmania)

struct BurnDriver BurnDrvgg_tazmania = {
	"gg_tazmania", NULL, NULL, NULL, "1992",
	"Taz-Mania - The Search for the Lost Seabirds (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tazmaniaRomInfo, gg_tazmaniaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (World)

static struct BurnRomInfo gg_tempojrRomDesc[] = {
	{ "mpr-17792-s.ic1",	0x80000, 0xde466796, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojr)
STD_ROM_FN(gg_tempojr)

struct BurnDriver BurnDrvgg_tempojr = {
	"gg_tempojr", NULL, NULL, NULL, "1995",
	"Tempo Jr. (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrRomInfo, gg_tempojrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19941205)

static struct BurnRomInfo gg_tempojrp8RomDesc[] = {
	{ "tempo jr. (prototype - dec 05,  1994).bin",	0x80000, 0x3475d48e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp8)
STD_ROM_FN(gg_tempojrp8)

struct BurnDriver BurnDrvgg_tempojrp8 = {
	"gg_tempojrp8", "gg_tempojr", NULL, NULL, "1994",
	"Tempo Jr. (Prototype, 19941205)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp8RomInfo, gg_tempojrp8RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19941212)

static struct BurnRomInfo gg_tempojrp7RomDesc[] = {
	{ "tempo jr. (prototype - dec 12,  1994).bin",	0x80000, 0xfb9a5885, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp7)
STD_ROM_FN(gg_tempojrp7)

struct BurnDriver BurnDrvgg_tempojrp7 = {
	"gg_tempojrp7", "gg_tempojr", NULL, NULL, "1994",
	"Tempo Jr. (Prototype, 19941212)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp7RomInfo, gg_tempojrp7RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19941226)

static struct BurnRomInfo gg_tempojrp6RomDesc[] = {
	{ "tempo jr. (prototype - dec 26,  1994).bin",	0x80000, 0xe484e41c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp6)
STD_ROM_FN(gg_tempojrp6)

struct BurnDriver BurnDrvgg_tempojrp6 = {
	"gg_tempojrp6", "gg_tempojr", NULL, NULL, "1994",
	"Tempo Jr. (Prototype, 19941226)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp6RomInfo, gg_tempojrp6RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19950206)

static struct BurnRomInfo gg_tempojrp2RomDesc[] = {
	{ "tempo jr. (prototype - feb 06,  1995).bin",	0x80000, 0x3ef6551c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp2)
STD_ROM_FN(gg_tempojrp2)

struct BurnDriver BurnDrvgg_tempojrp2 = {
	"gg_tempojrp2", "gg_tempojr", NULL, NULL, "1995",
	"Tempo Jr. (Prototype, 19950206)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp2RomInfo, gg_tempojrp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19950215)

static struct BurnRomInfo gg_tempojrp1RomDesc[] = {
	{ "tempo jr. (prototype - feb 15,  1995).bin",	0x80000, 0x79e83d67, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp1)
STD_ROM_FN(gg_tempojrp1)

struct BurnDriver BurnDrvgg_tempojrp1 = {
	"gg_tempojrp1", "gg_tempojr", NULL, NULL, "1995",
	"Tempo Jr. (Prototype, 19950215)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp1RomInfo, gg_tempojrp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19950117)

static struct BurnRomInfo gg_tempojrp5RomDesc[] = {
	{ "tempo jr. (prototype - jan 17,  1995).bin",	0x80000, 0xab6dc88c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp5)
STD_ROM_FN(gg_tempojrp5)

struct BurnDriver BurnDrvgg_tempojrp5 = {
	"gg_tempojrp5", "gg_tempojr", NULL, NULL, "1995",
	"Tempo Jr. (Prototype, 19950117)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp5RomInfo, gg_tempojrp5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19950120)

static struct BurnRomInfo gg_tempojrp4RomDesc[] = {
	{ "tempo jr. (prototype - jan 20,  1995).bin",	0x80000, 0x96d64b16, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp4)
STD_ROM_FN(gg_tempojrp4)

struct BurnDriver BurnDrvgg_tempojrp4 = {
	"gg_tempojrp4", "gg_tempojr", NULL, NULL, "1995",
	"Tempo Jr. (Prototype, 19950120)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp4RomInfo, gg_tempojrp4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19950131)

static struct BurnRomInfo gg_tempojrp3RomDesc[] = {
	{ "tempo jr. (prototype - jan 31,  1995).bin",	0x80000, 0x7afedf36, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp3)
STD_ROM_FN(gg_tempojrp3)

struct BurnDriver BurnDrvgg_tempojrp3 = {
	"gg_tempojrp3", "gg_tempojr", NULL, NULL, "1995",
	"Tempo Jr. (Prototype, 19950131)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp3RomInfo, gg_tempojrp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tempo Jr. (Prototype, 19941128)

static struct BurnRomInfo gg_tempojrp9RomDesc[] = {
	{ "tempo jr. (prototype - nov 28,  1994).bin",	0x80000, 0xa33b5b92, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tempojrp9)
STD_ROM_FN(gg_tempojrp9)

struct BurnDriver BurnDrvgg_tempojrp9 = {
	"gg_tempojrp9", "gg_tempojr", NULL, NULL, "1994",
	"Tempo Jr. (Prototype, 19941128)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tempojrp9RomInfo, gg_tempojrp9RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tengen World Cup Soccer (Euro, USA)

static struct BurnRomInfo gg_tengenwcRomDesc[] = {
	{ "mpr-15642-f.ic1",	0x40000, 0xdd6d2e34, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tengenwc)
STD_ROM_FN(gg_tengenwc)

struct BurnDriver BurnDrvgg_tengenwc = {
	"gg_tengenwc", NULL, NULL, NULL, "1993",
	"Tengen World Cup Soccer (Euro, USA)\0", NULL, "Tengen", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tengenwcRomInfo, gg_tengenwcRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Terminator 2 - Judgment Day (Euro, USA)

static struct BurnRomInfo gg_term2RomDesc[] = {
	{ "terminator 2 - judgment day (usa, europe).bin",	0x40000, 0x1bd15773, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_term2)
STD_ROM_FN(gg_term2)

struct BurnDriver BurnDrvgg_term2 = {
	"gg_term2", NULL, NULL, NULL, "1993",
	"Terminator 2 - Judgment Day (Euro, USA)\0", "Glitchy graphics, use the SMS version, instead!", "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_term2RomInfo, gg_term2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// The Terminator (Euro, USA)

static struct BurnRomInfo gg_termntrRomDesc[] = {
	{ "terminator, the (usa, europe).bin",	0x40000, 0xc029a5fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_termntr)
STD_ROM_FN(gg_termntr)

struct BurnDriver BurnDrvgg_termntr = {
	"gg_termntr", NULL, NULL, NULL, "1992",
	"The Terminator (Euro, USA)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_termntrRomInfo, gg_termntrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tesserae (Euro, USA)

static struct BurnRomInfo gg_tesseraeRomDesc[] = {
	{ "tesserae (usa, europe).bin",	0x20000, 0xca0e11cc, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tesserae)
STD_ROM_FN(gg_tesserae)

struct BurnDriver BurnDrvgg_tesserae = {
	"gg_tesserae", NULL, NULL, NULL, "1993",
	"Tesserae (Euro, USA)\0", NULL, "GameTek", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tesseraeRomInfo, gg_tesseraeRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tintin in Tibet (Euro)

static struct BurnRomInfo gg_tintinRomDesc[] = {
	{ "tintin in tibet (europe) (en,fr,de,es,nl,sv).bin",	0x80000, 0x969cf389, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tintin)
STD_ROM_FN(gg_tintin)

struct BurnDriver BurnDrvgg_tintin = {
	"gg_tintin", NULL, NULL, NULL, "1996",
	"Tintin in Tibet (Euro)\0", NULL, "Infogrames", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tintinRomInfo, gg_tintinRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tom and Jerry - The Movie (Jpn)

static struct BurnRomInfo gg_tomjermvjRomDesc[] = {
	{ "tom and jerry - the movie (jp).bin",	0x40000, 0xa1453efa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tomjermvj)
STD_ROM_FN(gg_tomjermvj)

struct BurnDriver BurnDrvgg_tomjermvj = {
	"gg_tomjermvj", "gg_tomjermv", NULL, NULL, "1993",
	"Tom and Jerry - The Movie (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tomjermvjRomInfo, gg_tomjermvjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Tom and Jerry - The Movie (USA)

static struct BurnRomInfo gg_tomjermvRomDesc[] = {
	{ "tom and jerry - the movie (usa).bin",	0x40000, 0x5cd33ff2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_tomjermv)
STD_ROM_FN(gg_tomjermv)

struct BurnDriver BurnDrvgg_tomjermv = {
	"gg_tomjermv", NULL, NULL, NULL, "1993",
	"Tom and Jerry - The Movie (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_tomjermvRomInfo, gg_tomjermvRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Torarete Tamaru ka!? (Jpn)

static struct BurnRomInfo gg_torareteRomDesc[] = {
	{ "mpr-16707.ic1",	0x80000, 0x5bcf9b97, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_torarete)
STD_ROM_FN(gg_torarete)

struct BurnDriver BurnDrvgg_torarete = {
	"gg_torarete", NULL, NULL, NULL, "1994",
	"Torarete Tamaru ka!? (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_torareteRomInfo, gg_torareteRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// True Lies (World)

static struct BurnRomInfo gg_trueliesRomDesc[] = {
	{ "true lies (world).bin",	0x80000, 0x5173b02a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_truelies)
STD_ROM_FN(gg_truelies)

struct BurnDriver BurnDrvgg_truelies = {
	"gg_truelies", NULL, NULL, NULL, "1995",
	"True Lies (World)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_trueliesRomInfo, gg_trueliesRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Ultimate Soccer (Euro, Jpn, Bra)

static struct BurnRomInfo gg_ultsoccrRomDesc[] = {
	{ "mpr-15827.ic1",	0x40000, 0x820965a3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ultsoccr)
STD_ROM_FN(gg_ultsoccr)

struct BurnDriver BurnDrvgg_ultsoccr = {
	"gg_ultsoccr", NULL, NULL, NULL, "1993",
	"Ultimate Soccer (Euro, Jpn, Bra)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ultsoccrRomInfo, gg_ultsoccrRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Urban Strike (Euro, USA)

static struct BurnRomInfo gg_ustrikeRomDesc[] = {
	{ "urban strike (usa, europe).bin",	0x80000, 0x185e9fc1, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_ustrike)
STD_ROM_FN(gg_ustrike)

struct BurnDriver BurnDrvgg_ustrike = {
	"gg_ustrike", NULL, NULL, NULL, "1995",
	"Urban Strike (Euro, USA)\0", NULL, "Black Pearl", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_ustrikeRomInfo, gg_ustrikeRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Vampire - Master of Darkness (USA)

static struct BurnRomInfo gg_vampireRomDesc[] = {
	{ "vampire - master of darkness (usa).bin",	0x40000, 0x7ec64025, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_vampire)
STD_ROM_FN(gg_vampire)

struct BurnDriver BurnDrvgg_vampire = {
	"gg_vampire", "gg_mastdark", NULL, NULL, "1993",
	"Vampire - Master of Darkness (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_vampireRomInfo, gg_vampireRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Virtua Fighter Animation (Euro, USA)

static struct BurnRomInfo gg_vfaRomDesc[] = {
	{ "mpr-18864-s.ic2",	0x100000, 0xd431c452, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_vfa)
STD_ROM_FN(gg_vfa)

struct BurnDriver BurnDrvgg_vfa = {
	"gg_vfa", NULL, NULL, NULL, "1996",
	"Virtua Fighter Animation (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_vfaRomInfo, gg_vfaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Virtua Fighter Mini (Jpn)

static struct BurnRomInfo gg_vfminiRomDesc[] = {
	{ "mpr-18679-s.ic1",	0x100000, 0xc05657f8, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_vfmini)
STD_ROM_FN(gg_vfmini)

struct BurnDriver BurnDrvgg_vfmini = {
	"gg_vfmini", "gg_vfa", NULL, NULL, "1996",
	"Virtua Fighter Mini (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_vfminiRomInfo, gg_vfminiRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// VR Troopers (Euro, USA)

static struct BurnRomInfo gg_vrtroopRomDesc[] = {
	{ "vr troopers (usa, europe).bin",	0x80000, 0xb0f22745, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_vrtroop)
STD_ROM_FN(gg_vrtroop)

struct BurnDriver BurnDrvgg_vrtroop = {
	"gg_vrtroop", NULL, NULL, NULL, "1995",
	"VR Troopers (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_vrtroopRomInfo, gg_vrtroopRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wagyan Land (Jpn)

static struct BurnRomInfo gg_wagyanRomDesc[] = {
	{ "mpr-13930.ic1",	0x40000, 0x29e697b2, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wagyan)
STD_ROM_FN(gg_wagyan)

struct BurnDriver BurnDrvgg_wagyan = {
	"gg_wagyan", NULL, NULL, NULL, "1991",
	"Wagyan Land (Jpn)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wagyanRomInfo, gg_wagyanRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wagyan Land (Jpn, Alt)

static struct BurnRomInfo gg_wagyanaRomDesc[] = {
	{ "wagan land (j) [a1].bin",	0x40000, 0xd5369192, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wagyana)
STD_ROM_FN(gg_wagyana)

struct BurnDriver BurnDrvgg_wagyana = {
	"gg_wagyana", "gg_wagyan", NULL, NULL, "1991",
	"Wagyan Land (Jpn, Alt)\0", NULL, "Namcot", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wagyanaRomInfo, gg_wagyanaRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wheel of Fortune (USA)

static struct BurnRomInfo gg_wheelofRomDesc[] = {
	{ "wheel of fortune (usa).bin",	0x20000, 0xe91cdb2a, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wheelof)
STD_ROM_FN(gg_wheelof)

struct BurnDriver BurnDrvgg_wheelof = {
	"gg_wheelof", NULL, NULL, NULL, "1992",
	"Wheel of Fortune (USA)\0", NULL, "GameTek", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wheelofRomInfo, gg_wheelofRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wimbledon (World)

static struct BurnRomInfo gg_wimbledRomDesc[] = {
	{ "mpr-14994.ic1",	0x20000, 0xce1108fd, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wimbled)
STD_ROM_FN(gg_wimbled)

struct BurnDriver BurnDrvgg_wimbled = {
	"gg_wimbled", NULL, NULL, NULL, "1992",
	"Wimbledon (World)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wimbledRomInfo, gg_wimbledRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Winter Olympics - Lillehammer '94 (Jpn)

static struct BurnRomInfo gg_wintoljRomDesc[] = {
	{ "mpr-16350.ic1",	0x80000, 0xd5195a39, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wintolj)
STD_ROM_FN(gg_wintolj)

struct BurnDriver BurnDrvgg_wintolj = {
	"gg_wintolj", "gg_wintol", NULL, NULL, "1994",
	"Winter Olympics - Lillehammer '94 (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wintoljRomInfo, gg_wintoljRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Winter Olympics - Lillehammer '94 (Euro, USA)

static struct BurnRomInfo gg_wintolRomDesc[] = {
	{ "mpr-16024.ic1",	0x80000, 0xd15d335b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wintol)
STD_ROM_FN(gg_wintol)

struct BurnDriver BurnDrvgg_wintol = {
	"gg_wintol", NULL, NULL, NULL, "1993",
	"Winter Olympics - Lillehammer '94 (Euro, USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wintolRomInfo, gg_wintolRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wizard Pinball (Euro)

static struct BurnRomInfo gg_wizardRomDesc[] = {
	{ "wizard pinball (europe).bin",	0x40000, 0x9e03f96c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wizard)
STD_ROM_FN(gg_wizard)

struct BurnDriver BurnDrvgg_wizard = {
	"gg_wizard", NULL, NULL, NULL, "1995",
	"Wizard Pinball (Euro)\0", NULL, "Domark", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wizardRomInfo, gg_wizardRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wolfchild (Euro)

static struct BurnRomInfo gg_wolfchldRomDesc[] = {
	{ "mpr-15717-s.ic1",	0x40000, 0x840a8f8e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wolfchld)
STD_ROM_FN(gg_wolfchld)

struct BurnDriver BurnDrvgg_wolfchld = {
	"gg_wolfchld", NULL, NULL, NULL, "1993",
	"Wolfchild (Euro)\0", NULL, "Virgin Interactive", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wolfchldRomInfo, gg_wolfchldRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy (Euro)

static struct BurnRomInfo gg_wboyRomDesc[] = {
	{ "mpr-13912.ic1",	0x20000, 0xea2dd3a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wboy)
STD_ROM_FN(gg_wboy)

struct BurnDriver BurnDrvgg_wboy = {
	"gg_wboy", NULL, NULL, NULL, "1991",
	"Wonder Boy (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wboyRomInfo, gg_wboyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy (Jpn)

static struct BurnRomInfo gg_wboyjRomDesc[] = {
	{ "wonder boy (japan).bin",	0x20000, 0x9977fcb3, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wboyj)
STD_ROM_FN(gg_wboyj)

struct BurnDriver BurnDrvgg_wboyj = {
	"gg_wboyj", "gg_wboy", NULL, NULL, "1990",
	"Wonder Boy (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wboyjRomInfo, gg_wboyjRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy - The Dragon's Trap (Euro)

static struct BurnRomInfo gg_wboydtrpRomDesc[] = {
	{ "mpr-14768.ic1",	0x40000, 0xa74c97a7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wboydtrp)
STD_ROM_FN(gg_wboydtrp)

struct BurnDriver BurnDrvgg_wboydtrp = {
	"gg_wboydtrp", NULL, NULL, NULL, "1992",
	"Wonder Boy - The Dragon's Trap (Euro)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wboydtrpRomInfo, gg_wboydtrpRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Wonder Boy III - The Dragon's Trap (Euro, Prototype)

static struct BurnRomInfo gg_wboydtrppRomDesc[] = {
	{ "wonder boy iii - the dragon's trap (europe) (beta).bin",	0x40000, 0xdb1b5b44, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wboydtrpp)
STD_ROM_FN(gg_wboydtrpp)

struct BurnDriver BurnDrvgg_wboydtrpp = {
	"gg_wboydtrpp", "gg_wboydtrp", NULL, NULL, "1992",
	"Wonder Boy III - The Dragon's Trap (Euro, Prototype)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wboydtrppRomInfo, gg_wboydtrppRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Woody Pop (Euro, USA, v1.1)

static struct BurnRomInfo gg_woodypopRomDesc[] = {
	{ "mpr-13808.ic1",	0x08000, 0xb74f3a4f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_woodypop)
STD_ROM_FN(gg_woodypop)

struct BurnDriver BurnDrvgg_woodypop = {
	"gg_woodypop", NULL, NULL, NULL, "1991",
	"Woody Pop (Euro, USA, v1.1)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_woodypopRomInfo, gg_woodypopRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Woody Pop (Euro, USA)

static struct BurnRomInfo gg_woodypop1RomDesc[] = {
	{ "woody pop (usa, europe).bin",	0x08000, 0x9c0e5a04, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_woodypop1)
STD_ROM_FN(gg_woodypop1)

struct BurnDriver BurnDrvgg_woodypop1 = {
	"gg_woodypop1", "gg_woodypop", NULL, NULL, "1991",
	"Woody Pop (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_woodypop1RomInfo, gg_woodypop1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Class Leader Board (Euro, USA)

static struct BurnRomInfo gg_wcleadRomDesc[] = {
	{ "mpr-14251.ic1",	0x40000, 0x868fe528, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wclead)
STD_ROM_FN(gg_wclead)

struct BurnDriver BurnDrvgg_wclead = {
	"gg_wclead", NULL, NULL, NULL, "1991",
	"World Class Leader Board (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wcleadRomInfo, gg_wcleadRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Cup USA 94 (Euro, USA)

static struct BurnRomInfo gg_wcup94RomDesc[] = {
	{ "world cup usa 94 (usa, europe) (en,fr,de,es,it,nl,pt,sv).bin",	0x80000, 0xd2bb3690, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wcup94)
STD_ROM_FN(gg_wcup94)

struct BurnDriver BurnDrvgg_wcup94 = {
	"gg_wcup94", NULL, NULL, NULL, "1994",
	"World Cup USA 94 (Euro, USA)\0", NULL, "U.S. Gold", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wcup94RomInfo, gg_wcup94RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// World Derby (Jpn)

static struct BurnRomInfo gg_wderbyRomDesc[] = {
	{ "world derby (japan).bin",	0x80000, 0x1e81861f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wderby)
STD_ROM_FN(gg_wderby)

struct BurnDriver BurnDrvgg_wderby = {
	"gg_wderby", NULL, NULL, NULL, "1994",
	"World Derby (Jpn)\0", NULL, "CRI", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wderbyRomInfo, gg_wderbyRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// WWF Raw (Euro, USA)

static struct BurnRomInfo gg_wwfrawRomDesc[] = {
	{ "wwf raw (usa, europe).bin",	0x80000, 0x8dc68d92, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wwfraw)
STD_ROM_FN(gg_wwfraw)

struct BurnDriver BurnDrvgg_wwfraw = {
	"gg_wwfraw", NULL, NULL, NULL, "1994",
	"WWF Raw (Euro, USA)\0", NULL, "Acclaim Entertainment", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wwfrawRomInfo, gg_wwfrawRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// WWF Wrestlemania Steel Cage Challenge (Euro, SMS Mode)

static struct BurnRomInfo gg_wwfsteelRomDesc[] = {
	{ "wwf wrestlemania steel cage challenge (europe).bin",	0x40000, 0xda8e95a9, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wwfsteel)
STD_ROM_FN(gg_wwfsteel)

struct BurnDriver BurnDrvgg_wwfsteel = {
	"gg_wwfsteel", NULL, NULL, NULL, "1993",
	"WWF Wrestlemania Steel Cage Challenge (Euro, SMS Mode)\0", NULL, "Flying Edge", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR | HARDWARE_SMS_GG_SMS_MODE, GBF_MISC, 0,
	GGGetZipName, gg_wwfsteelRomInfo, gg_wwfsteelRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men (USA)

static struct BurnRomInfo gg_xmenRomDesc[] = {
	{ "x-men (usa).bin",	0x80000, 0x567a5ee6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmen)
STD_ROM_FN(gg_xmen)

struct BurnDriver BurnDrvgg_xmen = {
	"gg_xmen", NULL, NULL, NULL, "1994",
	"X-Men (USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenRomInfo, gg_xmenRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Gamemaster's Legacy (Euro, USA)

static struct BurnRomInfo gg_xmenglRomDesc[] = {
	{ "x-men - gamemaster's legacy (usa, europe).bin",	0x80000, 0xc169c344, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmengl)
STD_ROM_FN(gg_xmengl)

struct BurnDriver BurnDrvgg_xmengl = {
	"gg_xmengl", NULL, NULL, NULL, "1994",
	"X-Men - Gamemaster's Legacy (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenglRomInfo, gg_xmenglRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Gamemaster's Legacy (Prototype, 19940810)

static struct BurnRomInfo gg_xmenglp3RomDesc[] = {
	{ "x-men - gamemaster's legacy (prototype - aug 10,  1994).bin",	0x80000, 0x66cc4034, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenglp3)
STD_ROM_FN(gg_xmenglp3)

struct BurnDriver BurnDrvgg_xmenglp3 = {
	"gg_xmenglp3", "gg_xmengl", NULL, NULL, "1994",
	"X-Men - Gamemaster's Legacy (Prototype, 19940810)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenglp3RomInfo, gg_xmenglp3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Gamemaster's Legacy (Prototype 36, 19940831)

static struct BurnRomInfo gg_xmenglp2RomDesc[] = {
	{ "x-men - gamemaster's legacy (prototype 36 - aug 31,  1994).bin",	0x80000, 0x271a0a1d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenglp2)
STD_ROM_FN(gg_xmenglp2)

struct BurnDriver BurnDrvgg_xmenglp2 = {
	"gg_xmenglp2", "gg_xmengl", NULL, NULL, "1994",
	"X-Men - Gamemaster's Legacy (Prototype 36, 19940831)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenglp2RomInfo, gg_xmenglp2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Gamemaster's Legacy (Prototype 51, 19940906)

static struct BurnRomInfo gg_xmenglp1RomDesc[] = {
	{ "x-men - gamemaster's legacy (prototype 51 - sep 06,  1994).bin",	0x80000, 0xa3cf94ba, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenglp1)
STD_ROM_FN(gg_xmenglp1)

struct BurnDriver BurnDrvgg_xmenglp1 = {
	"gg_xmenglp1", "gg_xmengl", NULL, NULL, "1994",
	"X-Men - Gamemaster's Legacy (Prototype 51, 19940906)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenglp1RomInfo, gg_xmenglp1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Euro, USA)

static struct BurnRomInfo gg_xmenmojoRomDesc[] = {
	{ "x-men - mojo world (usa, europe).bin",	0x80000, 0xc2cba9d7, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojo)
STD_ROM_FN(gg_xmenmojo)

struct BurnDriver BurnDrvgg_xmenmojo = {
	"gg_xmenmojo", NULL, NULL, NULL, "1996",
	"X-Men - Mojo World (Euro, USA)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojoRomInfo, gg_xmenmojoRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Prototype, 19960605)

static struct BurnRomInfo gg_xmenmojop7RomDesc[] = {
	{ "x-men - mojo world (prototype - jun 05,  1996).bin",	0x80000, 0x626f9987, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojop7)
STD_ROM_FN(gg_xmenmojop7)

struct BurnDriver BurnDrvgg_xmenmojop7 = {
	"gg_xmenmojop7", "gg_xmenmojo", NULL, NULL, "1996",
	"X-Men - Mojo World (Prototype, 19960605)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojop7RomInfo, gg_xmenmojop7RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Prototype, 19960613)

static struct BurnRomInfo gg_xmenmojop6RomDesc[] = {
	{ "x-men - mojo world (prototype - jun 13,  1996).bin",	0x80000, 0xc4f3ced4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojop6)
STD_ROM_FN(gg_xmenmojop6)

struct BurnDriver BurnDrvgg_xmenmojop6 = {
	"gg_xmenmojop6", "gg_xmenmojo", NULL, NULL, "1996",
	"X-Men - Mojo World (Prototype, 19960613)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojop6RomInfo, gg_xmenmojop6RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Prototype, 19960624)

static struct BurnRomInfo gg_xmenmojop5RomDesc[] = {
	{ "x-men - mojo world (prototype - jun 24,  1996).bin",	0x80000, 0x78001a5d, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojop5)
STD_ROM_FN(gg_xmenmojop5)

struct BurnDriver BurnDrvgg_xmenmojop5 = {
	"gg_xmenmojop5", "gg_xmenmojo", NULL, NULL, "1996",
	"X-Men - Mojo World (Prototype, 19960624)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojop5RomInfo, gg_xmenmojop5RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Prototype, 19960625)

static struct BurnRomInfo gg_xmenmojop4RomDesc[] = {
	{ "x-men - mojo world (prototype - jun 25,  1996).bin",	0x80000, 0xd4d2f077, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojop4)
STD_ROM_FN(gg_xmenmojop4)

struct BurnDriver BurnDrvgg_xmenmojop4 = {
	"gg_xmenmojop4", "gg_xmenmojo", NULL, NULL, "1996",
	"X-Men - Mojo World (Prototype, 19960625)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojop4RomInfo, gg_xmenmojop4RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Prototype, 19960627)

static struct BurnRomInfo gg_xmenmojop3RomDesc[] = {
	{ "x-men - mojo world (prototype - jun 27,  1996).bin",	0x80000, 0x425ac73f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojop3)
STD_ROM_FN(gg_xmenmojop3)

struct BurnDriver BurnDrvgg_xmenmojop3 = {
	"gg_xmenmojop3", "gg_xmenmojo", NULL, NULL, "1996",
	"X-Men - Mojo World (Prototype, 19960627)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojop3RomInfo, gg_xmenmojop3RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Prototype, 19960628)

static struct BurnRomInfo gg_xmenmojop2RomDesc[] = {
	{ "x-men - mojo world (prototype - jun 28,  1996).bin",	0x80000, 0xd44925ea, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojop2)
STD_ROM_FN(gg_xmenmojop2)

struct BurnDriver BurnDrvgg_xmenmojop2 = {
	"gg_xmenmojop2", "gg_xmenmojo", NULL, NULL, "1996",
	"X-Men - Mojo World (Prototype, 19960628)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojop2RomInfo, gg_xmenmojop2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// X-Men - Mojo World (Prototype, 19960629)

static struct BurnRomInfo gg_xmenmojop1RomDesc[] = {
	{ "x-men - mojo world (prototype - jun 29,  1996).bin",	0x80000, 0xba71da19, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_xmenmojop1)
STD_ROM_FN(gg_xmenmojop1)

struct BurnDriver BurnDrvgg_xmenmojop1 = {
	"gg_xmenmojop1", "gg_xmenmojo", NULL, NULL, "1996",
	"X-Men - Mojo World (Prototype, 19960629)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_xmenmojop1RomInfo, gg_xmenmojop1RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Yogi Bear in Yogi Bear's Goldrush (Prototype)

static struct BurnRomInfo gg_yogibearRomDesc[] = {
	{ "yogi bear in yogi bear's goldrush (unknown) (proto).bin",	0x40000, 0xe678f264, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_yogibear)
STD_ROM_FN(gg_yogibear)

struct BurnDriver BurnDrvgg_yogibear = {
	"gg_yogibear", NULL, NULL, NULL, "199?",
	"Yogi Bear in Yogi Bear's Goldrush (Prototype)\0", NULL, "Unknown", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_yogibearRomInfo, gg_yogibearRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Yu Yu Hakusho - Horobishimono no Gyakushuu (Jpn)

static struct BurnRomInfo gg_yuyuRomDesc[] = {
	{ "mpr-16456.ic1",	0x80000, 0x88ebbf9e, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_yuyu)
STD_ROM_FN(gg_yuyu)

struct BurnDriver BurnDrvgg_yuyu = {
	"gg_yuyu", NULL, NULL, NULL, "1994",
	"Yu Yu Hakusho - Horobishimono no Gyakushuu (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_yuyuRomInfo, gg_yuyuRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Yu Yu Hakusho 2 - Gekitou! Nanakyou no Tatakai (Jpn)

static struct BurnRomInfo gg_yuyu2RomDesc[] = {
	{ "mpr-17084.ic1",	0x80000, 0x46ae9159, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_yuyu2)
STD_ROM_FN(gg_yuyu2)

struct BurnDriver BurnDrvgg_yuyu2 = {
	"gg_yuyu2", NULL, NULL, NULL, "1994",
	"Yu Yu Hakusho 2 - Gekitou! Nanakyou no Tatakai (Jpn)\0", NULL, "Sega", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_yuyu2RomInfo, gg_yuyu2RomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zan Gear (Jpn)

static struct BurnRomInfo gg_zangearRomDesc[] = {
	{ "zan gear (japan).bin",	0x20000, 0x141aaf96, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_zangear)
STD_ROM_FN(gg_zangear)

struct BurnDriver BurnDrvgg_zangear = {
	"gg_zangear", NULL, NULL, NULL, "1990",
	"Zan Gear (Jpn)\0", NULL, "Wolf Team", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_zangearRomInfo, gg_zangearRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zool (Euro)

static struct BurnRomInfo gg_zoolRomDesc[] = {
	{ "zool (euro).bin",	0x40000, 0x23904898, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_zool)
STD_ROM_FN(gg_zool)

struct BurnDriver BurnDrvgg_zool = {
	"gg_zool", NULL, NULL, NULL, "1994",
	"Zool (Euro)\0", NULL, "Gremlin", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_zoolRomInfo, gg_zoolRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zool no Yume Bouken (Jpn)

static struct BurnRomInfo gg_zooljRomDesc[] = {
	{ "zool no yume bouken (japan).bin",	0x40000, 0xe35ef7ed, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_zoolj)
STD_ROM_FN(gg_zoolj)

struct BurnDriver BurnDrvgg_zoolj = {
	"gg_zoolj", "gg_zool", NULL, NULL, "1994",
	"Zool no Yume Bouken (Jpn)\0", NULL, "Infocom", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_zooljRomInfo, gg_zooljRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zool (USA)

static struct BurnRomInfo gg_zooluRomDesc[] = {
	{ "zool (usa).bin",	0x40000, 0xb287c695, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_zoolu)
STD_ROM_FN(gg_zoolu)

struct BurnDriver BurnDrvgg_zoolu = {
	"gg_zoolu", "gg_zool", NULL, NULL, "1993",
	"Zool (USA)\0", NULL, "GameTek", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_zooluRomInfo, gg_zooluRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zoop (USA, Prototype)

static struct BurnRomInfo gg_zooppRomDesc[] = {
	{ "zoop (europe).bin",	0x20000, 0xf397f041, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_zoopp)
STD_ROM_FN(gg_zoopp)

struct BurnDriver BurnDrvgg_zoopp = {
	"gg_zoopp", "gg_zoop", NULL, NULL, "1995",
	"Zoop (USA, Prototype)\0", NULL, "Viacom New Media", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_zooppRomInfo, gg_zooppRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// Zoop (USA)

static struct BurnRomInfo gg_zoopRomDesc[] = {
	{ "zoop (usa).bin",	0x20000, 0x3247ff8b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_zoop)
STD_ROM_FN(gg_zoop)

struct BurnDriver BurnDrvgg_zoop = {
	"gg_zoop", NULL, NULL, NULL, "1995",
	"Zoop (USA)\0", NULL, "Viacom New Media", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_zoopRomInfo, gg_zoopRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// WildSnake 1.0 Unreleased (Prototype?) (Spectrum Holobyte)

static struct BurnRomInfo gg_wildsnakeRomDesc[] = {
	{ "WildSnake [Proto].gg",	0x20000, 0xd460cc7f, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(gg_wildsnake)
STD_ROM_FN(gg_wildsnake)

struct BurnDriver BurnDrvgg_wildsnake = {
	"gg_wildsnake", NULL, NULL, NULL, "1994",
	"WildSnake (Prototype / Unreleased)\0", NULL, "Spectrum HoloByte", "Sega Game Gear",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_GAME_GEAR, GBF_MISC, 0,
	GGGetZipName, gg_wildsnakeRomInfo, gg_wildsnakeRomName, NULL, NULL, SMSInputInfo, GGDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Hong Kil Dong (KR)

static struct BurnRomInfo sms_hongkildongRomDesc[] = {
	{ "Hong Kil Dong (KR).sms",	0x0C000, 0x8040b2fa, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_hongkildong)
STD_ROM_FN(sms_hongkildong)

struct BurnDriver BurnDrvsms_hongkildong = {
	"sms_hongkildong", NULL, NULL, NULL, "1991",
	"Hong Kil Dong (Kor)\0", NULL, "Clover", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_hongkildongRomInfo, sms_hongkildongRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// DARC (Version 1.0)

static struct BurnRomInfo sms_darcRomDesc[] = {
	{ "DARC.sms",	0x80000, 0x61B3C657, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_darc)
STD_ROM_FN(sms_darc)

struct BurnDriver BurnDrvsms_darc = {
	"sms_darc10", NULL, NULL, NULL, "2015",
	"DARC (Version 1.0)\0", "Turn ON 'FM Emulation' in Dips for music/sfx!", "Zipper", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_darcRomInfo, sms_darcRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Lost Raider (Version 1.01)

static struct BurnRomInfo sms_lostraider101RomDesc[] = {
	{ "LostRaider-SMS-1.01.sms",	0x20000, 0x2553b745, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lostraider101)
STD_ROM_FN(sms_lostraider101)

struct BurnDriver BurnDrvsms_lostraider101 = {
	"sms_lostraider101", NULL, NULL, NULL, "2015",
	"Lost Raider (Version 1.01)\0", NULL, "Vingazole & Ichigobankai", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_lostraider101RomInfo, sms_lostraider101RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Moggy Master (Version 1.00)

static struct BurnRomInfo sms_moggym100RomDesc[] = {
	{ "mojon-twins--moggy-master.sms",	0x10000, 0x039539df, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_moggym100)
STD_ROM_FN(sms_moggym100)

struct BurnDriver BurnDrvsms_moggym100 = {
	"sms_moggym100", NULL, NULL, NULL, "2015",
	"Moggy Master (Version 1.00)\0", NULL, "Mojon Twins", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_moggym100RomInfo, sms_moggym100RomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Geki Oko PunPun Maru (Version 20151031b)

static struct BurnRomInfo sms_punpunRomDesc[] = {
	{ "punpun_20151031b.sms",	0x40000, 0xb35ebcdf, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_punpun)
STD_ROM_FN(sms_punpun)

struct BurnDriver BurnDrvsms_punpun = {
	"sms_punpun", NULL, NULL, NULL, "2013",
	"Geki Oko PunPun Maru (Version 20151031b?)\0", NULL, "Future Driver", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_JAPANESE, GBF_MISC, 0,
	SMSGetZipName, sms_punpunRomInfo, sms_punpunRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Waimanu: Scary Monsters Saga

static struct BurnRomInfo sms_waimanuRomDesc[] = {
	{ "WaimanuSMS.sms",	0x20000, 0x195c3f78, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_waimanu)
STD_ROM_FN(sms_waimanu)

struct BurnDriver BurnDrvsms_waimanu = {
	"sms_WaimanuSMS", NULL, NULL, NULL, "2015",
	"Waimanu: Scary Monsters Saga\0", NULL, "Disjointed Studio", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_waimanuRomInfo, sms_waimanuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Bruce Lee (Version 1.0)

static struct BurnRomInfo sms_bruceleeRomDesc[] = {
	{ "BruceLee-SMS-1.00.sms",	0x20000, 0x37E27A38, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_brucelee)
STD_ROM_FN(sms_brucelee)

struct BurnDriver BurnDrvsms_brucelee = {
	"sms_brucelee10", NULL, NULL, NULL, "2015",
	"Bruce Lee (Version 1.0)\0", NULL, "Kagesan", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_bruceleeRomInfo, sms_bruceleeRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Lambo (DEMO) by Genesis Project

static struct BurnRomInfo sms_lamboRomDesc[] = {
	{ "GenesisProject-Lambo.sms",	0x40000, 0x92FE2775, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_lambo)
STD_ROM_FN(sms_lambo)

struct BurnDriver BurnDrvsms_lambo = {
	"sms_lambo", NULL, NULL, NULL, "2015",
	"Lambo (Demo)\0", NULL, "Genesis Project", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM | HARDWARE_SMS_DISPLAY_PAL, GBF_MISC, 0,
	SMSGetZipName, sms_lamboRomInfo, sms_lamboRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Bara Buruu (Version 1.0)

static struct BurnRomInfo sms_baraburuRomDesc[] = {
	{ "BaraBuruu-SMS-1.00.sms",	0x20000, 0x43e39aa4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_baraburu)
STD_ROM_FN(sms_baraburu)

struct BurnDriver BurnDrvsms_baraburu = {
	"sms_baraburu10", NULL, NULL, NULL, "2016",
	"Bara Buruu (Version 1.0)\0", NULL, "Kagesan", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_baraburuRomInfo, sms_baraburuRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// Data Storm (Version 1.0)

static struct BurnRomInfo sms_datastormRomDesc[] = {
	{ "datastorm.sms",	0x8000, 0x37b775d6, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_datastorm)
STD_ROM_FN(sms_datastorm)

struct BurnDriver BurnDrvsms_datastorm = {
	"sms_datastorm10", NULL, NULL, NULL, "2016",
	"Data Storm (Version 1.0)\0", NULL, "haroldoop", "Sega Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_datastormRomInfo, sms_datastormRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

