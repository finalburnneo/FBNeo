// FB Alpha DJ Boy driver module
// Based on MAME driver by (Phil Bennette?)

#include "tiles_generic.h"
#include "z80_intf.h"
#include "msm6295.h"
#include "burn_ym2203.h"
#include "pandora.h"

static UINT8 *AllMem;
static UINT8 *MemEnd;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *DrvZ80ROM0;
static UINT8 *DrvZ80ROM1;
static UINT8 *DrvZ80ROM2;
static UINT8 *DrvGfxROM0;
static UINT8 *DrvGfxROM1;
static UINT8 *DrvSndROM0;
static UINT8 *DrvSndROM1;
static UINT8 *DrvVidRAM;
static UINT8 *DrvShareRAM0;
static UINT8 *DrvSprRAM;
static UINT8 *DrvPandoraRAM;
static UINT8 *DrvPalRAM;
static UINT8 *DrvZ80RAM2;

static UINT8 *soundlatch;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static UINT8 bankxor;
static INT32 watchdog;

static UINT8 nBankAddress0;
static UINT8 nBankAddress1;
static UINT8 nBankAddress2;

static INT32 videoreg = 0;
static UINT8 scrollx = 0;
static UINT8 scrolly = 0;

static UINT8 DrvJoy1[8];
static UINT8 DrvJoy2[8];
static UINT8 DrvJoy3[8];
static UINT8 DrvDips[2];
static UINT8 DrvInputs[3];
static UINT8 DrvReset;

static struct BurnInputInfo DjboyInputList[] = {
	{"P1 Coin",		BIT_DIGITAL,	DrvJoy1 + 2,	"p1 coin"	},
	{"P1 Start",		BIT_DIGITAL,	DrvJoy1 + 0,	"p1 start"	},
	{"P1 Up",		BIT_DIGITAL,	DrvJoy2 + 0,	"p1 up"		},
	{"P1 Down",		BIT_DIGITAL,	DrvJoy2 + 1,	"p1 down"	},
	{"P1 Left",		BIT_DIGITAL,	DrvJoy2 + 2,	"p1 left"	},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 3,	"p1 right"	},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 4,	"p1 fire 1"	},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 5,	"p1 fire 2"	},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 6,	"p1 fire 3"	},

	{"P2 Coin",		BIT_DIGITAL,	DrvJoy1 + 3,	"p2 coin"	},
	{"P2 Start",		BIT_DIGITAL,	DrvJoy1 + 1,	"p2 start"	},
	{"P2 Up",		BIT_DIGITAL,	DrvJoy3 + 0,	"p2 up"}	,
	{"P2 Down",		BIT_DIGITAL,	DrvJoy3 + 1,	"p2 down"	},
	{"P2 Left",		BIT_DIGITAL,	DrvJoy3 + 2,	"p2 left"	},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy3 + 3,	"p2 right"	},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy3 + 4,	"p2 fire 1"	},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy3 + 5,	"p2 fire 2"	},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy3 + 6,	"p2 fire 3"	},

	{"Reset",		BIT_DIGITAL,	&DrvReset,	"reset"		},
	{"Tilt",		BIT_DIGITAL,	DrvJoy1 + 5,	"tilt"		},
	{"Dip A",		BIT_DIPSWITCH,	DrvDips + 0,	"dip"		},
	{"Dip B",		BIT_DIPSWITCH,	DrvDips + 1,	"dip"		},
};

STDINPUTINFO(Djboy)

static struct BurnDIPInfo DjboyDIPList[]=
{
	{0x14, 0xff, 0xff, 0x00, NULL				},
	{0x15, 0xff, 0xff, 0x80, NULL				},

	{0   , 0xfe, 0   ,    2, "Flip Screen"			},
	{0x14, 0x01, 0x02, 0x00, "Off"				},
	{0x14, 0x01, 0x02, 0x02, "On"				},

	{0   , 0xfe, 0   ,    2, "Service Mode"			},
	{0x14, 0x01, 0x04, 0x00, "Off"				},
	{0x14, 0x01, 0x04, 0x04, "On"				},

