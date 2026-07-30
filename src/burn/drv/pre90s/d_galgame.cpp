// FinalBurn Neo Galaxy Game driver module
// Based on MAME driver by Mariusz Wojcieszek

#include "tiles_generic.h"
#include "t11_intf.h"

static UINT8 *AllMem;
static UINT8 *AllRam;
static UINT8 *RamEnd;
static UINT8 *MemEnd;
static UINT8 *DrvProgList;
static UINT8 *DrvT11RAM;

static UINT32 *DrvPalette;
static UINT8 DrvRecalc;

static INT32 gg_x;
static INT32 gg_y;
static INT32 gg_clk;
static INT32 gg_ac;
static INT32 gg_mq;

static INT32 interrupt;
static INT32 point_work_list_index;
static INT32 point_display_list_index;

#define MAX_POINTS 2048

static UINT16 *point_work_list_x;
static UINT16 *point_work_list_y;
static UINT16 *point_display_list_x;
static UINT16 *point_display_list_y;

static UINT8 DrvJoy1[16];
static UINT8 DrvJoy2[16];
static UINT16 DrvInputs[2];
static UINT8 DrvDips[2];
static UINT8 DrvReset;

static struct BurnInputInfo GalaxygameInputList[] = {
	{"Coin 1",			BIT_DIGITAL,	DrvJoy1 + 1,	"p1 coin"		},
	{"Coin 2",			BIT_DIGITAL,	DrvJoy1 + 9,	"p2 coin"		},
	{"Coin 3",			BIT_DIGITAL,	DrvJoy1 + 0,	"p3 coin"		},
	{"Coin 4",			BIT_DIGITAL,	DrvJoy1 + 8,	"p4 coin"		},

	{"P1 Left",			BIT_DIGITAL,	DrvJoy2 + 9,	"p1 left"		},
	{"P1 Right",		BIT_DIGITAL,	DrvJoy2 + 8,	"p1 right"		},
	{"P1 Button 1",		BIT_DIGITAL,	DrvJoy2 + 13,	"p1 fire 1"		},
	{"P1 Button 2",		BIT_DIGITAL,	DrvJoy2 + 11,	"p1 fire 2"		},
	{"P1 Button 3",		BIT_DIGITAL,	DrvJoy2 + 12,	"p1 fire 3"		},

	{"P2 Left",			BIT_DIGITAL,	DrvJoy2 + 1,	"p2 left"		},
	{"P2 Right",		BIT_DIGITAL,	DrvJoy2 + 0,	"p2 right"		},
	{"P2 Button 1",		BIT_DIGITAL,	DrvJoy2 + 5,	"p2 fire 1"		},
	{"P2 Button 2",		BIT_DIGITAL,	DrvJoy2 + 3,	"p2 fire 2"		},
	{"P2 Button 3",		BIT_DIGITAL,	DrvJoy2 + 4,	"p2 fire 3"		},

	{"Reset",			BIT_DIGITAL,	&DrvReset,		"reset"			},
	{"Dip A",			BIT_DIPSWITCH,	DrvDips + 0,	"dip"			},
	{"Dip B",			BIT_DIPSWITCH,	DrvDips + 1,	"dip"			},
};

STDINPUTINFO(Galaxygame)

static struct BurnDIPInfo GalaxygameDIPList[]=
{
	DIP_OFFSET(0x0f)
	{0x00, 0xff, 0xff, 0x00, NULL						},
	{0x01, 0xff, 0xff, 0x00, NULL						},

	{0   , 0xfe, 0   ,    2, "Speed"					},
	{0x00, 0x01, 0x80, 0x00, "Slow Speed"				},
	{0x00, 0x01, 0x80, 0x80, "Fast Speed"				},

	{0   , 0xfe, 0   ,    2, "Players"					},
	{0x00, 0x01, 0x40, 0x00, "Two Players"				},
	{0x00, 0x01, 0x40, 0x40, "One Player"				},

	{0   , 0xfe, 0   ,    2, "Gravity"					},
	{0x01, 0x01, 0x80, 0x00, "Positive Gravity"			},
	{0x01, 0x01, 0x80, 0x80, "Negative Gravity"			},

