// FB Alpha Crazy Balloon driver module
// Based on MAME driver by Nicola Salmoria

#include "tiles_generic.h"
#include "z80_intf.h"
#include "sn76477.h"
#include <math.h>

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvZ80RAM;
static UINT8 *DrvVidRAM;
static UINT8 *DrvColRAM;
static UINT8 *DrvSprRAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 *pc3092_data;
static UINT8 flipscreen;
static UINT8 irq_mask;
static INT32 sound_enable;
static UINT8 last_snd;

static UINT16 collision_address;
static INT32 collision_address_clear;

static UINT32 crbaloon_tone_step;
static UINT32 crbaloon_tone_pos;
static double crbaloon_tone_freq;
static double envelope_ctr;
static UINT32 sound_data08;

static INT32 sound_laugh;
static INT32 sound_laugh_trig;
static INT32 sound_appear_trig;
static INT32 sound_appear;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[2];
static UINT8 DrvReset;

static struct BurnInputInfo CrbaloonInputList[] = {
	{"P1 Coin",			BIT_DIGITAL,	DrvJoy2 + 6,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 start"	},
	{"P1 Up",			BIT_DIGITAL,	DrvJoy1 + 0,	"p1 up"		},
	{"P1 Down",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 down"	},
	{"P1 Left",			BIT_DIGITAL,	DrvJoy1 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy1 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 fire 1"	},

	{"P2 Start",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 start"	},
	{"P2 Up",			BIT_DIGITAL,	DrvJoy1 + 4,	"p2 up"		},
	{"P2 Down",			BIT_DIGITAL,	DrvJoy1 + 5,	"p2 down"	},
	{"P2 Left",			BIT_DIGITAL,	DrvJoy1 + 6,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy1 + 7,	"p2 right"	},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"		},
	{"Service",			BIT_DIGITAL,	DrvJoy2 + 2,	"service"	},
	{"Tilt",			BIT_DIGITAL,	DrvJoy2 + 3,	"tilt"		},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};	

STDINPUTINFO(Crbaloon)

static struct BurnDIPInfo CrbaloonDIPList[]=
{
	{0x0f, 0xff, 0xff, 0x87, NULL					},
	{0x10, 0xff, 0xff, 0xf0, NULL					},

	{0   , 0xfe, 0   ,    2, "Test?"				},
	{0x0f, 0x01, 0x01, 0x01, "I/O Check?"			},
	{0x0f, 0x01, 0x01, 0x00, "RAM Check?"			},

	{0   , 0xfe, 0   ,    2, "Cabinet"				},
	{0x0f, 0x01, 0x02, 0x02, "Upright"				},
	{0x0f, 0x01, 0x02, 0x00, "Cocktail"				},

	{0   , 0xfe, 0   ,    4, "Lives"				},
	{0x0f, 0x01, 0x0c, 0x00, "2"					},
	{0x0f, 0x01, 0x0c, 0x04, "3"					},
	{0x0f, 0x01, 0x0c, 0x08, "4"					},
	{0x0f, 0x01, 0x0c, 0x0c, "5"					},

	{0   , 0xfe, 0   ,    2, "Bonus Life"			},
	{0x0f, 0x01, 0x10, 0x00, "5000"					},
	{0x0f, 0x01, 0x10, 0x10, "10000"				},

	{0   , 0xfe, 0   ,    8, "Coinage"				},
	{0x0f, 0x01, 0xe0, 0x20, "4 Coins 1 Credits"	},
	{0x0f, 0x01, 0xe0, 0x40, "3 Coins 1 Credits"	},
	{0x0f, 0x01, 0xe0, 0x60, "2 Coins 1 Credits"	},
	{0x0f, 0x01, 0xe0, 0x80, "1 Coin  1 Credits"	},
	{0x0f, 0x01, 0xe0, 0xa0, "1 Coin  2 Credits"	},
	{0x0f, 0x01, 0xe0, 0xc0, "1 Coin  3 Credits"	},
	{0x0f, 0x01, 0xe0, 0xe0, "1 Coin  4 Credits"	},
	{0x0f, 0x01, 0xe0, 0x00, "Disable"				},

	{0   , 0xfe, 0   ,    2, "Invulnerability"		},
	{0x10, 0x01, 0x10, 0x10, "Off"					},
	{0x10, 0x01, 0x10, 0x00, "On"					},
};

STDDIPINFO(Crbaloon)

static void sound_tone_render(INT16 *buffer, INT32 len)
{
	INT16 square = 0;

	memset(buffer, 0, len * 2 * sizeof(INT16));

	if (crbaloon_tone_step)	{
		while (len-- > 0) {
			square = crbaloon_tone_pos & 0x80000000 ? 32767 : -32768;
			square = (INT16)((double)square * 0.05);        // volume
			square = (INT16)(square * exp(-envelope_ctr));  // envelope volume
			envelope_ctr += (crbaloon_tone_freq > 1100.0) ? 0.0008 : 0.0005; // step envelope, treat the higher pitched sounds w/ a faster envelope
			*buffer++ = square;
			*buffer++ = square;

			crbaloon_tone_pos += crbaloon_tone_step;
		}
	}
}

static void sound_tone_write(UINT8 data)
{
	crbaloon_tone_step = 0;
	envelope_ctr = 0.0;

	if (data && data != 0xff) {
		double freq = (13630.0 / (256 - data) + (data >= 0xea ? 13 : 0)) * 0.5;

		crbaloon_tone_freq = freq;
		crbaloon_tone_step = (UINT32)(freq * 65536.0 * 65536.0 / (double)nBurnSoundRate);
	}
}

static void sound_tone_write_hi(UINT8 data)
{
	crbaloon_tone_step = 0;
	envelope_ctr = 0.0;

	if (data && data != 0xff) {
		double freq = (640630.0 / (256 - data) + (data >= 0xea ? 13 : 0)) * 0.5;

		crbaloon_tone_freq = freq;
		crbaloon_tone_step = (UINT32)(freq * 65536.0 * 65536.0 / (double)nBurnSoundRate);
	}
}

static void sound_port_write(UINT8 data)
{
	irq_mask = data & 0x01;
	collision_address_clear = (data & 1) ? 0 : 1;

	sound_enable = data & 0x02;

	if (sound_enable) {
		if (data & 0x20) { // appear (face appears, bird tweet noise)
			sound_appear_trig = 2;
		}

		if (sound_appear_trig) {
			sound_appear_trig--;
			sound_appear++;
			UINT16 tone = (0xf0 + ((sound_appear % 5) * 40 + 6)) & 0xff;
			sound_tone_write_hi(tone);
			if (sound_appear > 20 || sound_appear_trig == 0) sound_tone_write_hi(0);
		} else {
			sound_appear = 0;
		}

		if (data & 0x40) { // laugh
			sound_laugh_trig = 3;
		}

		if (sound_laugh_trig) {
			sound_laugh_trig--;
			sound_laugh++;
			sound_tone_write(100 + ((sound_laugh % 7) * 20 + (rand() % 6)));
			if (sound_laugh > 90 || sound_laugh_trig == 0) sound_tone_write(0);
		} else {
			sound_laugh = 0;
		}

		// filter dupes for the blowing air guy
		if (((data & 0x38) == 0x10) && ((last_snd & 0x38) == 0x30)) return;
		if (((data & 0x38) == 0x30) && ((last_snd & 0x38) == 0x30)) return;
		if (((data & 0x38) == 0x10) && ((last_snd & 0x38) == 0x10)) return;

		// so the balloon makes a slightly different dieing sound when killed by the blowing air guy
		if (data & 0x08) {
			data = 0x08;
			sound_data08 = 8;
		}

		if (sound_data08) {
			sound_data08--;
			data &= ~0x30;
		}

		//bprintf(0, _T("data[%X]last[%X]. "), data, last_snd);
		//bprintf(0, _T("%S %S %S %S,  "), data&0x08 ? "0x08" : "", data&0x10 ? "0x10" : "", data&0x20 ? "0x20" : "", data&0x40 ? "0x40" : "");
		SN76477_set_slf_res(0,  ((data & 0x10)) ? RES_K(10) : RES_K(20));
		SN76477_mixer_b_w(0,    (~data & 0x10)  ? 0 : 1 );
		SN76477_mixer_c_w(0,    ((data & 0x10)) ? 0 : 1);
		if (data & 0x10) SN76477_enable_w(0, 1);
		SN76477_envelope_w(0, 1);
		SN76477_enable_w(0,      (data & 0x08)  ? 1 : 0);
		last_snd = data;
	}
}

static void __fastcall crbaloon_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xf)
	{
		case 0x00: // ?
		case 0x01: // watchdog
		return;

		case 0x02:
		case 0x03:
		case 0x04:
			DrvSprRAM[(port - 0x02) & 3] = data;
		return;

		case 0x05:
			sound_tone_write(data);
		return;

		case 0x06:
			sound_port_write(data);
		return;

		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			pc3092_data[(port & 0xf) - 7] = data & 0x0f;
			flipscreen = pc3092_data[1] & 0x01;
		return;

		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
		return;
	}
}

static UINT8 pc3259_read(UINT8 offset)
{
	UINT16 address = collision_address_clear ? 0xffff : collision_address;
	INT32 collided = (address != 0xffff);

	switch (offset >> 2)
	{
		case 0x00:
			return collided ? (address & 0x0f) : 0;

		case 0x01:
			return collided ? ((address >> 4) & 0x0f) : 0;

		case 0x02:;
			return collided ? (address >> 8) : 0;

		default:
		case 0x03:
			return collided ? 0x08 : 0x07;
	}

	return 0;
}

static UINT8 __fastcall crbaloon_read_port(UINT16 port)
{
	switch (port & 0x3)
	{
		case 0x00:
			return DrvDips[0];

		case 0x01:
			return DrvInputs[0];

		case 0x02:
			return (DrvDips[1] & 0xf0) | pc3259_read(port);

		case 0x03:
			if (pc3092_data[1] & 0x02) return DrvInputs[1];
			return DrvInputs[1] & 0xf;
	}

	return 0;
}

static tilemap_callback( bg )
{
	TILE_SET_INFO(0, DrvVidRAM[offs ^ 0x3ff], DrvColRAM[offs ^ 0x3ff], 0);
}

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ZetOpen(0);
	//SN76477_reset(0); reset rarely needed with sn76477, as it clears certain needed vars.
    SN76477_enable_w(0, 1);
    SN76477_enable_w(0, 0);
	sound_port_write(0);
	ZetReset();
	ZetClose();