	{0   , 0xfe, 0   ,    4, "Coin A"			},
	{0x14, 0x01, 0x30, 0x20, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0x30, 0x00, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0x30, 0x30, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0x30, 0x10, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Coin B"			},
	{0x14, 0x01, 0xc0, 0x80, "2 Coins 1 Credits"		},
	{0x14, 0x01, 0xc0, 0x00, "1 Coin  1 Credits"		},
	{0x14, 0x01, 0xc0, 0xc0, "2 Coins 3 Credits"		},
	{0x14, 0x01, 0xc0, 0x40, "1 Coin  2 Credits"		},

	{0   , 0xfe, 0   ,    4, "Difficulty"			},
	{0x15, 0x01, 0x03, 0x01, "Easy"				},
	{0x15, 0x01, 0x03, 0x00, "Normal"			},
	{0x15, 0x01, 0x03, 0x02, "Hard"				},
	{0x15, 0x01, 0x03, 0x03, "Hardest"			},

	{0   , 0xfe, 0   ,    4, "Bonus Levels (in thousands)"	},
	{0x15, 0x01, 0x0c, 0x00, "10,30,50,70,90"		},
	{0x15, 0x01, 0x0c, 0x04, "10,20,30,40,50,60,70,80,90"	},
	{0x15, 0x01, 0x0c, 0x08, "20,50"			},
	{0x15, 0x01, 0x0c, 0x0c, "None"				},

	{0   , 0xfe, 0   ,    4, "Lives"			},
	{0x15, 0x01, 0x30, 0x10, "3"				},
	{0x15, 0x01, 0x30, 0x00, "5"				},
	{0x15, 0x01, 0x30, 0x20, "7"				},
	{0x15, 0x01, 0x30, 0x30, "9"				},

	{0   , 0xfe, 0   ,    2, "Demo Sounds"			},
	{0x15, 0x01, 0x40, 0x40, "Off"				},
	{0x15, 0x01, 0x40, 0x00, "On"				},

	{0   , 0xfe, 0   ,    2, "Stereo Sound"			},
	{0x15, 0x01, 0x80, 0x80, "Off"				},
	{0x15, 0x01, 0x80, 0x00, "On"				},
};

STDDIPINFO(Djboy)

//---------------------------------------------------------------------------------------------------
// Kaneko Beast simulation - ripped from MAME

enum
{
	 eDJBOY_ATTRACT_HIGHSCORE = 0,
	 eDJBOY_ATTRACT_TITLE,
	 eDJBOY_ATTRACT_GAMEPLAY,
	 eDJBOY_PRESS_P1_START,
	 eDJBOY_PRESS_P1_OR_P2_START,
	 eDJBOY_ACTIVE_GAMEPLAY
};

enum
{
	 ePROT_NORMAL = 0,
	 ePROT_WRITE_BYTES,
	 ePROT_WRITE_BYTE,
	 ePROT_READ_BYTES,
	 ePROT_WAIT_DSW1_WRITEBACK,
	 ePROT_WAIT_DSW2_WRITEBACK,
	 ePROT_STORE_PARAM
};

static INT32 prot_busy_count;
static UINT8 prot_output_buffer[8];
static INT32 prot_available_data_count;
static INT32 prot_offs; // internal state
static UINT8 prot_ram[0x80]; // internal RAM
static UINT8 prot_param[8];
static INT32 coin;
static INT32 complete;
static INT32 lives[2];
static INT32 mDjBoyState;
static INT32 prot_mode;

static void ProtectionOut(INT32 i, UINT8 data)
{
	if (prot_available_data_count == i)
		prot_output_buffer[prot_available_data_count++] = data;
}

static INT32 GetLives()
{
	switch (DrvDips[1] & 0x30)
	{
		case 0x10: return 3;
		case 0x00: return 5;
		case 0x20: return 7;
		case 0x30: return 9;
	}

	return 0;
}

static void coinplus_w(UINT8 data)
{
	INT32 dsw = DrvDips[0];

	if (data & 1)
	{
		switch ((dsw & 0x30) >> 4)
		{
		case 0: coin += 4; break; // 1 coin, 1 credit
		case 1: coin += 8; break; // 1 coin, 2 credits
		case 2: coin += 2; break; // 2 coins, 1 credit
		case 3: coin += 6; break; // 2 coins, 3 credits
		}
	}
	if (data & 2)
	{
		switch ((dsw & 0xc0) >> 6)
		{
		case 0: coin += 4; break; // 1 coin, 1 credit
		case 1: coin += 8; break; // 1 coin, 2 credits
		case 2: coin += 2; break; // 2 coins, 1 credit
		case 3: coin += 6; break; // 2 coins, 3 credits
		}
	}
}

static void OutputProtectionState(INT32 i, INT32)
{
	INT32 io = ~DrvInputs[0];
	INT32 dat = 0x00;

	switch (mDjBoyState)
	{
	case eDJBOY_ATTRACT_HIGHSCORE:
		if (coin >= 4)
		{
			dat = 0x01;
			mDjBoyState = eDJBOY_PRESS_P1_START;
		}
		else if (complete)
		{
			dat = 0x06;
			mDjBoyState = eDJBOY_ATTRACT_TITLE;
		}
		break;

	case eDJBOY_ATTRACT_TITLE:
		if (coin >= 4)
		{
			dat = 0x01;
			mDjBoyState = eDJBOY_PRESS_P1_START;
		}
		else if (complete)
		{
			dat = 0x15;
			mDjBoyState = eDJBOY_ATTRACT_GAMEPLAY;
		}
		break;

	case eDJBOY_ATTRACT_GAMEPLAY:
		if (coin>=4)
		{
			dat = 0x01;
			mDjBoyState = eDJBOY_PRESS_P1_START;
		}
		else if (complete)
		{
			dat = 0x0b;
			mDjBoyState = eDJBOY_ATTRACT_HIGHSCORE;
		}
		break;

	case eDJBOY_PRESS_P1_START:
		if (io & 1) // p1 start
		{
			dat = 0x16;
			mDjBoyState = eDJBOY_ACTIVE_GAMEPLAY;
		}
		else if (coin >= 8)
		{
			dat = 0x05;
			mDjBoyState = eDJBOY_PRESS_P1_OR_P2_START;
		}
		break;

	case eDJBOY_PRESS_P1_OR_P2_START:
		if (io & 1) // p1 start
		{
			dat = 0x16;
			mDjBoyState = eDJBOY_ACTIVE_GAMEPLAY;
			lives[0] = GetLives();
			coin -= 4;
		}
		else if (io & 2) // p2 start
		{
			dat = 0x0a;
			mDjBoyState = eDJBOY_ACTIVE_GAMEPLAY;
			lives[0] = GetLives();
			lives[1] = GetLives();
			coin -= 8;
		}
		break;

	case eDJBOY_ACTIVE_GAMEPLAY:
		if (lives[0] == 0 && lives[1] == 0 && complete) // continue countdown complete
		{
			dat = 0x0f;
			mDjBoyState = eDJBOY_ATTRACT_HIGHSCORE;
		}
		else if (coin >= 4)
		{
			if ((io & 1) && lives[0] == 0)
			{
				dat = 0x12; // continue (P1)
				lives[0] = GetLives();
				mDjBoyState = eDJBOY_ACTIVE_GAMEPLAY;
				coin -= 4;
			}
			else if ((io & 2) && lives[1] == 0)
			{
				dat = 0x08; // continue (P2)
				lives[1] = GetLives();
				mDjBoyState = eDJBOY_ACTIVE_GAMEPLAY;
				coin -= 4;
			}
		}
		break;
	}
	complete = 0;
	ProtectionOut(i, dat);
}

static void CommonProt(INT32 i, INT32 type)
{
	int displayedCredits = coin / 4;
	if (displayedCredits > 9)
		displayedCredits = 9;

	ProtectionOut(i++, displayedCredits);
	ProtectionOut(i++, DrvInputs[0]); // COIN/START
	OutputProtectionState(i, type);
}

static void beast_data_w(UINT8 data)
{
	prot_busy_count = 1;

	watchdog = 0;

	if (prot_mode == ePROT_WAIT_DSW1_WRITEBACK)
	{
		ProtectionOut(0, DrvDips[1]); // DSW2
		prot_mode = ePROT_WAIT_DSW2_WRITEBACK;
	}
	else if (prot_mode == ePROT_WAIT_DSW2_WRITEBACK)
	{
		prot_mode = ePROT_STORE_PARAM;
		prot_offs = 0;
	}
	else if (prot_mode == ePROT_STORE_PARAM)
	{
		if (prot_offs < 8)
			prot_param[prot_offs++] = data;

		if(prot_offs == 8)
			prot_mode = ePROT_NORMAL;
	}
	else if (prot_mode == ePROT_WRITE_BYTE)
	{
		prot_ram[(prot_offs++) & 0x7f] = data;
		prot_mode = ePROT_WRITE_BYTES;
	}
	else
	{
		switch (data)
		{
		case 0x00:
			if (prot_mode == ePROT_WRITE_BYTES)
			{ // next byte is data to write to internal prot RAM
				prot_mode = ePROT_WRITE_BYTE;
			}
			else if (prot_mode == ePROT_READ_BYTES)
			{ // request next byte of internal prot RAM
				ProtectionOut(0, prot_ram[(prot_offs++) & 0x7f]);
			}

			break;

		case 0x01: // pc=7389
			OutputProtectionState(0, 0x01);
			break;

		case 0x02:
			CommonProt(0, 0x02);
			break;

		case 0x03: // prepare for memory write to protection device ram (pc == 0x7987) // -> 0x02
			prot_mode = ePROT_WRITE_BYTES;
			prot_offs = 0;
			break;

		case 0x04:
			ProtectionOut(0, 0); // ?
			ProtectionOut(1, 0); // ?
			ProtectionOut(2, 0); // ?
			ProtectionOut(3, 0); // ?
			CommonProt(4, 0x04);
			break;

		case 0x05: // 0x71f4
			ProtectionOut(0, DrvInputs[1]); // to $42
			ProtectionOut(1, 0); // ?
			ProtectionOut(2, DrvInputs[2]); // to $43
			ProtectionOut(3, 0); // ?
			ProtectionOut(4, 0); // ?
			CommonProt(5, 0x05);
			break;

		case 0x07:
			CommonProt(0, 0x07);
			break;

		case 0x08: // pc == 0x727a
			ProtectionOut(0, DrvInputs[0]); // COIN/START
			ProtectionOut(1, DrvInputs[1]); // JOY1
			ProtectionOut(2, DrvInputs[2]); // JOY2
			ProtectionOut(3, DrvDips[0]); // DSW1
			ProtectionOut(4, DrvDips[1]); // DSW2
			CommonProt(5, 0x08);
			break;

		case 0x09:
			ProtectionOut(0, 0); // ?
			ProtectionOut(1, 0); // ?
			ProtectionOut(2, 0); // ?
			CommonProt(3, 0x09);
			break;

		case 0x0a:
			CommonProt(0, 0x0a);
			break;

		case 0x0c:
			CommonProt(1, 0x0c);
			break;

		case 0x0d:
			CommonProt(2, 0x0d);
			break;

		case 0xfe: // prepare for memory read from protection device ram (pc == 0x79ee, 0x7a3f)
			if (prot_mode == ePROT_WRITE_BYTES)
			{
				prot_mode = ePROT_READ_BYTES;
			}
			else
			{
				prot_mode = ePROT_WRITE_BYTES;
			}
			prot_offs = 0;
			break;

		case 0xff: /* read DSW (pc == 0x714d) */
			ProtectionOut(0, DrvDips[0]); /* DSW1 */
			prot_mode = ePROT_WAIT_DSW1_WRITEBACK;
			break;

		case 0xa9: // 1-player game: P1 dies, 2-player game: P2 dies
			if (lives[0] > 0 && lives[1] > 0 )
			{
				lives[1]--;
			}
			else if (lives[0] > 0)
			{
				lives[0]--;
			}
			else
			{
				complete = 0xa9;
			}
			break;

		case 0x92: // p2 lost life; in 2-p game, P1 died
			if (lives[0] > 0 && lives[1] > 0 )
			{
				lives[0]--;
			}
			else if (lives[1] > 0)
			{
				lives[1]--;
			}
			else
			{
				complete = 0x92;
			}
			break;

		case 0xa3: // p2 bonus life
			lives[1]++;
			break;

		case 0xa5: // p1 bonus life
			lives[0]++;
			break;

		case 0xad: // 1p game start ack
			break;

		case 0xb0: // 1p+2p game start ack
			break;

		case 0xb3: // 1p continue ack
			break;

		case 0xb7: // 2p continue ack
			break;

		default:
		case 0x97:
		case 0x9a:
			break;
		}
	}
}

static UINT8 beast_data_r()
{
	UINT8 data = 0x00;
	if (prot_available_data_count)
	{
		INT32 i;
		data = prot_output_buffer[0];
		prot_available_data_count--;
		for (i = 0; i < prot_available_data_count; i++)
			prot_output_buffer[i] = prot_output_buffer[i + 1];
	}

	return data;
}

static UINT8 beast_status_r()
{
	UINT8 result = 0;

	if (prot_busy_count)
	{
		prot_busy_count--;
		result |= 1 << 3;
	}
	if (!prot_available_data_count)
	{
		result |= 1 << 2;
	}
	return result;
}

static void beast_reset()
{
	coin = 0;
	complete = 0;
	lives[0] = 0;
	lives[1] = 0;

	prot_busy_count = 0;
	prot_available_data_count = 0;
	prot_offs = 0;

	memset(prot_output_buffer, 0, 8);
	memset(prot_ram, 0, 0x80);
	memset(prot_param, 0, 8);

	mDjBoyState = eDJBOY_ATTRACT_HIGHSCORE;
	prot_mode = ePROT_NORMAL;
}

static INT32 beast_scan(INT32 nAction)
{
	struct BurnArea ba;
	
	if (nAction & ACB_MEMORY_RAM) {	
		memset(&ba, 0, sizeof(ba));
    		ba.Data	  = prot_ram;
		ba.nLen	  = 0x80;
		ba.szName = "Beast Prot. Ram";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
    		ba.Data	  = prot_param;
		ba.nLen	  = 0x8;
		ba.szName = "Beast Prot. Params";
		BurnAcb(&ba);

		memset(&ba, 0, sizeof(ba));
    		ba.Data	  = prot_output_buffer;
		ba.nLen	  = 0x8;
		ba.szName = "Beast Prot. Output Buffer";
		BurnAcb(&ba);
	}
	
	if (nAction & ACB_DRIVER_DATA) {

		SCAN_VAR(coin);
		SCAN_VAR(complete);
		SCAN_VAR(lives[0]);
		SCAN_VAR(lives[1]);
	
		SCAN_VAR(prot_busy_count);
		SCAN_VAR(prot_available_data_count);
		SCAN_VAR(prot_offs);

		SCAN_VAR(mDjBoyState);
		SCAN_VAR(prot_mode);
	}
	
	return 0;
}

//----------------------------------------------------------------------------------------------------------

static void cpu0_bankswitch(INT32 data)
{
	nBankAddress0 = data;

	ZetMapMemory(DrvZ80ROM0 + ((nBankAddress0 ^ bankxor) * 0x2000), 0xc000, 0xdfff, ZET_ROM);
}

static void __fastcall djboy_main_write(UINT16 address, UINT8 data)
{
	if ((address & 0xf000) == 0xb000) {
		DrvSprRAM[address & 0xfff] = data;
		address = (address & 0x800) | ((address & 0xff) << 3) | ((address & 0x700) >> 8);
		DrvPandoraRAM[address] = data;
		return;
	}
}

static void __fastcall djboy_main_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			cpu0_bankswitch(data);
		return;
	}
}