	{0   , 0xfe, 0   ,    2, "Sun"						},
	{0x01, 0x01, 0x40, 0x00, "Sun (& Gravity)"			},
	{0x01, 0x01, 0x40, 0x40, "No Sun (& No Gravity)"	},
};

STDDIPINFO(Galaxygame)

static void ke_write(INT32 address, UINT16 data)
{
	switch ((address >> 1) & 7)
	{
		case 0: // DIV
		{
				if ( data != 0 )
				{
					INT32 dividend = (INT32)((UINT32)((UINT16)gg_ac << 16) | (UINT16)(gg_mq));
					gg_mq = dividend / (INT16)data;
					gg_ac = dividend % (INT16)data;
				}
				else
				{
					gg_mq = 0;
					gg_ac = 0;
				}
		}
		break;

		case 1: // AC
			gg_ac = (INT16)data;
		break;

		case 2: // MQ
			gg_mq = (INT16)data;
			if (gg_mq < 0)
			{
				gg_ac = -1;
			}
			else
			{
				gg_ac = 0;
			}
		break;

		case 3: // X
		{
				INT32 mulres = (INT32)gg_mq*(INT32)(INT16)data;
				gg_ac = mulres >> 16;
				gg_mq = mulres & 0xffff;
		}
		break;

		case 6: // LSH
		{
				data &= 63;
				INT32 val = (INT32)((UINT32)((UINT16)gg_ac << 16) | (UINT16)(gg_mq));
				if ( data < 32 )
				{
					val = val << data;
				}
				else
				{
					val = val >> (64 - data);
				}
				gg_mq = val & 0xffff;
				gg_ac = (val >> 16) & 0xffff;
		}
		break;

		case 7: // ASH
		{
				data &= 63;
				INT32 val = (INT32)((UINT32)((UINT16)gg_ac << 16) | (UINT16)(gg_mq));
				if ( data < 32 )
				{
					val = val << data;
				}
				else
				{
					val = val >> (64 - data);
				}
				gg_mq = val & 0xffff;
				gg_ac = (val >> 16) & 0xffff;
		}
		break;
	}
}

static void y_write(UINT16 data)
{
	gg_y = data;

	if (data == 0x0101)
	{
		if (interrupt)
		{
			for (INT32 i = 0; i < point_work_list_index && i < MAX_POINTS; i++ )
			{
				point_display_list_x[i] = point_work_list_x[i];
				point_display_list_y[i] = point_work_list_y[i] ^ 0xff80;
			}
			point_display_list_index = point_work_list_index;
			point_work_list_index = 0;
			interrupt = 0;
		}
	}
	else if (point_work_list_index < MAX_POINTS)
	{
		point_work_list_x[point_work_list_index] = gg_x;
		point_work_list_y[point_work_list_index] = gg_y;
		point_work_list_index++;
	}
}

static void galgame_write_byte(UINT16 address, UINT8 data)
{
	bprintf (0, _T("WB: %4.4x, %2.2x\n"), address, data);
}

static void galgame_write_word(UINT16 address, UINT16 data)
{
	//bprintf (0, _T("WW: %4.4x, %4.4x\n"), address, data);
	
	if ((address & 0xfff0) == 0xfec0) {
		ke_write(address, data);
		return;
	}

	switch (address)
	{
		case 0xff52:
			y_write(data);
		return;

		case 0xff5a:
			gg_x = data;
		return;

		case 0xff66:
			gg_clk = data;
		return;
	}
}

static UINT8 galgame_read_byte(UINT16 address)
{
	bprintf (0, _T("RB: %4.4x\n"), address);

	return 0;
}

static UINT16 galgame_read_word(UINT16 address)
{
	//bprintf (0, _T("RW: %4.4x\n"), address);

	if ((address & 0xfff0) == 0xfec0)
	{
		switch ((address & 0xe) >> 1)
		{
			case 1: return gg_ac;
			case 2: return gg_mq;
		}

		return 0;
	}

	switch (address)
	{
		case 0xff52:
			return gg_y;

		case 0xff54:
			return DrvInputs[0]; // coinac

		case 0xff5a:
			return gg_x;

		case 0xff5c:
			return DrvInputs[1]; // sr
	}

	return 0;
}

