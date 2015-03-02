// Sega Mastersystem driver for FBA, interface for SMS Plus by Charles MacDonald

#include "tiles_generic.h"
#include "smsshared.h"
#include "z80_intf.h"
#include "sn76496.h"

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
	{0x3d, 0xff, 0xff, 0x00, NULL				},
};

STDDIPINFO(SMS)

static struct BurnInputInfo SMSInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	SMSJoy1 + 1,	"p1 start"	},
	{"P1 Select",		BIT_DIGITAL,	SMSJoy1 + 2,	"p1 select"	},
	{"P1 Up",		    BIT_DIGITAL,	SMSJoy1 + 3,	"p1 up"		},
	{"P1 Down",		    BIT_DIGITAL,	SMSJoy1 + 4,	"p1 down"	},
	{"P1 Left",		    BIT_DIGITAL,	SMSJoy1 + 5,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	SMSJoy1 + 6,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	SMSJoy1 + 7,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	SMSJoy1 + 8,	"p1 fire 2"	},

	{"P2 Start",		BIT_DIGITAL,	SMSJoy2 + 1,	"p2 start"	},
	{"P2 Select",		BIT_DIGITAL,	SMSJoy2 + 2,	"p2 select"	},
	{"P2 Up",		    BIT_DIGITAL,	SMSJoy2 + 3,	"p2 up"		},
	{"P2 Down",		    BIT_DIGITAL,	SMSJoy2 + 4,	"p2 down"	},
	{"P2 Left",		    BIT_DIGITAL,	SMSJoy2 + 5,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	SMSJoy2 + 6,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	SMSJoy2 + 7,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	SMSJoy2 + 8,	"p2 fire 2"	},

	{"Reset",		    BIT_DIGITAL,	&DrvReset,	    "reset"		},
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

INT32 SMSInit();

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);
	vdp_init();     // gets rid of crap on the screen w/GG
	render_init();  // ""
	sms_reset();

	return 0;
}

INT32 SMSExit()
{
	ZetExit();
	GenericTilesExit();

	BurnFree (AllMem);

	if(cart.rom)
    {
        free(cart.rom);
        cart.rom = NULL;
    }

	system_shutdown();

	return 0;
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
        else
        if(SMSJoy1[4]) input.pad[0] |= INPUT_DOWN;
        if(SMSJoy1[5]) input.pad[0] |= INPUT_LEFT;
        else
        if(SMSJoy1[6]) input.pad[0] |= INPUT_RIGHT;
        if(SMSJoy1[7]) input.pad[0] |= INPUT_BUTTON2;
		if(SMSJoy1[8]) input.pad[0] |= INPUT_BUTTON1;
		// Player 2
		if(SMSJoy2[3]) input.pad[1] |= INPUT_UP;
        else
        if(SMSJoy2[4]) input.pad[1] |= INPUT_DOWN;
        if(SMSJoy2[5]) input.pad[1] |= INPUT_LEFT;
        else
        if(SMSJoy2[6]) input.pad[1] |= INPUT_RIGHT;
        if(SMSJoy2[7]) input.pad[1] |= INPUT_BUTTON2;
		if(SMSJoy2[8]) input.pad[1] |= INPUT_BUTTON1;
        if(SMSJoy1[1]) input.system |= (IS_GG) ? INPUT_START : INPUT_PAUSE;
	}

	ZetOpen(0);
	system_frame(0);
	ZetClose();

	if (pBurnDraw)
		SMSDraw();

	return 0;
}

void system_manage_sram(uint8 *sram, int slot, int mode)
{

}

typedef struct {
    uint32 crc;
    int mapper;
    int display;
    int territory;
    char *name;
} rominfo_t;