static inline void palette_update(INT32 offs)
{
	INT32 p = (DrvPalRAM[offs+1] + (DrvPalRAM[offs+0] * 256));

	INT32 r = (p >> 8) & 0x0f;
	INT32 g = (p >> 4) & 0x0f;
	INT32 b = (p >> 0) & 0x0f;

	DrvPalette[offs/2] = BurnHighCol((r*16)+r,(g*16)+g,(b*16)+b,0);
}

static void __fastcall djboy_cpu1_write(UINT16 address, UINT8 data)
{
	if ((address & 0xfc00) == 0xd000) {
		DrvPalRAM[address & 0x3ff] = data;
		if (address & 1) palette_update(address & 0x3fe);
		return;
	}

	if ((address & 0xf000) == 0xd000) {
		DrvPalRAM[address & 0xfff] = data;
		return;
	}
}

static void cpu1_bankswitch(INT32 data)
{
	INT32 bankdata[16] = { 0,1,2,3,-1,-1,-1,-1,4,5,6,7,8,9,10,11 };

	if (bankdata[data & 0x0f] == -1) return;

	nBankAddress1 = bankdata[data & 0x0f];

	ZetMapMemory(DrvZ80ROM1 + (nBankAddress1 * 0x4000), 0x8000, 0xbfff, ZET_ROM);
}