static INT32 irq_callback(INT32)
{
	t11SetIRQLine(0, CPU_IRQSTATUS_NONE);

	return 0x40;
}

static void ProcessListFile(UINT8 *code); // fwd

static INT32 DrvDoReset()
{
	memset (AllRam, 0, RamEnd - AllRam);

	ProcessListFile(DrvProgList); // uses t11 read/writes

	t11Open(0);
	t11Reset();
	t11Close();

	gg_x = 0;
	gg_y = 0;
	gg_clk = 0;
	gg_ac = 0;
	gg_mq = 0;

	interrupt = 0;
	point_work_list_index = 0;
	point_display_list_index = 0;

	return 0;
}

static INT32 MemIndex()
{
	UINT8 *Next; Next = AllMem;

	DrvProgList				= Next; Next += 0x20000;

	DrvPalette				= (UINT32*)Next; Next += 0x0002 * sizeof(UINT32);

	AllRam					= Next;

	DrvT11RAM				= Next; Next += 0x002000;

	point_work_list_x		= (UINT16*)Next; Next += MAX_POINTS * sizeof(UINT16);
	point_work_list_y		= (UINT16*)Next; Next += MAX_POINTS * sizeof(UINT16);
	point_display_list_x	= (UINT16*)Next; Next += MAX_POINTS * sizeof(UINT16);
	point_display_list_y	= (UINT16*)Next; Next += MAX_POINTS * sizeof(UINT16);

	RamEnd					= Next;

	MemEnd					= Next;

	return 0;
}

static UINT8 read_uint16(UINT16 *pval, int pos, const UINT8* line, int linelen)
{
	int i;

	*pval = 0;
	if ( linelen < (pos + 6) )
	{
		return 0;
	}

	for ( i = 0; i < 6; i++ )
	{
		*pval <<= 3;
		*pval |= line[pos + i] - 0x30;
	}
	return 1;
}

static UINT8 read_uint8(UINT8 *pval, int pos, const UINT8* line, int linelen)
{
	int i;

	*pval = 0;
	if ( linelen < (pos + 3) )
	{
		return 0;
	}

	for ( i = 0; i < 3; i++ )
	{
		*pval <<= 3;
		*pval |= line[pos + i] - 0x30;
	}
	return 1;
}

static void ProcessListFile(UINT8 *code)
{
	t11Open(0);

	int filepos = 0, linepos, linelen;
	UINT8 line[256];
	UINT16 address;
	UINT16 val;
	UINT8 val8;

	//load lst file
	while( code[filepos] != 0 )
	{
		linepos = 0;
		while( code[filepos] != 0x0d )
		{
			line[linepos++] = code[filepos++];
		}
		line[linepos] = 0;
		filepos += 2;
		linelen = linepos;

		if ( linelen == 0 )
		{
			continue;
		}
		if ( ( line[8] != ' ' ) && read_uint16(&address, 7, line, linelen ) )
		{
			if ( (linelen >= 15+6) && (line[15] != ' ') )
			{
				read_uint16(&val, 15, line, linelen);
				t11WriteWord(address, val);
				address += 2;

				if ( (linelen >= 22+6) && (line[22] != ' ') )
				{
					read_uint16(&val, 22, line, linelen);
					t11WriteWord(address, val);
					address += 2;
				}

				if ( (linelen >= 29+6) && (line[29] != ' ') )
				{
					read_uint16(&val, 29, line, linelen);
					t11WriteWord(address, val);
					address += 2;
				}

			}
			else
			{
				if ( (linelen >= 18+3) && (line[18] != ' ') )
				{
					read_uint8(&val8, 18, line, linelen);
					t11WriteByte(address, val8);
					address += 1;
				}
			}

		}
	}

	// set startup code
	t11WriteWord(0, 012700); /* MOV #0, R0 */
	t11WriteWord(2, 0);
	t11WriteWord(4, 0x8d00); /* MTPS R0 */
	t11WriteWord(6, 000167); /* JMP 0500*/
	t11WriteWord(8, 000500 - 10);

	t11Close();
}