static rominfo_t game_list[] = {
    {0x29822980, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Cosmic Spacehead"},
    {0xB9664AE1, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Fantastic Dizzy"},
    {0xA577CE46, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Micro Machines"},
    {0x8813514B, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Excellent Dizzy (Proto)"},
    {0xAA140C9C, MAPPER_CODIES, DISPLAY_PAL, TERRITORY_EXPORT, "Excellent Dizzy (Proto - GG)"}, 
	{0x72420f38, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Addams Family"},
	{0x887d9f6b, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "XXAce of Aces"},
	{0x3793c01a, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "XXShadow Dancer (KR)"},
	{0x5c205ee1, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Xenon 2"},
    {0xec726c0d, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Xenon 2"},
	{0xe0b1aff8, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Smash TV"},
	{0x2d48c1d3, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Back to the Future III"},
	{0xc0e25d62, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "California Games II (Europe)"},
	{0x7e5839a0, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Championship Hockey"},
	{0x13ac9023, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Cool Spot (Euro)"},
	{0xb3768a7a, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Disney's Bonkers"},
	{0x8370f6cd, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Double Hawk"},
	{0xc4d5efc5, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Earthworm Jim"},
	{0xec2da554, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "GP Rider"},
	{0x205caae8, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Operation Wolf"},
	{0x95b9ea95, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "PGA Tour Golf"},
	{0xb840a446, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Pit Fighter (euro)"},
	{0x0047b615, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Predator (euro)"},
	{0xa908cff5, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Space Gun (euro)"},
	{0xebe45388, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Spider Man - Sinister Six"},
	{0x0f8287ec, MAPPER_SEGA,   DISPLAY_PAL, TERRITORY_EXPORT, "Street Fighter II"},
	{0x445525E2, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Penguin Adventure (KR)"},
	{0x83F0EEDE, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Street Master (KR)"},
	{0xA05258F5, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Won-Si-In (KR)"},
	{0x06965ED9, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "F-1 Spirit - The way to Formula-1 (KR)"},
	{0x77EFE84A, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Cyborg Z (KR)"},
	{0xF89AF3CC, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Knightmare II - The Maze of Galious (KR)"},
	{0x9195C34C, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Super Boy 3 (KR)"},
	{0xE316C06D, MAPPER_MSX_NEMESIS, DISPLAY_NTSC, TERRITORY_EXPORT, "XNemesis (KR)"},
	{0x0A77FA5E, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Nemesis 2 (KR)"},
	{0x565c799f, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Xyzolog (KR)"},
	{0xca082218, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "XPooyan (KR)"},
	{0x89b79e77, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "XDallye Pigu-Wang (KR)"},
	{0x61e8806f, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Flashpoint (KR)"},
	//{, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, " (KR)"},
	{0x643b6b76, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Block Hole (KR)"},
	{0x577ec227, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Galaxian (KR)"},
	{0x0ae470e5, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "King and Balloon (KR)"},
	{0xb49aa6fc, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Mopiranger (KR)"},
	{0x41cc2ade, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Micro Xevious (KR)"},
	{0x8034bd27, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Road Fighter (KR)"},
	{0x5b8e65e4, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Sky Jaguar (KR)"},
	{0x08bf3de3, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "XAlibaba (KR)"},
	{0x6d309ac5, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Bubble Bobble YMsoft (KR)"},
	{0x22c09cfd, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Super Bubble Boggle (KR)"},
	{0xdbbf4dd1, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "New Boggle Boggle 2 (KR)"},
	{0x0918fba0, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "C_SO! (KR)"},
	{0x7778e256, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Comic Bakery (KR)"},
	{0xf06f2ccb, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Eagles 5(KR)"},
	{0xdd74bcf1, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Exa Innova E.I. (KR)"},
	{0x17ab6883, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "FA Tetris (KR)"},
	{0x18fb98a3, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, "Jang Pung 3 (KR)"},
	//{, MAPPER_MSX,         DISPLAY_NTSC, TERRITORY_EXPORT, " (KR)"},
	{0         , -1  , -1, -1, NULL},
};
// Notes:
// X-prefix = not working, possibly a different mapper(?)
// Sangokushi 3 (Kor) is broke. maybe its a korean 8k mapper?
// Super Arkanoid - no input??
// Dinobasher - weird!
// Earthworm Jim - crashes after title
// GP Rider - goes mental in-game
// Jang Pung 3 - goes mental @ boot
// Space Gun - won't boot
// Shadow Dancer - won't boot
// Street Fighter II - reboots @ game start
// Spiderman - Sinister Six - won't boot
// The Best Game Collection - don't work

static INT32 load_rom()
{
    INT32 size;
	struct BurnRomInfo ri;

    if(cart.rom)
    {
        free(cart.rom);
        cart.rom = NULL;
    }

	BurnDrvGetRomInfo(&ri, 0);
	size = ri.nLen;

	/* Don't load games smaller than 16K */
    if(size < 0x4000) return 0;

	cart.rom = (uint8 *)malloc(0x100000);
	if (BurnLoadRom(cart.rom + 0x0000, 0, 1)) return 0;

    /* Take care of image header, if present */
    if((size / 512) & 1)
    {
        size -= 512;
        memmove(cart.rom, cart.rom + 512, size);
    }

    cart.pages = (size / 0x4000);
    cart.crc = crc32(0L, cart.rom, size);

    /* Assign default settings (US NTSC machine) */
    cart.mapper     = MAPPER_SEGA;
    sms.display     = DISPLAY_NTSC;
    sms.territory   = TERRITORY_EXPORT;

    /* Look up mapper in game list */
    for(INT32 i = 0; game_list[i].name != NULL; i++)
    {
        if(cart.crc == game_list[i].crc)
        {
            cart.mapper     = game_list[i].mapper;
            sms.display     = game_list[i].display;
            sms.territory   = game_list[i].territory;
        }
    }

	/* Figure out game image type */
	if (strstr(BurnDrvGetTextA(DRV_NAME), "sms_ggaleste")) {
		sms.console = CONSOLE_GG; // Temp. hack for GG Aleste, since GG games aren't "officially" supported yet.
	}
    else
        sms.console = CONSOLE_SMS;

    system_assign_device(PORT_A, DEVICE_PAD2B);
    system_assign_device(PORT_B, DEVICE_PAD2B);

    return 1;
}

INT32 SMSInit()
{
	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	GenericTilesInit();

    if(load_rom() == 0)
	{
		bprintf(0, _T("Error loading SMS rom!\n"));
		return 1;
	} else {
		bprintf(0, _T("SMS rom loaded ok!\n"));
	}

/* Set up bitmap structure */
    memset(&bitmap, 0, sizeof(bitmap_t));
    bitmap.width  = 256;
    bitmap.height = 192;
    bitmap.depth  = 16;
    bitmap.granularity = 2;
    bitmap.pitch  = bitmap.width * bitmap.granularity;
    bitmap.data   = (uint8 *)pTransDraw;
    bitmap.viewport.x = 0;
    bitmap.viewport.y = 0;
    bitmap.viewport.w = 256;
    bitmap.viewport.h = 192;

    snd.fm_which = SND_EMU2413;
    snd.fps = (1) ? FPS_NTSC : FPS_PAL;
    snd.fm_clock = (1) ? CLOCK_NTSC : CLOCK_PAL;
    snd.psg_clock = (1) ? CLOCK_NTSC : CLOCK_PAL;
    snd.sample_rate = 44100;
    snd.mixer_callback = NULL;

    sms.use_fm = 0;

    system_init();

	return 0;
}

static void system_load_state()
{
    sms_mapper_w(3, cart.fcr[3]);
    sms_mapper_w(2, cart.fcr[2]);
    sms_mapper_w(1, cart.fcr[1]);
    sms_mapper_w(0, cart.fcr[0]);

    /* Force full pattern cache update */
    bg_list_index = 0x200;
    for(INT32 i = 0; i < 0x200; i++)
    {
        bg_name_list[i] = i;
        bg_name_dirty[i] = -1;
    }

    /* Restore palette */
    for(INT32 i = 0; i < PALETTE_SIZE; i++)
		palette_sync(i, 1);
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

// 4 PAK All Action (Aus)

static struct BurnRomInfo sms_4pakRomDesc[] = {
	{ "4 pak all action (aus).bin",	0x100000, 0xa67f2a5c, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_4pak)
STD_ROM_FN(sms_4pak)

struct BurnDriver BurnDrvsms_4pak = {
	"sms_4pak", NULL, NULL, NULL, "1995",
	"4 PAK All Action (Aus)\0", NULL, "HES", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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

struct BurnDriver BurnDrvsms_20em1 = {
	"sms_20em1", NULL, NULL, NULL, "1995",
	"20 em 1 (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"The Three Dragon Story (Kor)\0", NULL, "Zemina?", "Sega Mastersystem",
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
	"94 Super World Cup Soccer (Kor)\0", NULL, "Daou Infosys", "Sega Mastersystem",
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
	"Ace of Aces (Euro)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Action Fighter (Euro, USA, Bra, v2)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Action Fighter (Euro, Jpn, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Addams Family (Euro)\0", NULL, "Flying Edge", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Aerial Assault (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Aerial Assault (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"After Burner (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Agigongnyong Dooly (Kor)\0", NULL, "Daou Infosys", "Sega Mastersystem",
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
	"Air Rescue (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Disney's Aladdin (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Aleste (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Aleste II GG (SMS Conversion)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_aleste2gg2smsRomInfo, sms_aleste2gg2smsRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};

// GG Aleste

static struct BurnRomInfo sms_alesteggRomDesc[] = {
	{ "gg aleste (japan).bin",	0x40000, 0x1b80a75b, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_alestegg)
STD_ROM_FN(sms_alestegg)

struct BurnDriver BurnDrvsms_alestegg = {
	"sms_ggaleste", NULL, NULL, NULL, "1993",
	"Aleste GG\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alesteggRomInfo, sms_alesteggRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
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
	"Alex Kidd BMX Trial (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alex Kidd - High-Tech World (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alex Kidd in Miracle World (Euro, USA, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alex Kidd in Miracle World (Euro, USA, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alex Kidd in Miracle World (Bra, v1, Pirate)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Alex Kidd no Miracle World (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_alexkiddjRomInfo, sms_alexkiddjRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
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
	"Alex Kidd - The Lost Stars (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alex Kidd in Shinobi World (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alf (USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alibaba and 40 Thieves (Kor)\0", NULL, "HiCom", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Alien 3 (Euro, Bra)\0", NULL, "Arena", "Sega Mastersystem",
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
	"Altered Beast (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"American Baseball (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"American Pro Football (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Andre Agassi Tennis (Euro, Bra)\0", NULL, "TecMagik", "Sega Mastersystem",
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
	"Anmitsu Hime (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Arcade Smash Hits (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Argos no Juujiken (Jpn)\0", NULL, "Tecmo", "Sega Mastersystem",
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
	"Argos no Juujiken (Jpn, Pirate?)\0", NULL, "Tecmo", "Sega Mastersystem",
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
	"Disney's Ariel The Little Mermaid (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Ashura (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Assault City (Euro, Bra, Light Phaser version)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Assault City (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Asterix and the Great Rescue (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Asterix (Euro, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Asterix (Euro, Bra, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Asterix and the Secret Mission (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alien Storm (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Astro Flash (Jpn, Pirate)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Astro Warrior and Pit Pot (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Astro Warrior (Jpn, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alien Syndrome (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alien Syndrome (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Alien Syndrome (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ayrton Senna's Super Monaco GP II (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Aztec Adventure - The Golden Road to Paradise (Euro, USA) ~ Nazca '88 - The Golden Road to Paradise (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Back to the Future Part II (Euro, Bra)\0", NULL, "Image Works", "Sega Mastersystem",
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
	"Back to the Future Part III (Euro)\0", NULL, "Image Works", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Baku Baku Animal (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Bank Panic (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Simpsons - Bart vs. The Space Mutants (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"The Simpsons - Bart vs. The World (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"Basket Ball Nightmare (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Batman Returns (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Battlemaniacs (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Battle Out Run (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Shadow of the Beast (Euro, Bra)\0", NULL, "TecMagik", "Sega Mastersystem",
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
	"The Best Game Collection - Hang On + Pit Pot + Spy vs Spy (Kor)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection - Great Baseball + Great Soccer + Super Tennis (Kor)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection - Teddy Boy Blues + Pit-Pot + Astro Flash (Kor)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection - Teddy Boy Blues + Great Soccer + Comical Machine Gun Joe (Kor)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection - Ghost House + Teddy Boy Blues + Seishun Scandal (Kor)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection - Satellite-7 + Great Baseball + Seishun Scandal (Kor)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection (Kor, 8 in 1 Ver. A)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection (Kor, 8 in 1 Ver. B)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Best Game Collection (Kor, 8 in 1 Ver. C)\0", NULL, "Hi-Com", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Black Belt (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Blade Eagle (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Blade Eagle (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Block Hole (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Bomber Raid (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Bonanza Bros. (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Disney's Bonkers Wax Up! (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Chouon Senshi Borgman (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Chouon Senshi Borgman (Jpn, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Bram Stoker's Dracula (Euro)\0", NULL, "Sony Imagesoft", "Sega Mastersystem",
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
	"Bubble Bobble (Kor, Clover)\0", NULL, "Clover", "Sega Mastersystem",
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
	"Bubble Bobble (Kor, YM Soft)\0", NULL, "YM Soft", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Super Bubble Bobble (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"New Boggle Boggle 2 (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Bubble Bobble (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Buggy Run (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"C_So! (Kor)\0", NULL, "Joy Soft", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"California Games II (Euro)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"California Games II (Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"California Games (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Captain Silver (Euro, Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Captain Silver (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Casino Games (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Castelo Ra-Tim-Bum (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Castle of Illusion Starring Mickey Mouse (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Castle of Illusion Starring Mickey Mouse (USA, Display Unit Sample)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Castle of Illusion Starring Mickey Mouse (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Champions of Europe (Euro, Bra)\0", NULL, "TecMagik", "Sega Mastersystem",
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
	"Chapolim x Dracula - Um Duelo Assustador (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Taito Chase H.Q. (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Cheese Cat-astrophe Starring Speedy Gonzales (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Championship Hockey (Euro)\0", NULL, "U.S. Gold", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Choplifter (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Choplifter (Jpn, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Choplifter (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Chuck Rock (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Chuck Rock II - Son of Chuck (Euro)\0", NULL, "Core Design", "Sega Mastersystem",
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
	"Chuck Rock II - Son of Chuck (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"The Circuit (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Cloud Master (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Color and Switch Test (v1.3)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Columns (Euro, USA, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Columns (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Comic Bakery (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Comical Machine Gun Joe (Tw)\0", NULL, "Aaronix", "Sega Mastersystem",
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
	"Comical Machine Gun Joe (Kor)\0", NULL, "Samsung", "Sega Mastersystem",
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
	"Cool Spot (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Cosmic Spacehead (Euro)\0", NULL, "Codemasters", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Cyber Shinobi (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Cyber Shinobi (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Cyborg Hunter (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Cyborg Z (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Daffy Duck in Hollywood (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dallyeora Pigu-Wang (Kor)\0", NULL, "Game Line", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Danan: The Jungle Fighter (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Double Dragon (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Double Dragon (Kor)\0", NULL, "Samsung", "Sega Mastersystem",
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
	"Dead Angle (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Deep Duck Trouble Starring Donald Duck (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Desert Speedtrap Starring Road Runner and Wile E. Coyote (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dick Tracy (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dinobasher Starring Bignose the Caveman (Euro, Prototype)\0", NULL, "Codemasters", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Dinosaur Dooley (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Doki Doki Penguin Land - Uchuu Daibouken (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Doki Doki Penguin Land - Uchuu Daibouken (Jpn, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Double Hawk (Euro)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Double Hawk (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Double Target - Cynthia no Nemuri (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dragon - The Bruce Lee Story (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Dragon Crystal (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dr. HELLO (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Dr. Robotnik's Mean Bean Machine (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Desert Strike (Euro)\0", NULL, "Domark", "Sega Mastersystem",
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
	"Gokuaku Doumei Dump Matsumoto (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dynamite Duke (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dynamite Dux (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Dynamite Headdy (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"E.I. - Exa Innova (Kor)\0", NULL, "HiCom", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Eagles 5 (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Ecco the Dolphin (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ecco - The Tides of Time (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Earthworm Jim (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Enduro Racer (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Enduro Racer (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"E-SWAT - City Under Siege (Euro, USA, Easy Version)\0", NULL, "Sega", "Sega Mastersystem",
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
	"E-SWAT - City Under Siege (Euro, USA, Bra, Hard Version)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Excellent Dizzy Collection (Euro, USA, Prototype)\0", NULL, "Codemasters", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_excdizzyRomInfo, sms_excdizzyRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


// F1 (Euro, Bra)

static struct BurnRomInfo sms_f1RomDesc[] = {
	{ "mpr-15830-f.ic1",	0x40000, 0xec788661, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_f1)
STD_ROM_FN(sms_f1)

struct BurnDriver BurnDrvsms_f1 = {
	"sms_f1", NULL, NULL, NULL, "1993",
	"F1 (Euro, Bra)\0", NULL, "Domark", "Sega Mastersystem",
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
	"F-1 Spirit - The way to Formula-1 (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"F-16 Fighting Falcon (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"F-16 Fighting Falcon (Tw)\0", NULL, "Aaronix", "Sega Mastersystem",
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
	"F-16 Fighter (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"FA Tetris (Kor)\0", NULL, "FA Soft", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Family Games (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fantastic Dizzy (Euro)\0", NULL, "Codemasters", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Fantasy Zone II - The Tears of Opa-Opa (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fantasy Zone II - Opa Opa no Namida (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fantasy Zone (World, v2)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fantasy Zone (World, v1, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fantasy Zone (Jpn, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fantasy Zone (Tw)\0", NULL, "Aaronix", "Sega Mastersystem",
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
	"Fantasy Zone - The Maze (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Final Bubble Bobble (Jpn)\0", NULL, "Sega ", "Sega Mastersystem",
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
	"Felipe em Acao (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Ferias Frustradas do Pica-Pau (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"FIFA International Soccer (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Fire and Forget II (Euro)\0", NULL, "Titus Arcade", "Sega Mastersystem",
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
	"Fire and Ice (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"The Flash (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Flashpoint (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Flintstones (Euro, Bra)\0", NULL, "Grandslam", "Sega Mastersystem",
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
	"George Foreman's KO Boxing (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"Forgotten Worlds (Euro, Bra, Aus)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fushigi no Oshiro Pit Pot (Jpn, Pirate?)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Gaegujangi Ggachi (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Gain Ground (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Gain Ground (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Galactic Protector (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Galaxian (Kor)\0", NULL, "HiCom", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Galaxy Force (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Galaxy Force (USA)\0", NULL, "Activision", "Sega Mastersystem",
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
	"Game Box Serie Esportes Radicais (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Gangcheol RoboCop (Kor)\0", NULL, "Sieco", "Sega Mastersystem",
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
	"Gangster Town (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Gauntlet (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Golden Axe (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Golden Axe Warrior (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Geraldinho (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Ghostbusters (Euro, USA, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ghost House (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ghost House (Jpn, Pirate?)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Ghost House (Kor)\0", NULL, "Samsung", "Sega Mastersystem",
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
	"Ghouls'n Ghosts (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ghouls'n Ghosts (USA, Demo)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Global Defense (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Global Defense (Euro, USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"G-LOC Air Battle (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Golfamania (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Golfamania (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Golvellius (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"GP Rider (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Great Baseball (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Baseball (Jpn, Pirate?)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Great Basketball (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Football (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Golf (Euro, USA) ~ Masters Golf (Japan)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Golf (Euro, USA, v1.0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Golf (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Golf (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Golf (Kor)\0", NULL, "Samsung?", "Sega Mastersystem",
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
	"Great Ice Hockey (Jpn, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Soccer (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Soccer (Tw)\0", NULL, "Aaronix", "Sega Mastersystem",
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
	"Great Volleyball (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Volleyball (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Gun.Smoke (Kor)\0", NULL, "ProSoft", "Sega Mastersystem",
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
	"Haja no Fuuin (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hang-On (Euro, Bra, Aus)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hang-On and Astro Warrior (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hang-On and Safari Hunt (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hang-On and Safari Hunt (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Heavyweight Champ (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Advanced Dungeons and Dragons - Heroes of the Lance (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"High School! Kimengumi (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hokuto no Ken (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hokuto no Ken (Tw)\0", NULL, "Aaronix", "Sega Mastersystem",
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
	"Home Alone (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hook (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hoshi wo Sagashite... (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hwarang Ui Geom (Kor)\0", NULL, "Samsung", "Sega Mastersystem",
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
	"Impossible Mission (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Impossible Mission (Euro, Prototype)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"The Incredible Crash Dummies (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"The Incredible Hulk (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Indiana Jones and the Last Crusade (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Indiana Jones and the Last Crusade (Euro, Prototype)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"James 'Buster' Douglas Knockout Boxing (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"James 'Buster' Douglas Knockout Boxing (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"James Bond 007 - The Duel (Euro)\0", NULL, "Domark", "Sega Mastersystem",
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
	"James Bond 007 - The Duel (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Janggun ui Adeul (Kor)\0", NULL, "Daou Infosys", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Jang Pung 3 (Kor)\0", NULL, "Sanghun", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Joe Montana Football (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Jungle Book (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Jurassic Park (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Kenseiden (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Kenseiden (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"King's Quest - Quest for the Crown (USA)\0", NULL, "Parker Brothers", "Sega Mastersystem",
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
	"King's Quest - Quest for the Crown (USA, Prototype)\0", NULL, "Parker Brothers", "Sega Mastersystem",
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
	"King and Balloon (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Klax (Euro)\0", NULL, "Tengen", "Sega Mastersystem",
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
	"Knightmare II - The Maze of Galious (Kor)\0", NULL, "Zemina?", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Krusty's Fun House (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"Kujaku Ou (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Kung Fu Kid (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Land of Illusion Starring Mickey Mouse (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Laser Ghost (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Legend of Illusion Starring Mickey Mouse (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Lemmings (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Lemmings (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Lemmings 2 - The Tribes (Euro, Prototype)\0", NULL, "Psygnosis", "Sega Mastersystem",
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
	"Line of Fire (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Disney's The Lion King (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Lord of the Sword (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Lord of Sword (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Loretta no Shouzou (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Lucky Dime Caper Starring Donald Duck (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Lucky Dime Caper Starring Donald Duck (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Mahjong Sengoku Jidai (Jpn, HK)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Mahjong Sengoku Jidai (Jpn, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Makai Retsuden (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Maou Golvellius (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Maou Golvellius (Jpn, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Marble Madness (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Marksman Shooting and Trap Shooting and Safari Hunt (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Marksman Shooting and Trap Shooting (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Masters of Combat (Euro, Bra, Aus)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Master of Darkness (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Maze Hunter 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Maze Walker (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Megumi Rescue (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Mercs (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Mickey's Ultimate Challenge (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Mick and Mack as the Global Gladiators (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Micro Machines (Euro)\0", NULL, "Codemasters", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Show do Milhao (Bra, Prototype)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Miracle Warriors - Seal of the Dark Lord (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Miracle Warriors - Seal of the Dark Lord (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Missile Defense 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Mortal Kombat (Euro, Bra)\0", NULL, "Arena", "Sega Mastersystem",
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
	"Mortal Kombat II (Euro, Bra)\0", NULL, "Acclaim Entertainment", "Sega Mastersystem",
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
	"Mortal Kombat 3 (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Monica no Castelo do Dragao (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Monopoly (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Monopoly (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Monopoly (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Montezuma's Revenge Featuring Panama Joe (USA)\0", NULL, "Parker Brothers", "Sega Mastersystem",
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
	"Montezuma's Revenge Featuring Panama Joe (USA, Prototype)\0", NULL, "Parker Brothers", "Sega Mastersystem",
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
	"Mopiranger (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Ms. Pac-Man (Euro, Bra)\0", NULL, "Tengen", "Sega Mastersystem",
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
	"Michael Jackson's Moonwalker (Euro, USA, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Michael Jackson's Moonwalker (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Micro Xevious (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"My Hero (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"NBA Jam (Euro, Prototype)\0", NULL, "Acclaim", "Sega Mastersystem",
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
	"Super Golf (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Nekkyuu Koushien (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Nemesis (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Nemesis 2 (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Ninja (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Ninja (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ninja Gaiden (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ninja Gaiden (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Olympic Gold (Euro)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Olympic Gold (Kor)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Opa Opa (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Operation Wolf (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"The Ottifants (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Out Run (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Out Run 3-D (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Out Run Europa (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Pac-Mania (Euro)\0", NULL, "Tengen", "Sega Mastersystem",
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
	"Paperboy (Euro, v0)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Paperboy (USA, Bra, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Parlour Games (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Pat Riley Basketball (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Penguin Adventure (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Penguin Land (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"PGA Tour Golf (Euro)\0", NULL, "Tengen", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"PitFighter - The Ultimate Challenge (Euro)\0", NULL, "Domark", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"PitFighter - The Ultimate Challenge (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Pooyan (Kor)\0", NULL, "HiCom", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Populous (Euro, Bra)\0", NULL, "TecMagik", "Sega Mastersystem",
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
	"Poseidon Wars 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Power Strike (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Power Strike II (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Prince of Persia (Euro, Bra)\0", NULL, "Domark", "Sega Mastersystem",
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
	"Predator 2 (Euro)\0", NULL, "Arena", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Predator 2 (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Pro Wrestling (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The Pro Yakyuu Pennant Race (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Phantasy Star (Euro, USA, v3)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Phantasy Star (Euro, USA, v2)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Phantasy Star (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Phantasy Star (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Phantasy Star (Jpn, MD\0", NULL, "Sega", "Sega Mastersystem",
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
	"Phantasy Star (Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Psychic World (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Psycho Fox (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Putt and Putter (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Putt and Putter (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Puznic (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
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
	"Quartet (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Quest for the Shaven Yak Starring Ren Hoek and Stimpy (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Rainbow Islands - The Story of Bubble Bobble 2 (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Rainbow Islands - The Story of Bubble Bobble 2 (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Rambo - First Blood Part II (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Rambo III (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Rampage (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Rampart (Euro)\0", NULL, "Tengen", "Sega Mastersystem",
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
	"Rastan (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"R.C. Grand Prix (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"R.C. Grand Prix (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Reggie Jackson Baseball (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Renegade (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Rescue Mission (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Road Fighter (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Road Rash (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"James Pond 2 - Codename RoboCod (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"RoboCop 3 (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"RoboCop versus The Terminator (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Rocky (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"R-Type (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"R-Type (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Running Battle (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sagaia (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sangokushi 3 (Kor)\0", NULL, "Game Line", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Sapo Xule - O Mestre do Kung Fu (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"S.O.S Lagoa Poluida (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Sapo Xule vs. Os Invasores do Brejo (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Satellite 7 (Jpn, Pirate?)\0", NULL, "pirate", "Sega Mastersystem",
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
	"The Smurfs 2 (Euro)\0", NULL, "Infogrames", "Sega Mastersystem",
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
	"The Smurfs 2 (Euro, Prototype)\0", NULL, "Infogrames", "Sega Mastersystem",
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
	"Special Criminal Investigation (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Special Criminal Investigation (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Scramble Spirits (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"SDI (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Secret Command (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sega Chess (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sega Graphic Board (Jpn, Prototype v2.0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sega World Tournament Golf (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Seishun Scandal (Jpn, Pirate?)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Sensible Soccer (Euro)\0", NULL, "Sony Imagesoft", "Sega Mastersystem",
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
	"Sky Jaguar (Kor)\0", NULL, "Samsung", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Street Fighter II (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Shadow Dancer (Euro, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Shanghai (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Shanghai (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Space Harrier (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Space Harrier 3-D (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Space Harrier 3D (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Space Harrier (Jpn, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Shinobi (Euro, USA, Bra, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Shinobi (Jpn, Bra, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Shooting Gallery (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sitio do Picapau Amarelo (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Slap Shot (USA, v2)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Slap Shot (Euro, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Slap Shot (Euro, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Slap Shoot (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Monaco GP (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Monaco GP (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Monaco GP (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Monaco GP (Euro, Older Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Promocao Especial M. System III Compact (Bra, Sample)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"The Smurfs (Euro, Bra)\0", NULL, "Infogrames", "Sega Mastersystem",
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
	"Solomon no Kagi - Oujo Rihita no Namida (Jpn)\0", NULL, "Salio", "Sega Mastersystem",
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
	"Sonic The Hedgehog (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_sonicRomInfo, sms_sonicRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
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
	"Sonic The Hedgehog 2 (Euro, Bra, Kor, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sonic The Hedgehog 2 (Euro, Bra, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sonic Blast (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Sonic The Hedgehog Chaos (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sonic's Edusoft (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sonic The Hedgehog Spinball (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Streets of Rage (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Streets of Rage II (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Space Gun (Euro)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Speedball (Image Works) (Euro)\0", NULL, "Image Works", "Sega Mastersystem",
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
	"Speedball 2 (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Speedball (Virgin) (Euro, USA)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"SpellCaster (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Spider-Man - Return of the Sinister Six (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Spider-Man vs. The Kingpin (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sports Pad Football (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Sports Pad Soccer (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Spy vs. Spy (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Spy vs Spy (Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Spy vs. Spy (Jpn, Pirate?)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Spy vs Spy (Tw)\0", NULL, "Aaronix", "Sega Mastersystem",
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
	"Spy vs. Spy (USA, Display Unit Sample)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Space Invaders (Euro)\0", NULL, "Domark", "Sega Mastersystem",
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
	"Smash T.V. (Euro)\0", NULL, "Flying Edge", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Star Wars (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Street Master (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Strider (Euro, USA, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Strider (USA, Display Unit Sample)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Strider II (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Submarine Attack (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Suho Jeonsa (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Sukeban Deka II - Shoujo Tekkamen Densetsu (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Summer Games (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Summer Games (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Superman - The Man of Steel (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Super Arkanoid (Kor)\0", NULL, "HiCom", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Super Basketball (USA, CES Demo)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Super Bioman 1 (Kor)\0", NULL, "HiCom", "Sega Mastersystem",
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
	"Super Boy I (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
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
	"Super Boy II (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Super Boy 3 (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Super Boy 4 (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
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
	"Super Kick Off (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Super Off Road (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"Super Racing (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Tennis (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"T2 - The Arcade Game (Euro)\0", NULL, "Arena", "Sega Mastersystem",
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
	"Taz-Mania (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Taz-Mania (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Taz in Escape from Mars (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Tecmo World Cup '92 (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Tecmo World Cup '93 (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Teddy Boy (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Teddy Boy Blues (Jpn, Pirate?)\0", NULL, "pirate", "Sega Mastersystem",
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
	"Tennis Ace (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Tensai Bakabon (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Terminator 2 - Judgment Day (Euro)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"The Terminator (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"The Terminator (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Thunder Blade (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Thunder Blade (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Time Soldiers (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"The New Zealand Story (Euro)\0", NULL, "TecMagik", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Tom and Jerry - The Movie (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Tom and Jerry (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Toto World 3 (Kor)\0", NULL, "Open Corp.", "Sega Mastersystem",
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
	"TransBot (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"TransBot (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Treinamento Do Mymo (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Trivial Pursuit - Genus Edition (Euro)\0", NULL, "Domark", "Sega Mastersystem",
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
	"Ttoriui Moheom (Kor)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Turma da Monica em O Resgate (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"As Aventuras da TV Colosso (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Ultima IV - Quest of the Avatar (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ultima IV - Quest of the Avatar (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ultimate Soccer (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Vampire (Euro, Prototype)\0", NULL, "Unknown", "Sega Mastersystem",
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
	"Virtua Fighter Animation (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Vigilante (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Walter Payton Football (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wanted (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy (Euro, USA, Bra, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy (Euro, Kor, v0) ~ Super Wonder Boy (Jpn, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy III - The Dragon's Trap (Euro, USA, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy in Monster Land (Euro, USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy in Monster Land (Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Wonder Boy - Monster World (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy in Monster Land (Euro, USA, Hacked?)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy in Monster World (Euro, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonder Boy in Monster World (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wonsiin (Kor)\0", NULL, "Zemina", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"World Class Leader Board (Euro, Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"World Cup Italia '90 (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"World Cup Italia '90 (USA, Demo)\0", NULL, "Sega", "Sega Mastersystem",
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
	"World Cup USA 94 (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wimbledon II (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Wimbledon (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Winter Olympics - Lillehammer '94 (Euro)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Winter Olympics - Lillehammer '94 (Bra)\0", NULL, "U.S. Gold", "Sega Mastersystem",
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
	"Wolfchild (Euro, Bra)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
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
	"World Games (Euro, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"World Games (Euro, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"World Grand Prix (Euro)\0", NULL, "Sega", "Sega Mastersystem",
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
	"World Grand Prix (USA, Bra, Kor)\0", NULL, "Sega", "Sega Mastersystem",
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
	"World Grand Prix (USA, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"World Soccer (Euro, Jpn, Kor) ~ Great Soccer (USA)\0", NULL, "Sega", "Sega Mastersystem",
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
	"WWF Wrestlemania - Steel Cage Challenge (Euro, Bra)\0", NULL, "Flying Edge", "Sega Mastersystem",
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
	"Where in the World is Carmen Sandiego? (USA)\0", NULL, "Parker Brothers", "Sega Mastersystem",
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
	"Where in the World is Carmen Sandiego? (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Xenon 2 - Megablast (Image Works) (Euro)\0", NULL, "Image Works", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Xenon 2 - Megablast (Virgin) (Euro)\0", NULL, "Virgin Interactive", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"X-Men - Mojo World (Bra)\0", NULL, "Tec Toy", "Sega Mastersystem",
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
	"Xyzolog (Kor)\0", NULL, "Taito?", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
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
	"Ys (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ys - The Vanished Omens (Euro, USA, Bra)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ys (USA, Demo)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Zaxxon 3-D (World)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Zaxxon 3-D (World, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Zillion (Euro, v2)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Zillion (Euro, v0) ~ Akai Koudan Zillion (Jpn, v0)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Zillion (USA, v1)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Zillion II - The Tri Formation (Euro, USA) ~ Tri Formation (Jpn)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Zool - Ninja of the 'Nth' Dimension (Euro)\0", NULL, "Gremlin Interactive", "Sega Mastersystem",
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
	"The A-Team (Music prototype)\0", NULL, "Probe", "Sega Mastersystem",
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
	"Lethal Weapon 3 (Music prototype)\0", NULL, "Probe", "Sega Mastersystem",
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
	"Astro Flash (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Bank Panic (Euro, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Comical Machine Gun Joe (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"F-16 Fighter (Euro, USA, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"F-16 Fighting Falcon (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"F-16 Fighting Falcon (USA, Sega Card for Display Unit)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Fushigi no Oshiro Pit Pot (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ghost House (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ghost House (Sega Card, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Ghost House (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Baseball (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Soccer (Euro, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Soccer (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Great Tennis (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hang-On (Euro, Bra, Aus, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Hang-On (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"My Hero (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Satellite 7 (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Seishun Scandal (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Spy vs. Spy (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Spy vs Spy (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Super Tennis (Euro, USA, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Teddy Boy (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Teddy Boy Blues (Jpn, Ep-MyCard, Prototype)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Teddy Boy Blues (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
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
	"TransBot (Euro, USA, Bra, Sega Card)\0", NULL, "Sega", "Sega Mastersystem",
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
	"Woody Pop - Shinjinrui no Block Kuzushi (Jpn, MyCard)\0", NULL, "Sega", "Sega Mastersystem",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName, sms_woodypopRomInfo, sms_woodypopRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan, &SMSPaletteRecalc, 0x1000,
	256, 192, 4, 3
};


