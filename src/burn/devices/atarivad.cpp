#include "tiles_generic.h"
#include "m68000_intf.h"
#include "atarimo.h"

static INT32 playfield_number[2]; // const

static INT32 playfield_latched;
static INT32 palette_bank;
static UINT16 control_data[32];
static UINT16 pf_scrolly[2];
static UINT16 pf_scrollx[2];
static INT32 mo_xscroll = 0;
static INT32 mo_yscroll = 0;

static UINT8 *playfield_data[3];
static UINT16 *pf_data[3];

// call every scanline
static INT32 tilerow_scanline = 0;
static INT32 tilerow_partial_prev_line = 0;

INT32 atarivad_scanline_timer = 0;
INT32 atarivad_scanline_timer_enabled = 0;
static void (*scanline_timer_callback)(INT32 param); // const
static void (*AtariVADPartialCB)(INT32 param);

INT32 atarivad_scanline; // external

static UINT8 *palette_ram;
static void (*atari_palette_write)(INT32 offset, UINT16 data) = NULL;

static tilemap_callback( bg ) // offtwall // shuuz
{
	UINT16 code = pf_data[0][offs];
	UINT16 color = pf_data[2][offs] >> 8;

	TILE_SET_INFO(0, code, color, TILE_FLIPYX(code >> 15));
}

static tilemap_callback( bg0 )
{
	UINT16 code = pf_data[0][offs];
	UINT16 color = pf_data[2][offs];

	TILE_SET_INFO(0, code, color, TILE_FLIPYX(code >> 15) | TILE_GROUP((color >> 4) & 3));
//	*category = (color >> 4) & 3;
}

static tilemap_callback( bg1 )
{
	UINT16 code = pf_data[1][offs];
	UINT16 color = pf_data[2][offs] >> 8;

	TILE_SET_INFO(1, code, color, TILE_FLIPYX(code >> 15) | TILE_GROUP((color >> 4) & 3));
//	*category = (color >> 4) & 3;
}

static void scanline_timer_dummy(INT32 ) {}
static void palette_write_dummy(INT32, UINT16) {}

static void update_parameter(UINT16 data)
{
	INT32 control = data & 0xf;
	data >>= 7;

	switch (control)
	{
		case 9:
			mo_xscroll = data;
		break;

		case 10:
			pf_scrollx[1] = data;
		break;

		case 11:
			pf_scrollx[0] = data;
		break;

		case 13:
			mo_yscroll = data;
		break;

		case 14:
			pf_scrolly[1] = data;
		break;

		case 15:
			pf_scrolly[0] = data;
		break;
	}
}

void AtariVADRecalcPalette()
{
	if (atari_palette_write) {
		for (INT32 i = 0; i < 0x7ff; i++) {
			UINT16 pal = *((UINT16*)(palette_ram + (i << 1)));
			atari_palette_write(i, pal);
		}
	}
}

static void __fastcall atari_vad_write_word(UINT32 address, UINT16 data)
{
	address &= 0x1fffe;

//	bprintf (0, _T("VAD,WW: %5.5x, %4.4x\n"), address, data);

	if ((address & 0xff000) == 0x00000) {
		*((UINT16*)(palette_ram + address)) = data;
		if (atari_palette_write) {
			atari_palette_write(address / 2, data);
		}
		return;
	}

	if ((address & 0xfffc0) == 0x0ffc0)
	{
		INT32 offset = (address / 2) & 0x1f;
		UINT16 prev_control = control_data[offset];
		control_data[offset] = data;

		switch (offset)
		{
			case 0x03:
				if (data != prev_control)
				{
	//				bprintf (0, _T("timer activated: %5.5x\n"), data&0x1ff);
					atarivad_scanline_timer = data & 0x1ff;
					atarivad_scanline_timer_enabled = 1;
				}
			break;

			case 0x0a:
				palette_bank = (~data & 0x0400) >> 10;
				playfield_latched = data & 0x80;
			break;

			case 0x10:
			case 0x11:
			case 0x12:
			case 0x13:
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
			case 0x18:
			case 0x19:
			case 0x1a:
			case 0x1b:
				update_parameter(data);
			break;

			case 0x1c:
			case 0x1d:
		//		bprintf (0, _T("write: %2.2x, %4.4x\n"), offset, data);
			break;

			case 0x1e:
				scanline_timer_callback(CPU_IRQSTATUS_NONE);
			break;
		}
		return;
	}

	if ((address & 0xfe000) == 0x10000) { // playfield2_latched_msb_w
		address = (address & 0x1ffe) / 2;
		pf_data[1][address] = data;
		if (playfield_latched) {
			pf_data[2][address] = (pf_data[2][address] & 0x00ff) | (control_data[0x1c] & 0xff00);
		}
		return;
	}

	if ((address & 0xfe000) == 0x12000) { // playfield_latched_lsb_w
		address = (address & 0x1ffe) / 2;
		pf_data[0][address] = data;
		if (playfield_latched) {
			pf_data[2][address] = (pf_data[2][address] & 0xff00) | (control_data[0x1d] & 0x00ff);
		}
		return;
	}

	if ((address & 0xfe000) == 0x14000) { // playfield_latched_msb_w
		address = (address & 0x1ffe) / 2;
		pf_data[0][address] = data;
		if (playfield_latched) {
			pf_data[2][address] = (pf_data[2][address] & 0x00ff) | (control_data[0x1c] & 0xff00);
		}
		return;
	}

	// relief pitcher goes nuts with unmapped writes.
	//bprintf (0, _T("VAD,WW: %5.5x, %4.4x\n"), address, data);
}