static INT32 DrvInit()
{
	BurnAllocMemIndex();

	t11Init(5, irq_callback);
	t11Open(0);
	t11MapMemory(DrvT11RAM,		0x0000, 0x1fff, MAP_RAM);
	t11SetWriteByteHandler(galgame_write_byte);
	t11SetWriteWordHandler(galgame_write_word);
	t11SetReadByteHandler(galgame_read_byte);
	t11SetReadWordHandler(galgame_read_word);
	t11Close();
	
	{
		if (BurnLoadRom(DrvProgList, 0, 1)) return 1;
	}

	GenericTilesInit();

	DrvDoReset();

	return 0;
}

static INT32 DrvExit()
{
	GenericTilesExit();

	t11Exit();

	BurnFreeMemIndex();

	return 0;
}

static void draw_points()
{
	for (INT32 i = 0; i < point_display_list_index && i < MAX_POINTS; i++)
	{
		INT32 sx = point_display_list_x[i] >> 7;
		INT32 sy = point_display_list_y[i] >> 7;

		if (sx >= 0 && sx < nScreenWidth && sy >= 0 && sy < nScreenHeight)
		{
			pTransDraw[sy * nScreenWidth + sx] = 1;
		}
	}
}

static INT32 DrvDraw()
{
	if (DrvRecalc) {
		DrvPalette[0] = 0;
		DrvPalette[1] = BurnHighCol(0xff,0xff,0xff, 0);
		DrvRecalc = 0;
	}

	BurnTransferClear(0);

	draw_points();

	BurnTransferCopy(DrvPalette);

	return 0;
}

static INT32 DrvFrame()
{
	if (DrvReset) {
		DrvDoReset();
	}

	{
		DrvInputs[0] = 0;
		DrvInputs[1] = (DrvDips[1] << 8) | DrvDips[0];

		for (INT32 i = 0; i < 16; i++) {
			DrvInputs[0] ^= (DrvJoy1[i] & 1) << i;
			DrvInputs[1] ^= (DrvJoy2[i] & 1) << i;
		}
	}

	INT32 nInterleave = 1;
	INT32 nCyclesTotal[1] = { 3000000 / 60 };
	INT32 nCyclesDone[1] = { 0 };

	t11Open(0);

	for (INT32 i = 0; i < nInterleave; i++)
	{
		CPU_RUN(0, t11);

		if (i == (nInterleave - 1) && (gg_clk & 0x40)) {
			t11SetIRQLine(0, CPU_IRQSTATUS_ACK);
			interrupt = 1;
		}
	}

	t11Close();

	if (pBurnSoundOut) {

	}

	if (pBurnDraw) {
		BurnDrvRedraw();
	}

	return 0;
}

static INT32 DrvScan(INT32 nAction, INT32 *pnMin)
{
	if (pnMin) {
		*pnMin = 0x029702;
	}

	if (nAction & ACB_VOLATILE) {
		ScanVar(AllRam, RamEnd - AllRam, "All Ram");

		t11Scan(nAction);

		SCAN_VAR(gg_x);
		SCAN_VAR(gg_y);
		SCAN_VAR(gg_clk);
		SCAN_VAR(gg_ac);
		SCAN_VAR(gg_mq);

		SCAN_VAR(interrupt);
		SCAN_VAR(point_work_list_index);
		SCAN_VAR(point_display_list_index);

	}

	return 0;
}


// Galaxy Game

static struct BurnRomInfo galgameRomDesc[] = {
	{ "sw97.lst",		0x01f062, 0x838018a5,  1 | BRF_PRG | BRF_ESS }, //  0 t11 Code
};

STD_ROM_PICK(galgame)
STD_ROM_FN(galgame)

struct BurnDriver BurnDrvGalgame = {
	"galgame", NULL, NULL, NULL, "1971",
	"Galaxy Game\0", "NO Sound", "Computer Recreations, Inc", "Miscellaneous",
	NULL, NULL, NULL, NULL,
	BDF_GAME_WORKING, 2, HARDWARE_MISC_PRE90S, GBF_MISC, 0,
	NULL, galgameRomInfo, galgameRomName, NULL, NULL, NULL, NULL, GalaxygameInputInfo, GalaxygameDIPInfo,
	DrvInit, DrvExit, DrvFrame, DrvDraw, DrvScan, &DrvRecalc, 2,
	512, 512, 4, 3
};
