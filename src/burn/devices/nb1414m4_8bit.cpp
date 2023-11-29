// license:MAME
// copyright-holders:Angelo Salese
/*******************************************************************************************************************

Nichibutsu 1414M4 device emulation

Written by Angelo Salese, based on researches by Tomasz Slanina with Legion

This is some fancy MCU / blitter that copies text strings in various Nihon Bussan games;

TODO:
- where is the condition that makes "insert coin" text to properly blink?
- first byte meaning is completely unknown;
- Ninja Emaki triggers unknown commands 0x8000 & 0xff20;
- Ninja Emaki continue screen is corrupt;
- How to NOT draw the params?
- Device-ify this;

Notes:
- Just before any string in the "MCU" rom, there's a control byte, this meaning is as follows:
  0?-- ---- ---- ---- interpret as data?
  10-- ---- ---- ---- single string transfer
  11-- ---- ---- ---- src -> dst copy, if destination != 0 fixed src, otherwise do a src -> dst
  --xx xxxx xxxx xxxx destination offset in the VRAM tilemap

- I'm sure that this is a shared device, that shares everything. All of the known differences are due of not
  understood features of the chip (some bytes in the ROM etc.)

********************************************************************************************************************/

#include "tiles_generic.h" // for UINT* etc
#include "nb1414m4_8bit.h"

UINT8* nb1414_blit_data8b = nullptr;
UINT32 nb1414_frame8b = 0;
static bool nb1414_in_game8b = false;

static void nichibutsu_1414m4_dma(UINT16 src, UINT16 dst, UINT16 size, UINT8 condition, UINT8* vram)
{
	UINT8* data = nb1414_blit_data8b;
	int i;

	for (i = 0; i < size; i++)
	{
		if (i + dst + 0x000 < 18) //avoid param overwrite
			continue;

		vram[i + dst + 0x000] = (condition) ? (data[i + (0) + src] & 0xff) : 0x20;

		vram[i + dst + 0x400] = (condition) ? (data[i + (size) + src] & 0xff) : data[0x13];
	}
}

static void nichibutsu_1414m4_fill(UINT16 dst, UINT8 tile, UINT8 pal, UINT8* vram)
{
	int i;

	for (i = 0; i < 0x400; i++)
	{
		if (i + dst + 0x000 < 18) //avoid param overwrite
			continue;

		vram[i + dst + 0x000] = tile;
		vram[i + dst + 0x400] = pal;
	}
}

static void insert_coin_msg(UINT8* vram)
{
	if (nb1414_in_game8b)
		return;

	nb1414_frame8b++;

	UINT8* data = nb1414_blit_data8b;
	int credit_count = (vram[0xf] & 0xff);
	UINT8 fl_cond = nb1414_frame8b & 0x10; /* for insert coin "flickering" */
	UINT16 dst;

	if (credit_count == 0)
	{
		dst = (data[0x01] << 8 | data[0x02]) & 0x3fff;

		nichibutsu_1414m4_dma(0x0003, dst, 0x10, fl_cond, vram);
	}
	else
	{
		dst = (data[0x49] << 8 | data[0x4a]) & 0x3fff;

		nichibutsu_1414m4_dma(0x004b, dst, 0x18, 1, vram);
	}
}