static UINT16 __fastcall atari_vad_read_word(UINT32 address)
{
	address &= 0x3fe;

	if (address == 0x03c0) {
		INT32 ret = atarivad_scanline;
		if (ret >= 255) ret = 255;
		if (atarivad_scanline >= nScreenHeight) ret |= 0x4000;
		return ret;
	}

	if (address >= 0x03c2) {
		return control_data[(address & 0x3e)/2];
	}

	bprintf (0, _T("VAD,RW: %5.5x\n"), address);

	return 0;
}

static void __fastcall atari_vad_write_byte(UINT32 address, UINT8 data)
{
	bprintf (0, _T("VAD,WB: %5.5x, %2.2x\n"), address & 0x1ffff, data);
}

static UINT8 __fastcall atari_vad_read_byte(UINT32 address)
{
	bprintf (0, _T("AtariVAD RB: %5.5x\n"), address);

	return 0;
}

INT32 AtariVADScan(INT32 nAction, INT32 *pnMin)
{
	if (nAction & ACB_VOLATILE)
	{
		ScanVar(playfield_data[0], 0x4000 * 3, "VAD Playfield");
		ScanVar(palette_ram, 0x1000, "VAD Palette");
		SCAN_VAR(playfield_latched);
		SCAN_VAR(palette_bank);
		SCAN_VAR(control_data);
		SCAN_VAR(pf_scrolly);
		SCAN_VAR(pf_scrollx);
		SCAN_VAR(mo_xscroll);
		SCAN_VAR(mo_yscroll);
	}

	if (nAction & ACB_WRITE) {
		AtariVADRecalcPalette();
	}

	return 0;
}

void AtariVADReset()
{
	memset (playfield_data[0], 0, 0x2000 * 3);

	playfield_latched = 0;
	palette_bank = 0;

	memset (control_data, 0, sizeof(control_data));

	atarivad_scanline_timer = 0;
	atarivad_scanline_timer_enabled = 0;

	tilerow_scanline = 0;
	tilerow_partial_prev_line = 0;

	pf_scrolly[0] = pf_scrolly[1] = 0;
	pf_scrollx[0] = pf_scrollx[1] = 0;

	mo_xscroll = 0;
	mo_yscroll = 0;
}

void AtariVADInit(INT32 tmap_num0, INT32 tmap_num1, INT32 bg_map_type, void (*sl_timer_cb)(INT32), void (*palette_write)(INT32 offset, UINT16 data))
{
	playfield_data[0] = (UINT8*)BurnMalloc(0x4000 * 3); // extra...
	playfield_data[1] = playfield_data[0] + 0x4000;
	playfield_data[2] = playfield_data[0] + 0x8000;

	pf_data[0] = (UINT16*)playfield_data[0];
	pf_data[1] = (UINT16*)playfield_data[1];
	pf_data[2] = (UINT16*)playfield_data[2];

	palette_ram = (UINT8*)BurnMalloc(0x1000);

	scanline_timer_callback = (sl_timer_cb) ? sl_timer_cb : scanline_timer_dummy;
	AtariVADPartialCB = NULL;

	GenericTilemapInit(tmap_num0, TILEMAP_SCAN_COLS, bg_map_type ? bg_map_callback : bg0_map_callback, 8, 8, 64, 64);
	GenericTilemapInit(tmap_num1, TILEMAP_SCAN_COLS, bg1_map_callback, 8, 8, 64, 64);
	GenericTilemapSetTransparent(tmap_num1, 0);

	playfield_number[0] = tmap_num0;
	playfield_number[1] = tmap_num1;

	atari_palette_write = palette_write ? palette_write : palette_write_dummy;
}

void AtariVADSetPartialCB(void (*partial_cb)(INT32))
{
	AtariVADPartialCB = partial_cb;
}

void AtariVADExit()
{
	BurnFree(playfield_data[0]);
	BurnFree(palette_ram);
}