static void __fastcall djboy_cpu1_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			videoreg = data;
			cpu1_bankswitch(data);
		return;

		case 0x02:
		{
			*soundlatch = data;
			ZetClose();
			ZetOpen(2);
			ZetNmi();
			ZetClose();
			ZetOpen(1);
		}
		return;

		case 0x04:
			beast_data_w(data);
		return;

		case 0x06:
			scrolly = data;
		return;

		case 0x08:
			scrollx = data;
		return;

		case 0x0a:
		{
			ZetClose();
			ZetOpen(0);
			ZetNmi();
			ZetClose();
			ZetOpen(1);
		}
		return;

		case 0x0e:
			coinplus_w(data);
		return;
	}
}

static UINT8 __fastcall djboy_cpu1_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x04:
			return beast_data_r();

		case 0x0c:
			return beast_status_r();
	}

	return 0;
}

static void cpu2_bankswitch(INT32 data)
{
	nBankAddress2 = data;

	ZetMapMemory(DrvZ80ROM2 + (data * 0x04000), 0x8000, 0xbfff, ZET_ROM);
}

static void __fastcall djboy_cpu2_write_port(UINT16 port, UINT8 data)
{
	switch (port & 0xff)
	{
		case 0x00:
			cpu2_bankswitch(data);
		return;

		case 0x02:
		case 0x03:
			BurnYM2203Write(0, port & 1, data);
		return;

		case 0x06:
			MSM6295Command(0, data);
		return;

		case 0x07:
			MSM6295Command(1, data);
		return;
	}
}