static void credit_msg(UINT8* vram)
{
	UINT8* data = nb1414_blit_data8b;
	int credit_count = (vram[0xf] & 0xff);
	UINT8 fl_cond = nb1414_frame8b & 0x10; /* for insert coin "flickering" */
	UINT16 dst;

	dst = ((data[0x023] << 8) | (data[0x024] & 0xff)) & 0x3fff;
	nichibutsu_1414m4_dma(0x0025, dst, 0x10, 1, vram); /* credit */

	/* credit num */
	dst = ((data[0x045] << 8) | (data[0x046] & 0xff)) & 0x3fff;
	vram[dst + 0x000] = (credit_count & 0xf0) ? (((credit_count & 0xf0) >> 4) + 0x30) : 0x20;
	vram[dst + 0x400] = (data[0x47]);
	vram[dst + 0x001] = ((credit_count & 0x0f) + 0x30);
	vram[dst + 0x401] = (data[0x48]);

	if (nb1414_in_game8b)
		return;

	if (credit_count == 1) /* ONE PLAYER ONLY */
	{
		dst = ((data[0x07b] << 8) | (data[0x07c] & 0xff)) & 0x3fff;
		nichibutsu_1414m4_dma(0x007d, dst, 0x18, fl_cond, vram);
	}
	else if (credit_count > 1) /* ONE OR TWO PLAYERS */
	{
		dst = ((data[0x0ad] << 8) | (data[0x0ae] & 0xff)) & 0x3fff;
		nichibutsu_1414m4_dma(0x00af, dst, 0x18, fl_cond, vram);
	}
}

static void kozure_score_msg(UINT16 dst, UINT8 src_base, UINT8* vram)
{
	int i;
	UINT8 first_digit;
	UINT8 res;
	UINT8* data = nb1414_blit_data8b;

	first_digit = 0;

	for (i = 0; i < 6; i++)
	{
		res = ((vram[(i / 2) + 5 + src_base * 3] >> (!(i & 1) * 4)) & 0xf);

		if (first_digit || res)
		{
			vram[i + dst + 0x0000] = res + 0x30;
			first_digit = 1;
		}
		else
			vram[i + dst + 0x0000] = 0x20;

		vram[i + dst + 0x0400] = data[0x10f + (src_base * 0x1c) + i];
	}

	vram[6 + dst + 0x0000] = vram[5 + dst + 0x0000] == 0x20 ? 0x20 : 0x30;
	vram[6 + dst + 0x0400] = data[0x10f + (src_base * 0x1c) + 6];
	vram[7 + dst + 0x0000] = 0x30;
	vram[7 + dst + 0x0400] = data[0x10f + (src_base * 0x1c) + 7];
}

static void nichibutsu_1414m4_0200(UINT16 mcu_cmd, UINT8* vram)
{
	UINT8* data = nb1414_blit_data8b;
	UINT16 dst;

	// In any game, this bit is set when now playing.
	// If it is set, "INSERT COIN" etc. are not displayed.
	nb1414_in_game8b = (mcu_cmd & 0x80) != 0;

	dst = (data[0x330 + ((mcu_cmd & 0xf) * 2)] << 8) | (data[0x331 + ((mcu_cmd & 0xf) * 2)] & 0xff);

	dst &= 0x3fff;

	if (dst & 0x7ff) // fill
		nichibutsu_1414m4_fill(0x0000, data[dst & 0x3fff], data[dst + 1], vram);
	else // src -> dst
		nichibutsu_1414m4_dma(dst & 0x3fff, 0x0000, 0x400, 1, vram);
}