void AtariVADMap(INT32 startaddress, INT32 endaddress, INT32 shuuz)
{
	INT32 range = (endaddress + 1) - startaddress;
	INT32 address = startaddress;

	SekMapHandler(5,					address, address + range - 1, MAP_WRITE);
	SekSetWriteWordHandler(5,			atari_vad_write_word);
	SekSetWriteByteHandler(5,			atari_vad_write_byte);

	SekMapHandler(6,					address + 0xfc00, address + 0x0ffff, MAP_READ);
	SekSetReadWordHandler(6,			atari_vad_read_word);
	SekSetReadByteHandler(6,			atari_vad_read_byte);

	SekMapMemory(palette_ram,			address + 0x00000, address + 0x00fff, MAP_ROM);

	if (shuuz) { // shuuz
		SekMapMemory(playfield_data[0],		address + 0x14000, address + 0x15fff, MAP_ROM);
		SekMapMemory(playfield_data[2],		address + 0x16000, address + 0x17fff, MAP_RAM);
	} else {
		SekMapMemory(playfield_data[1],		address + 0x10000, address + 0x11fff, MAP_ROM);
		SekMapMemory(playfield_data[0],		address + 0x12000, address + 0x13fff, MAP_ROM);
		SekMapMemory(playfield_data[2],		address + 0x14000, address + 0x15fff, MAP_RAM);
	}
}

// call on line 255
void AtariVADEOFUpdate(UINT16 *eof_data)
{
	for (INT32 i = 0; i < 0x1f; i++) {
		if (eof_data[i]) {
			atari_vad_write_word(0x0ffc0 + (i*2), eof_data[i]);
		}
	}

	tilerow_partial_prev_line = 0;
}

void AtariVADTimerUpdate()
{
	if (atarivad_scanline_timer_enabled) {
		if (atarivad_scanline_timer == atarivad_scanline) {
			scanline_timer_callback(CPU_IRQSTATUS_ACK);
		}
	}
}

void AtariVADTileRowUpdate(INT32 scanline, UINT16 *alphamap_ram)
{
	if (tilerow_scanline != scanline) return; // timer!

	if ((scanline < nScreenHeight) && (control_data[0x0a] & 0x2000))
	{
		INT32 offset = ((scanline / 8) * 64) + 48 + (2 * (scanline & 7));
		INT32 data0 = alphamap_ram[offset];
		INT32 data1 = alphamap_ram[offset+1];

		if (scanline > 0 && ((data0 | data1) & 0xf) && pBurnDraw)
		{
			if (tilerow_partial_prev_line > scanline) tilerow_partial_prev_line = 0;

			if (AtariVADPartialCB) AtariVADPartialCB(scanline);

			tilerow_partial_prev_line = scanline;
		}

		if (data0 & 0xf) update_parameter(data0);
		if (data1 & 0xf) update_parameter(data1);
	}

	scanline += (control_data[0x0a] & 0x2000) ? 1 : 8;
	if (scanline >= nScreenHeight) scanline = 0;
	tilerow_scanline = scanline;
}

void AtariVADDraw(UINT16 *pDestDraw, INT32 use_categories)
{
	atarimo_set_xscroll(0, mo_xscroll);
	atarimo_set_yscroll(0, mo_yscroll);

	GenericTilemapSetScrollX(playfield_number[0], pf_scrollx[0] + (pf_scrollx[1] & 7));
	GenericTilemapSetScrollY(playfield_number[0], pf_scrolly[0]);
	GenericTilemapSetScrollX(playfield_number[1], pf_scrollx[1] + 4);
	GenericTilemapSetScrollY(playfield_number[1], pf_scrolly[1]);

	if (use_categories)
	{
		if (nBurnLayer & 1) GenericTilemapDraw(playfield_number[0], pDestDraw, 0x00 | TMAP_FORCEOPAQUE);
		if (nBurnLayer & 1) GenericTilemapDraw(playfield_number[0], pDestDraw, 0x01 | TMAP_SET_GROUP(0x01));
		if (nBurnLayer & 1) GenericTilemapDraw(playfield_number[0], pDestDraw, 0x02 | TMAP_SET_GROUP(0x02));
		if (nBurnLayer & 1) GenericTilemapDraw(playfield_number[0], pDestDraw, 0x03 | TMAP_SET_GROUP(0x03));
		if (nBurnLayer & 2) GenericTilemapDraw(playfield_number[1], pDestDraw, 0x80 | TMAP_SET_GROUP(0x00));
		if (nBurnLayer & 2) GenericTilemapDraw(playfield_number[1], pDestDraw, 0x84 | TMAP_SET_GROUP(0x01));
		if (nBurnLayer & 2) GenericTilemapDraw(playfield_number[1], pDestDraw, 0x88 | TMAP_SET_GROUP(0x02));
		if (nBurnLayer & 2) GenericTilemapDraw(playfield_number[1], pDestDraw, 0x8c | TMAP_SET_GROUP(0x03));
	}
	else
	{
		if (nBurnLayer & 1) GenericTilemapDraw(playfield_number[0], pDestDraw, 0);
		if (nBurnLayer & 2) GenericTilemapDraw(playfield_number[1], pDestDraw, 1);
	}
}