static UINT8 __fastcall djboy_cpu2_read_port(UINT16 port)
{
	switch (port & 0xff)
	{
		case 0x02:
		case 0x03:
			return BurnYM2203Read(0, port & 1);

		case 0x04:
			return *soundlatch;

		case 0x06:
			return MSM6295ReadStatus(0);

		case 0x07:
			return MSM6295ReadStatus(1);
	}

	return 0;
}

static INT32 DrvSynchroniseStream(INT32 nSoundRate)
{
	return (INT64)ZetTotalCycles() * nSoundRate / 6000000;
}

static double DrvGetTime()
{
	return (double)ZetTotalCycles() / 6000000;
}

static INT32 DrvDoReset(INT32 full_reset)
{
	if (full_reset) {
		memset (AllRam, 0, RamEnd - AllRam);
	}

	ZetOpen(0);
	ZetReset();
	ZetClose();

	ZetOpen(1);
	ZetReset();
	ZetClose();

	ZetOpen(2);
	ZetReset();
	BurnYM2203Reset();
	ZetClose();

	MSM6295Reset(0);
	MSM6295Reset(1);

	beast_reset();

	return 0;
}

static INT32 DrvGfxDecode()
{
	static INT32 Plane[4]  = { STEP4(0,1) };
	static INT32 XOffs[16] = { STEP8(0,4), STEP8(256, 4) };
	static INT32 YOffs[16] = { STEP8(0,32), STEP8(512, 32) };

	UINT8 *tmp = (UINT8*)BurnMalloc(0x200000);
	if (tmp == NULL) {
		return 1;
	}

	memcpy (tmp, DrvGfxROM0, 0x200000);

	GfxDecode(0x4000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM0);

	memcpy (tmp, DrvGfxROM1, 0x100000);

	GfxDecode(0x2000, 4, 16, 16, Plane, XOffs, YOffs, 0x400, tmp, DrvGfxROM1);

	BurnFree (tmp);

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvZ80ROM0		= Next; Next += 0x040000;
	DrvZ80ROM1		= Next; Next += 0x030000;
	DrvZ80ROM2		= Next; Next += 0x020000;

	DrvGfxROM0		= Next; Next += 0x400000;
	DrvGfxROM1		= Next; Next += 0x200000;

	MSM6295ROM		= Next;
	DrvSndROM0		= Next; Next += 0x100000;
	DrvSndROM1		= Next; Next += 0x040000;

	DrvPalette		= (UINT32*)Next; Next += 0x0200 * sizeof(UINT32);

	AllRam			= Next;

	DrvVidRAM		= Next; Next += 0x001000;

	DrvShareRAM0		= Next; Next += 0x002000;

	DrvPandoraRAM		= Next; Next += 0x001000;
	DrvSprRAM		= Next; Next += 0x001000;
	DrvPalRAM		= Next; Next += 0x000900;
	DrvZ80RAM2		= Next; Next += 0x002000;

	soundlatch		= Next; Next += 0x000001;

	RamEnd			= Next;

	MemEnd			= Next;

	return 0;
}