	irq_mask = 0;
	flipscreen = 0;
	collision_address_clear = 1;
	collision_address = 0;
	sound_enable = 0;
	last_snd = 0;

	sound_laugh = 0;
	sound_laugh_trig = 0;
	sound_data08 = 0;
	envelope_ctr = 0;

	crbaloon_tone_step = 0;
	crbaloon_tone_pos = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM		= Next; Next += 0x004000;

	DrvGfxROM0		= Next; Next += 0x004000;
	DrvGfxROM1		= Next; Next += 0x000800;

	DrvPalette		= (UINT32*)Next; Next += 0x0020 * sizeof(UINT32);

	AllRam			= Next;

	DrvZ80RAM		= Next; Next += 0x000400;
	DrvVidRAM		= Next; Next += 0x000400;
	DrvColRAM		= Next; Next += 0x000400;
	DrvSprRAM		= Next; Next += 0x000003;
	pc3092_data		= Next; Next += 0x000005;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvGfxDecode()
{
	INT32 Plane[1] = { 0 };
	INT32 XOffs[8] = { STEP8(0,1) };
	INT32 YOffs[8] = { STEP8((7*8),-8) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x800);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x800);

	GfxDecode(0x100, 1, 8, 8, Plane, XOffs, YOffs, 0x040, tmp, DrvGfxROM0);