/*
[0x02] & 0x01 p1 up
[0x02] & 0x02 p1 down
[0x02] & 0x04 p1 left
[0x02] & 0x08 p1 right
[0x02] & 0x10 p1 button 1
[0x02] & 0x20 p1 button 2
[0x02] & 0x40 p1 button 3
[0x03] & 0x01 p2 up
[0x03] & 0x02 p2 down
[0x03] & 0x04 p2 left
[0x03] & 0x08 p2 right
[0x03] & 0x10 p2 button 1
[0x03] & 0x20 p2 button 2
[0x03] & 0x40 p2 button 3
[0x04] & 0x10 service
[0x04] & 0x04 coin A
[0x04] & 0x08 coin B
[0x04] & 0x01 start 1
[0x04] & 0x02 start 2
[0x05] DSW1
[0x06] DSW2
[0x07] & 0x40 demo sounds ON / OFF
[0x07] & 0x7 lives setting
[0x07] & 0x80 cabinet (upright / table)
[0x07] & 0x30 difficulty (easy / normal / hard / hardest)
[0x0f] coinage A
[0x10] coinage B
[0x11] sound test num
*/
static void nichibutsu_1414m4_0600(UINT8 is2p, UINT8* vram)
{
	UINT8* data = nb1414_blit_data8b;
	UINT16 dst;
	int i;

	dst = ((data[0x1f5] << 8) | (data[0x1f6] & 0xff)) & 0x3fff;
	vram[dst] = (vram[7] & 0x7) + 0x30; //data[0x1f7];

	dst = ((data[0x1f8] << 8) | (data[0x1f9] & 0xff)) & 0x3fff;
	nichibutsu_1414m4_dma(0x1fa + (((vram[7] & 0x30) >> 4) * 0x18), dst, 12, 1, vram);

	// 0x25a - 0x261 unknown meaning

	dst = ((data[0x262] << 8) | (data[0x263] & 0xff)) & 0x3fff;
	nichibutsu_1414m4_dma(0x264 + (((vram[7] & 0x80) >> 7) * 0x18), dst, 12, 1, vram);

	dst = ((data[0x294] << 8) | (data[0x295] & 0xff)) & 0x3fff;
	nichibutsu_1414m4_dma(0x296 + (((vram[7] & 0x40) >> 6) * 0x18), dst, 12, 1, vram);

	dst = ((data[0x2c6] << 8) | (data[0x2c7] & 0xff)) & 0x3fff;
	vram[dst] = ((vram[0xf] & 0xf0) >> 4) + 0x30; //data[0x2c8];

	dst = ((data[0x2c9] << 8) | (data[0x2ca] & 0xff)) & 0x3fff;
	vram[dst] = ((vram[0xf] & 0x0f) >> 0) + 0x30; //data[0x2cb];

	dst = ((data[0x2cc] << 8) | (data[0x2cd] & 0xff)) & 0x3fff;
	vram[dst] = ((vram[0x10] & 0xf0) >> 4) + 0x30; //data[0x2ce];

	dst = ((data[0x2cf] << 8) | (data[0x2d0] & 0xff)) & 0x3fff;
	vram[dst] = ((vram[0x10] & 0x0f) >> 0) + 0x30; //data[0x2d1];

	dst = ((data[0x2d2] << 8) | (data[0x2d3] & 0xff)) & 0x3fff;
	vram[dst + 0] = ((vram[0x11] & 0xf0) >> 4) + 0x30; //data[0x2d4];
	vram[dst + 1] = (vram[0x11] & 0x0f) + 0x30; //data[0x2d5];

	dst = ((data[0x2d6] << 8) | (data[0x2d7] & 0xff)) & 0x3fff;
	nichibutsu_1414m4_dma(0x2d8 + (is2p * 0x18), dst, 12, 1, vram); // 1p / 2p string

	dst = ((data[0x308] << 8) | (data[0x309] & 0xff)) & 0x3fff;
	for (i = 0; i < 5; i++) /* system inputs */
		nichibutsu_1414m4_dma(0x310 + (((vram[0x04] >> (4 - i)) & 1) * 6), dst + (i * 0x20), 0x3, 1, vram);

	dst = ((data[0x30a] << 8) | (data[0x30b] & 0xff)) & 0x3fff;
	for (i = 0; i < 7; i++) /* 1p / 2p inputs */
		nichibutsu_1414m4_dma(0x310 + (((vram[0x02 + is2p] >> (6 - i)) & 1) * 6), dst + (i * 0x20), 0x3, 1, vram);

	dst = ((data[0x30c] << 8) | (data[0x30d] & 0xff)) & 0x3fff;
	for (i = 0; i < 8; i++) /* dips */
		nichibutsu_1414m4_dma(0x310 + (((vram[0x05] >> (7 - i)) & 1) * 6), dst + (i * 0x20), 0x3, 1, vram);

	dst = ((data[0x30e] << 8) | (data[0x30f] & 0xff)) & 0x3fff;
	for (i = 0; i < 8; i++) /* dips */
		nichibutsu_1414m4_dma(0x310 + (((vram[0x06] >> (7 - i)) & 1) * 6), dst + (i * 0x20), 0x3, 1, vram);
}

