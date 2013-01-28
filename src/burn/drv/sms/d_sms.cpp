#include "burnint.h"
#include "sms.h"


static struct BurnDIPInfo SMSDIPList[] = {
	{0x3d, 0xff, 0xff, 0x00, NULL				},
	{0x3e, 0xff, 0xff, 0x00, NULL				},
	{0x3f, 0xff, 0xff, 0x01, NULL				},

};

STDDIPINFO(SMS)

static struct BurnInputInfo SMSInputList[] = {
	{"P1 Start",		BIT_DIGITAL,	SMSJoy1 + 3,	"p1 start"	}, // 0
	{"P1 Select",		BIT_DIGITAL,	SMSJoy1 + 2,	"p1 select"	},
	{"P1 Up",		BIT_DIGITAL,	SMSJoy1 + 4,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	SMSJoy1 + 6,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	SMSJoy1 + 7,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	SMSJoy1 + 5,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	SMSJoy1 + 1,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	SMSJoy1 + 0,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	SMSJoy1 + 8,	"p1 fire 3"	},
	{"P1 Button 4",		BIT_DIGITAL,	SMSJoy1 + 9,	"p1 fire 4"	},
	{"P1 Button 5",		BIT_DIGITAL,	SMSJoy1 + 10,	"p1 fire 5"	},
	{"P1 Button 6",		BIT_DIGITAL,	SMSJoy1 + 11,	"p1 fire 6"	},

};

STDINPUTINFO(SMS)

static struct BurnRomInfo sms_akmwRomDesc[] = {
	{ "sms_akmw.sms", 0x20000, 0xAED9AAC4, BRF_PRG | BRF_ESS },
};

STD_ROM_PICK(sms_akmw)
STD_ROM_FN(sms_akmw)

struct BurnDriverD BurnDrvsms_akmw = {
	"sms_akmw", NULL, NULL, NULL, "1990",
	"Alex Kidd\0", NULL, "SEGA", "Master System",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 5, HARDWARE_SEGA_MASTER_SYSTEM, GBF_MISC, 0,
	SMSGetZipName,  sms_akmwRomInfo, sms_akmwRomName, NULL, NULL, SMSInputInfo, SMSDIPInfo,
	SMSInit, SMSExit, SMSFrame, SMSDraw, SMSScan,
	&SMSPaletteRecalc, 0x400, 512, 240, 4, 3
};