	BurnFree(tmp);

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

	{
		if (BurnLoadRom(DrvZ80ROM  + 0x0000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x0800,  1, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x1800,  3, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2000,  4, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM  + 0x2800,  5, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x0000,  6, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x0000,  7, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	for (INT32 i = 0; i < 0x10000; i+= 0x8000)
	{
		ZetMapMemory(DrvZ80ROM,			0x0000 + i, 0x3fff + i, MAP_ROM);

		for (INT32 j = 0; j < 0x800; j += 0x0400)
		{
			ZetMapMemory(DrvZ80RAM,			0x4000 + i + j, 0x43ff + i + j, MAP_RAM);
			ZetMapMemory(DrvVidRAM,			0x4800 + i + j, 0x4bff + i + j, MAP_RAM);
			ZetMapMemory(DrvColRAM,			0x5000 + i + j, 0x53ff + i + j, MAP_RAM);
		}
	}
	ZetSetOutHandler(crbaloon_write_port);
	ZetSetInHandler(crbaloon_read_port);
	ZetClose();

	SN76477_init(0);
	SN76477_set_mastervol(0, 4.00); // first! (all params below depend on this being set first)
	SN76477_set_noise_res(0, RES_K(47));
	SN76477_set_filter_res(0, RES_K(330));
	SN76477_set_filter_cap(0, CAP_P(470));
	SN76477_set_decay_res(0, RES_K(220));
	SN76477_set_attack_decay_cap(0,CAP_U(1.0));
	SN76477_set_attack_res(0, RES_K(4.7));
	SN76477_set_amplitude_res(0, RES_M(1));
	SN76477_set_feedback_res(0, RES_K(200));
	SN76477_set_vco_res(0, RES_K(330));
	SN76477_set_vco_cap(0, CAP_P(470));
	SN76477_set_vco_voltage(0, 5.0);
	SN76477_set_pitch_voltage(0, 5.0);
	SN76477_set_slf_res(0, RES_K(20));
	SN76477_set_slf_cap(0, CAP_P(420));
	SN76477_set_oneshot_res(0, RES_K(47));
	SN76477_set_oneshot_cap(0, CAP_U(1.0));
	SN76477_set_mixer_params(0, 0, 0, 1);
	SN76477_envelope_w(0, 1);

	SN76477_enable_w(0, 0);

	GenericTilesInit();
	GenericTilemapInit(0, TILEMAP_SCAN_ROWS, bg_map_callback, 8, 8, 32, 32);
	GenericTilemapSetGfx(0, DrvGfxROM0, 1, 8, 8, 0x4000, 0, 0xf);

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	ZetExit();
	SN76477_exit(0);

	BurnFree(AllMem);

	return 0;
}

static void DrvPaletteInit()
{
	for (INT32 i = 0; i < 32; i++)
	{
		UINT8 pen = 0xf;
		if (i & 0x01) pen = i >> 1;

		UINT8 h = (~pen & 0x08) ? 0xff : 0x55;
		UINT8 r = h * ((~pen >> 0) & 1);
		UINT8 g = h * ((~pen >> 1) & 1);
		UINT8 b = h * ((~pen >> 2) & 1);

		DrvPalette[i] = BurnHighCol(r,g,b,0);
	}
}

static void draw_sprites()
{
	UINT8 code  = DrvSprRAM[0] & 0x0f;
	UINT8 color = DrvSprRAM[0] >> 4;
	UINT8 sy    = DrvSprRAM[2] - 32;

	UINT8 *gfx = DrvGfxROM1 + (code << 7);

	if (flipscreen)
		sy += 32;

	collision_address = 0xffff;

	for (INT32 y = 0x1f; y >= 0; y--, sy++)
	{
		UINT8 sx  = DrvSprRAM[1];
		UINT8 data = 0;

		for (INT32 x = 0x1f; x >= 0; x--, sx++, data<<=1)
		{
			if ((x & 0x07) == 0x07)
				data = (sy >= 0xe0) ? 0 : gfx[((x >> 3) << 5) | y];

			INT32 bit = data & 0x80;

			if (bit)
			{
				if (sy < nScreenHeight && sx < nScreenWidth)
				{
					if (pTransDraw[(sy * nScreenWidth) + sx] & 1)
						collision_address = ((((sy ^ 0xff) >> 3) << 5) | ((sx ^ 0xff) >> 3)) + 1;

					pTransDraw[(sy * nScreenWidth) + sx] = (color << 1) | 1;
				}
			}
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPaletteInit();
		DrvRecalc = 0;
	}

	GenericTilemapDraw(0, pTransDraw, 0);

	draw_sprites();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0xff;
		DrvInputs[1] = 0x3f;

		for (INT32 i = 0; i < 8; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	ZetOpen(0);
	ZetRun(3329000 / 60);
	if (irq_mask) ZetSetIRQLine(0, CPU_IRQSTATUS_HOLD);
	ZetClose();

	if (pBurnSoundOut) {
		sound_tone_render(pBurnSoundOut, nBurnSoundLen);
		SN76477_sound_update(0, pBurnSoundOut, nBurnSoundLen);

		if (!sound_enable)
			memset (pBurnSoundOut, 0, nBurnSoundLen * 2 * sizeof(INT16));
	}

	if (pBurnDraw) {
		DrvDraw();
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

		SCAN_VAR(flipscreen);
		SCAN_VAR(irq_mask);
		SCAN_VAR(collision_address);
		SCAN_VAR(collision_address_clear);
		SCAN_VAR(crbaloon_tone_step);
		SCAN_VAR(crbaloon_tone_pos);
		SCAN_VAR(crbaloon_tone_freq);
		SCAN_VAR(sound_enable);
		SCAN_VAR(last_snd);
		SCAN_VAR(sound_laugh_trig);
		SCAN_VAR(sound_laugh);
		SCAN_VAR(sound_appear_trig);
		SCAN_VAR(sound_appear);
		SCAN_VAR(envelope_ctr);
		SCAN_VAR(sound_data08);
	}

	return 0;
}


// Crazy Balloon (set 1)

static struct BurnRomInfo crbaloonRomDesc[] = {
	{ "cl01.bin",		0x0800, 0x9d4eef0b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "cl02.bin",		0x0800, 0x10f7a6f7, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "cl03.bin",		0x0800, 0x44ed6030, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cl04.bin",		0x0800, 0x62f66f6c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cl05.bin",		0x0800, 0xc8f1e2be, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "cl06.bin",		0x0800, 0x7d465691, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "cl07.bin",		0x0800, 0x2c1fbea8, 2 | BRF_GRA },           //  6 Background Tiles

	{ "cl08.bin",		0x0800, 0xba898659, 3 | BRF_GRA },           //  7 Sprite data
};

STD_ROM_PICK(crbaloon)
STD_ROM_FN(crbaloon)

struct BurnDriver BurnDrvCrbaloon = {
	"crbaloon", NULL, NULL, NULL, "1980",
	"Crazy Balloon (set 1)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_MAZE | GBF_ACTION, 0,
	NULL, crbaloonRomInfo, crbaloonRomName, NULL, NULL, NULL, NULL, CrbaloonInputInfo, CrbaloonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 32,
	224, 256, 3, 4
};


// Crazy Balloon (set 2)

static struct BurnRomInfo crbaloon2RomDesc[] = {
	{ "cl01.bin",		0x0800, 0x9d4eef0b, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 Code
	{ "crazybal.ep2",	0x0800, 0x87572086, 1 | BRF_PRG | BRF_ESS }, //  1
	{ "crazybal.ep3",	0x0800, 0x575fe995, 1 | BRF_PRG | BRF_ESS }, //  2
	{ "cl04.bin",		0x0800, 0x62f66f6c, 1 | BRF_PRG | BRF_ESS }, //  3
	{ "cl05.bin",		0x0800, 0xc8f1e2be, 1 | BRF_PRG | BRF_ESS }, //  4
	{ "crazybal.ep6",	0x0800, 0xfed6ff5c, 1 | BRF_PRG | BRF_ESS }, //  5

	{ "cl07.bin",		0x0800, 0x2c1fbea8, 2 | BRF_GRA },           //  6 Background Tiles

	{ "cl08.bin",		0x0800, 0xba898659, 3 | BRF_GRA },           //  7 Sprite data
};

STD_ROM_PICK(crbaloon2)
STD_ROM_FN(crbaloon2)

struct BurnDriver BurnDrvCrbaloon2 = {
	"crbaloon2", "crbaloon", NULL, NULL, "1980",
	"Crazy Balloon (set 2)\0", NULL, "Taito Corporation", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE | BDF_ORIENTATION_VERTICAL | BDF_ORIENTATION_FLIPPED, 2, HARDWARE_PREFIX_TAITO, GBF_MAZE | GBF_ACTION, 0,
	NULL, crbaloon2RomInfo, crbaloon2RomName, NULL, NULL, NULL, NULL, CrbaloonInputInfo, CrbaloonDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 32,
	224, 256, 3, 4
};