static void nichibutsu_1414m4_0e00(UINT16 mcu_cmd, UINT8* vram)
{
	UINT8* data = nb1414_blit_data8b;
	UINT16 dst;

	dst = ((data[0xdf] << 8) | (data[0xe0] & 0xff)) & 0x3fff;
	nichibutsu_1414m4_dma(0x00e1, dst, 8, 1, vram); /* hi-score */

	dst = ((data[0xfb] << 8) | (data[0xfc] & 0xff)) & 0x3fff;
	nichibutsu_1414m4_dma(0x00fd, dst, 8, !(mcu_cmd & 1), vram); /* 1p-msg */
	dst = ((data[0x10d] << 8) | (data[0x10e] & 0xff)) & 0x3fff;
	kozure_score_msg(dst, 0, vram); /* 1p score */

	if (mcu_cmd & 0x80)
	{
		dst = ((data[0x117] << 8) | (data[0x118] & 0xff)) & 0x3fff;
		nichibutsu_1414m4_dma(0x0119, dst, 8, !(mcu_cmd & 2), vram); /* 2p-msg */
		dst = ((data[0x129] << 8) | (data[0x12a] & 0xff)) & 0x3fff;
		kozure_score_msg(dst, 1, vram); /* 2p score */
	}

	if ((mcu_cmd & 0x04) == 0)
	{
		dst = ((data[0x133] << 8) | (data[0x134] & 0xff)) & 0x3fff;
		nichibutsu_1414m4_dma(0x0135, dst, 0x10, 1, vram); /* game over */
		insert_coin_msg(vram);
		if ((mcu_cmd & 0x18) == 0) // TODO: either one of these two disables credit display
			credit_msg(vram);
	}
}

void nb_1414m4_init8b()
{
	nb1414_frame8b = 0;
	nb1414_in_game8b = false;
}

void nb_1414m4_scan8b()
{
	SCAN_VAR(nb1414_frame8b);
	SCAN_VAR(nb1414_in_game8b);
}

void nb_1414m4_exec8b(UINT16 mcu_cmd, UINT8* vram, UINT16* scrollx, UINT16* scrolly, INT32 emakimode)
{
	/* latch fg scroll values */
	*scrollx = (vram[0x0d] & 0xff) | ((vram[0x0e] & 0xff) << 8);
	*scrolly = (vram[0x0b] & 0xff) | ((vram[0x0c] & 0xff) << 8);

	if (emakimode && mcu_cmd == 0x286)
	{
		// this fixes the continue screen in Ninja Emaki
		memset(vram, 0x00, 18);
	}

	/* process the command */
	switch (mcu_cmd & 0xff00)
	{
	/* title screen / continue screens */
	case 0x0000: insert_coin_msg(vram);
		credit_msg(vram);
		break;

	/* direct DMA'ing / fill */
	case 0x0200: nichibutsu_1414m4_0200(mcu_cmd & 0x87, vram);
		break;

	/* service mode */
	case 0x0600: nichibutsu_1414m4_0600(mcu_cmd & 1, vram);
		break;

	/* gameplay */
	case 0x0e00: nichibutsu_1414m4_0e00(mcu_cmd & 0xff, vram);
		break;

	case 0x8000: break; //Ninja Emaki, attract mode
	case 0xff00: break; //Ninja Emaki POST, presumably invalid
	default:
		//popmessage("NB 1414M4 executes %04x command, contact MAMEdev\n",mcu_cmd);
		break;
	}
}