static INT32 DrvInit()
{
	BurnSetRefreshRate(57.50);

	AllMem = NULL;
	MemIndex();
	INT32 nLen = MemEnd - (UINT8 *)0;
	if ((AllMem = (UINT8 *)BurnMalloc(nLen)) == NULL) return 1;
	memset(AllMem, 0, nLen);
	MemIndex();

	{
		if (BurnLoadRom(DrvZ80ROM0 + 0x000000,  0, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM0 + 0x020000,  1, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM1 + 0x000000,  2, 1)) return 1;
		if (BurnLoadRom(DrvZ80ROM1 + 0x010000,  3, 1)) return 1;

		if (BurnLoadRom(DrvZ80ROM2 + 0x000000,  4, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM0 + 0x000000,  6, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x080000,  7, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x100000,  8, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x180000,  9, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM0 + 0x1f0000, 10, 1)) return 1;

		if (BurnLoadRom(DrvGfxROM1 + 0x000000, 11, 1)) return 1;
		if (BurnLoadRom(DrvGfxROM1 + 0x080000, 12, 1)) return 1;

		if (BurnLoadRom(DrvSndROM0 + 0x000000, 13, 1)) return 1;

		if (BurnLoadRom(DrvSndROM1 + 0x000000, 14, 1)) return 1;

		DrvGfxDecode();
	}

	ZetInit(0);
	ZetOpen(0);
	ZetMapMemory(DrvZ80ROM0,		0x0000, 0xafff, ZET_ROM);
	ZetMapMemory(DrvSprRAM,			0xb000, 0xbfff, ZET_ROM); // handler...
	ZetMapMemory(DrvShareRAM0,		0xe000, 0xffff, ZET_RAM);
	ZetSetWriteHandler(djboy_main_write);
	ZetSetOutHandler(djboy_main_write_port);
	ZetClose();

	ZetInit(1);
	ZetOpen(1);
	ZetMapMemory(DrvZ80ROM1,		0x0000, 0x7fff, ZET_ROM);
	ZetMapMemory(DrvVidRAM,			0xc000, 0xcfff, ZET_RAM);
	ZetMapMemory(DrvPalRAM,			0xd000, 0xd8ff, ZET_ROM); // handler
	ZetMapMemory(DrvShareRAM0,		0xe000, 0xffff, ZET_RAM);
	ZetSetWriteHandler(djboy_cpu1_write);
	ZetSetOutHandler(djboy_cpu1_write_port);
	ZetSetInHandler(djboy_cpu1_read_port);
	ZetClose();

	ZetInit(2);
	ZetOpen(2);
	ZetMapMemory(DrvZ80ROM2,		0x0000, 0x7fff, ZET_ROM);
	ZetMapMemory(DrvZ80RAM2,		0xc000, 0xdfff, ZET_RAM);
	ZetSetOutHandler(djboy_cpu2_write_port);
	ZetSetInHandler(djboy_cpu2_read_port);
	ZetClose();

	BurnYM2203Init(1, 3000000, NULL, DrvSynchroniseStream, DrvGetTime, 0);
	BurnTimerAttachZet(6000000);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_YM2203_ROUTE, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_1, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_2, 0.50, BURN_SND_ROUTE_BOTH);
	BurnYM2203SetRoute(0, BURN_SND_YM2203_AY8910_ROUTE_3, 0.50, BURN_SND_ROUTE_BOTH);

	MSM6295Init(0, 1500000 / 165, 1);
	MSM6295SetRoute(0, 0.80, BURN_SND_ROUTE_BOTH);

	MSM6295Init(1, 1500000 / 165, 1);
	MSM6295SetRoute(1, 0.80, BURN_SND_ROUTE_BOTH);

	GenericTilesInit();

	pandora_init(DrvPandoraRAM, DrvGfxROM0, (0x400000/0x100)-1, 0x100, 0, -16);

	DrvDoReset(1);

	return 0;
}

static INT32 DrvExit()
{
	pandora_exit();

	GenericTilesExit();

	ZetExit();

	MSM6295Exit(0);
	MSM6295Exit(1);
	BurnYM2203Exit();

	BurnFree(AllMem);

	return 0;
}

static void draw_layer()
{
	INT32 xscroll = ((scrollx + ((videoreg & 0xc0) << 2)) - 0x391) & 0x3ff;
	INT32 yscroll = (scrolly + ((videoreg & 0x20) << 3) + 16) & 0x1ff;

	for (INT32 offs = 0; offs < 64 * 32; offs++)
	{
		INT32 sx = (offs & 0x3f) * 16;
		INT32 sy = (offs / 0x40) * 16;

		sx -= xscroll;
		if (sx < -15) sx += 1024;
		sy -= yscroll;
		if (sy < -15) sy += 512;

		if (sy >= nScreenHeight || sy >= nScreenWidth) continue;

		INT32 attr = DrvVidRAM[offs + 0x800];
		INT32 code = DrvVidRAM[offs + 0x000] + ((attr & 0x0f) * 256) + ((attr & 0x80) << 5);
		INT32 color = attr >> 4;

		Render16x16Tile_Clip(pTransDraw, code, sx, sy, color, 4, 0, DrvGfxROM1);
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		for (INT32 offs = 0; offs < 0x400; offs+=2) {
			palette_update(offs);
		}
		DrvRecalc = 0;
	}

	draw_layer();

	pandora_update(pTransDraw);

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset(1);
	}

	watchdog++;
	if (watchdog > 180) {
		DrvDoReset(0);
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

	INT32 nInterleave = 256;
	INT32 nCyclesTotal[3] =  { (6000000 * 10) / 575, (6000000 * 10) / 575,(6000000 * 10) / 575 }; // 57.5 fps
	INT32 nCyclesDone[3] = { 0, 0, 0 };

	for (INT32 i = 0; i < nInterleave; i++) {

		INT32 nSegment = (nCyclesTotal[0] / nInterleave) * (i + 1);

		ZetOpen(0);
		nCyclesDone[0] += ZetRun(nSegment - nCyclesDone[0]);
		if (i == 64 || i == 240) {
			if (i ==  64) ZetSetVector(0xff);
			if (i == 240) ZetSetVector(0xfd);
			ZetSetIRQLine(0, ZET_IRQSTATUS_AUTO);
		}
		ZetClose();

		ZetOpen(1);
		nCyclesDone[1] += ZetRun(nSegment - nCyclesDone[1]);
		if (i == 255) ZetSetIRQLine(0, ZET_IRQSTATUS_AUTO);
		ZetClose();

		ZetOpen(2);
		BurnTimerUpdate(nCyclesDone[1] /*sync with sub cpu*/);
		if (i == 255) ZetSetIRQLine(0, ZET_IRQSTATUS_AUTO);
		ZetClose();
	}

	ZetOpen(2);

	BurnTimerEndFrame(nCyclesTotal[2]);

	if (pBurnSoundOut) {
		BurnYM2203Update(pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(0, pBurnSoundOut, nBurnSoundLen);
		MSM6295Render(1, pBurnSoundOut, nBurnSoundLen);
	}

	ZetClose();
	
	if (pBurnDraw) {
		DrvDraw();
	}

	pandora_buffer_sprites();

	return 0;
}

static INT32 DrvScan(INT32 nAction,INT32 *pnMin)
{
	struct BurnArea ba;

	if (pnMin) {
		*pnMin = 0x029707;
	}

	if (nAction & ACB_VOLATILE) {		
		memset(&ba, 0, sizeof(ba));

		ba.Data	  = AllRam;
		ba.nLen	  = RamEnd - AllRam;
		ba.szName = "All Ram";
		BurnAcb(&ba);

		ZetScan(nAction);

		beast_scan(nAction);

		BurnYM2203Scan(nAction, pnMin);
		MSM6295Scan(0, nAction);
		MSM6295Scan(1, nAction);

		SCAN_VAR(nBankAddress0);
		SCAN_VAR(nBankAddress1);
		SCAN_VAR(nBankAddress2);
		SCAN_VAR(videoreg);
		SCAN_VAR(scrollx);
		SCAN_VAR(scrolly);
	}

	if (nAction & ACB_WRITE)
	{
		ZetOpen(0);
		cpu0_bankswitch(nBankAddress0);
		ZetClose();

		ZetOpen(1);
		cpu1_bankswitch(nBankAddress1);
		ZetClose();

		ZetOpen(2);
		cpu2_bankswitch(nBankAddress2);
		ZetClose();
	}

	return 0;
}


// DJ Boy (set 1)

static struct BurnRomInfo djboyRomDesc[] = {
	{ "bs64.4b",	0x20000, 0xb77aacc7, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bs65.5y",	0x10000, 0x0f1456eb, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bs07.1b",	0x10000, 0xd9b7a220, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs203.5j",	0x40000, 0x805341fb, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "bs203.5j",	0x40000, 0x805341fb, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(djboy)
STD_ROM_FN(djboy)

static INT32 DjboyInit()
{
	bankxor = 0;

	return DrvInit();
}

struct BurnDriver BurnDrvDjboy = {
	"djboy", NULL, NULL, NULL, "1989",
	"DJ Boy (set 1)\0", NULL, "Kaneko (American Sammy license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyRomInfo, djboyRomName, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};


// DJ Boy (set 2)

static struct BurnRomInfo djboyaRomDesc[] = {
	{ "bs19s.rom",	0x20000, 0x17ce9f6c, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bs15s.rom",	0x10000, 0xe6f966b2, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bs07.1b",	0x10000, 0xd9b7a220, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs203.5j",	0x40000, 0x805341fb, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "bs203.5j",	0x40000, 0x805341fb, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(djboya)
STD_ROM_FN(djboya)

struct BurnDriver BurnDrvDjboya = {
	"djboya", "djboy", NULL, NULL, "1989",
	"DJ Boy (set 2)\0", NULL, "Kaneko (American Sammy license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyaRomInfo, djboyaRomName, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 244, 4, 3
};


// DJ Boy (Japan)

static struct BurnRomInfo djboyjRomDesc[] = {
	{ "bs12.4b",	0x20000, 0x0971523e, 1 | BRF_PRG | BRF_ESS }, //  0 Z80 #0 Code
	{ "bs100.4d",	0x20000, 0x081e8af8, 1 | BRF_PRG | BRF_ESS }, //  1

	{ "bs13.5y",	0x10000, 0x5c3f2f96, 2 | BRF_PRG | BRF_ESS }, //  2 Z80 #1 Code
	{ "bs101.6w",	0x20000, 0xa7c85577, 2 | BRF_PRG | BRF_ESS }, //  3

	{ "bs200.8c",	0x20000, 0xf6c19e51, 3 | BRF_PRG | BRF_ESS }, //  4 Z80 #2 Code

	{ "beast.9s",	0x01000, 0xebe0f5f3, 4 | BRF_PRG | BRF_ESS }, //  5 Kaneko Beast MCU

	{ "bs000.1h",	0x80000, 0xbe4bf805, 5 | BRF_GRA },           //  6 Sprites
	{ "bs001.1f",	0x80000, 0xfdf36e6b, 5 | BRF_GRA },           //  7
	{ "bs002.1d",	0x80000, 0xc52fee7f, 5 | BRF_GRA },           //  8
	{ "bs003.1k",	0x80000, 0xed89acb4, 5 | BRF_GRA },           //  9
	{ "bsxx.1b",	0x10000, 0x22c8aa08, 5 | BRF_GRA },           // 10

	{ "bs004.1s",	0x80000, 0x2f1392c3, 6 | BRF_GRA },           // 11 Tiles
	{ "bs005.1u",	0x80000, 0x46b400c4, 6 | BRF_GRA },           // 12

	{ "bs-204.5j",	0x40000, 0x510244f0, 7 | BRF_SND },           // 13 OKI #0 Samples

	{ "bs-204.5j",	0x40000, 0x510244f0, 8 | BRF_SND },           // 14 OKI #1 Samples
};

STD_ROM_PICK(djboyj)
STD_ROM_FN(djboyj)

static INT32 DjboyjInit()
{
	bankxor = 0x1f;

	return DrvInit();
}

struct BurnDriver BurnDrvDjboyj = {
	"djboyj", "djboy", NULL, NULL, "1989",
	"DJ Boy (Japan)\0", NULL, "Kaneko (Sega license)", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING | BDF_CLONE, 2, HARDWARE_KANEKO_MISC, GBF_SCRFIGHT, 0,
	NULL, djboyjRomInfo, djboyjRomName, NULL, NULL, DjboyInputInfo, DjboyDIPInfo,
	DjboyjInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 0x200,
	256, 224, 4, 3
};
